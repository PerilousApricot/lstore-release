/*
   Copyright 2016 Vanderbilt University

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

//***********************************************************************
// Generic LIO functionality
//***********************************************************************
#include <gop/mq.h>
#include <sys/stat.h>
#include <tbx/log.h>

#include "blacklist.h"
#include "exnode.h"
#include "lio/lio_visibility.h"

#ifndef _LIO_ABSTRACT_H_
#define _LIO_ABSTRACT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LIO_FSCK_FINISHED           -1
#define LIO_FSCK_GOOD                0
#define LIO_FSCK_MISSING_OWNER       1
#define LIO_FSCK_MISSING_EXNODE      2
#define LIO_FSCK_MISSING_EXNODE_SIZE 4
#define LIO_FSCK_MISSING_INODE       8
#define LIO_FSCK_MISSING            16

#define LIO_FSCK_MANUAL      0
#define LIO_FSCK_PARENT      1
#define LIO_FSCK_DELETE      2
#define LIO_FSCK_USER        4
#define LIO_FSCK_SIZE_REPAIR 8

extern char *_lio_stat_keys[];
#define  _lio_stat_key_size 7

typedef struct lio_config_t lio_config_t;
typedef struct lio_fn_t lio_fn_t;
typedef struct lio_fsck_iter_t lio_fsck_iter_t;

LIO_API extern FILE *_lio_ifd;  //** Default information log device
extern char *_lio_exe_name;  //** Executable name

struct lio_config_t {
    data_service_fn_t *ds;
    object_service_fn_t *os;
    resource_service_fn_t *rs;
    thread_pool_context_t *tpc_unlimited;
    thread_pool_context_t *tpc_cache;
    mq_context_t *mqc;
    service_manager_t *ess;
    service_manager_t *ess_nocache;  //** Copy of ess but missing cache.  Kind of a kludge...
    tbx_stack_t *plugin_stack;
    cache_t *cache;
    data_attr_t *da;
    tbx_inip_file_t *ifd;
    tbx_list_t *open_index;
    creds_t *creds;
    apr_thread_mutex_t *lock;
    apr_pool_t *mpool;
    char *cfg_name;
    char *section_name;
    char *ds_section;
    char *mq_section;
    char *os_section;
    char *rs_section;
    char *tpc_unlimited_section;
    char *tpc_cache_section;
    char *creds_name;
    char *exe_name;
    blacklist_t *blacklist;
    ex_off_t readahead;
    ex_off_t readahead_trigger;
    int calc_adler32;
    int timeout;
    int max_attr;
    int anonymous_creation;
    int auto_translate;
    int ref_cnt;
};

typedef struct lio_path_tuple_t lio_path_tuple_t;
struct lio_path_tuple_t {
    creds_t *creds;
    lio_config_t *lc;
    char *path;
    int is_lio;
};

typedef struct unified_object_iter_t unified_object_iter_t;
struct unified_object_iter_t {
    lio_path_tuple_t tuple;
    os_object_iter_t *oit;
    local_object_iter_t *lit;
};

typedef struct lio_cp_file_t lio_cp_file_t;
struct lio_cp_file_t {
    segment_rw_hints_t *rw_hints;
    lio_path_tuple_t src_tuple;
    lio_path_tuple_t dest_tuple;
    ex_off_t bufsize;
    int slow;
};

typedef struct lio_cp_path_t lio_cp_path_t;
struct lio_cp_path_t {
    lio_path_tuple_t src_tuple;
    lio_path_tuple_t dest_tuple;
    os_regex_table_t *path_regex;
    os_regex_table_t *obj_regex;
    int recurse_depth;
    int dest_type;
    int obj_types;
    int max_spawn;
    int slow;
    ex_off_t bufsize;
};

LIO_API extern lio_config_t *lio_gc;
LIO_API extern tbx_log_fd_t *lio_ifd;
LIO_API extern int lio_parallel_task_count;

#define LIO_READ_MODE      1
#define LIO_WRITE_MODE     2
#define LIO_TRUNCATE_MODE  4
#define LIO_CREATE_MODE    8
#define LIO_APPEND_MODE   16
#define LIO_RW_MODE        (LIO_READ_MODE|LIO_WRITE_MODE)

#define LIO_COPY_DIRECT   0
#define LIO_COPY_INDIRECT 1

#define lio_lock(s) apr_thread_mutex_lock((s)->lock)
#define lio_unlock(s) apr_thread_mutex_unlock((s)->lock)

struct lio_file_handle_t {  //** Shared file handle
    exnode_t *ex;
    segment_t *seg;
    lio_config_t *lc;
    ex_id_t vid;
    int ref_count;
    int remove_on_close;
    ex_off_t readahead_end;
    tbx_atomic_unit32_t modified;
    tbx_list_t *write_table;
};

typedef struct lio_file_handle_t lio_file_handle_t;

typedef struct lio_fd_t lio_fd_t;
struct lio_fd_t {  //** Individual file descriptor
    lio_config_t *lc;
    lio_file_handle_t *fh;  //** Shared handle
    creds_t *creds;
    char *path;
    int mode;         //** R/W mode
    ex_off_t curr_offset;
};

extern tbx_sl_compare_t ex_id_compare;

LIO_API op_generic_t *lio_exists_op(lio_config_t *lc, creds_t *creds, char *path);
LIO_API int lio_exists(lio_config_t *lc, creds_t *creds, char *path);
LIO_API op_generic_t *lio_create_op(lio_config_t *lc, creds_t *creds, char *path, int type, char *ex, char *id);
LIO_API op_generic_t *lio_remove_op(lio_config_t *lc, creds_t *creds, char *path, char *ex_optional, int ftype_optional);
LIO_API op_generic_t *lio_remove_regex_op(lio_config_t *lc, creds_t *creds, os_regex_table_t *path, os_regex_table_t *object_regex, int obj_types, int recurse_depth, int np);

LIO_API op_generic_t *lio_regex_object_set_multiple_attrs_op(lio_config_t *lc, creds_t *creds, char *id, os_regex_table_t *path, os_regex_table_t *object_regex, int object_types, int recurse_depth, char **key, void **val, int *v_size, int n);
op_generic_t *gop_lio_abort_regex_object_set_multiple_attrs(lio_config_t *lc, op_generic_t *gop);
LIO_API op_generic_t *lio_move_op(lio_config_t *lc, creds_t *creds, char *src_path, char *dest_path);
op_generic_t *gop_lio_symlink_object(lio_config_t *lc, creds_t *creds, char *src_path, char *dest_path, char *id);
LIO_API op_generic_t *lio_hardlink_op(lio_config_t *lc, creds_t *creds, char *src_path, char *dest_path, char *id);
LIO_API op_generic_t *lio_link_op(lio_config_t *lc, creds_t *creds, int symlink, char *src_path, char *dest_path, char *id);

LIO_API os_object_iter_t *lio_create_object_iter(lio_config_t *lc, creds_t *creds, os_regex_table_t *path, os_regex_table_t *obj_regex, int object_types, os_regex_table_t *attr, int recurse_dpeth, os_attr_iter_t **it, int v_max);
LIO_API os_object_iter_t *lio_create_object_iter_alist(lio_config_t *lc, creds_t *creds, os_regex_table_t *path, os_regex_table_t *obj_regex, int object_types, int recurse_depth, char **key, void **val, int *v_size, int n_keys);
LIO_API int lio_next_object(lio_config_t *lc, os_object_iter_t *it, char **fname, int *prefix_len);
LIO_API void lio_destroy_object_iter(lio_config_t *lc, os_object_iter_t *it);

LIO_API int lio_fopen_flags(char *sflags);
LIO_API op_generic_t *lio_open_op(lio_config_t *lc, creds_t *creds, char *path, int mode, char *id, lio_fd_t **fd, int max_wait);
LIO_API op_generic_t *lio_close_op(lio_fd_t *fd);
//NOT NEEDED NOW???? op_generic_t *gop_lio_abort_open_object(lio_config_t *lc, op_generic_t *gop);

op_generic_t *gop_lio_read(lio_fd_t *fd, char *buf, ex_off_t size, ex_off_t off, segment_rw_hints_t *rw_hints);
op_generic_t *gop_lio_readv(lio_fd_t *fd, tbx_iovec_t *iov, int n_iov, ex_off_t size, ex_off_t off, segment_rw_hints_t *rw_hints);
op_generic_t *gop_lio_read_ex(lio_fd_t *fd, int n_iov, ex_tbx_iovec_t *iov, tbx_tbuf_t *buffer, ex_off_t boff, segment_rw_hints_t *rw_hints);
op_generic_t *gop_lio_write(lio_fd_t *fd, char *buf, ex_off_t size, off_t off, segment_rw_hints_t *rw_hints);
op_generic_t *gop_lio_writev(lio_fd_t *fd, tbx_iovec_t *iov, int n_iov, ex_off_t size, off_t off, segment_rw_hints_t *rw_hints);
op_generic_t *gop_lio_write_ex(lio_fd_t *fd, int n_iov, ex_tbx_iovec_t *iov, tbx_tbuf_t *buffer, ex_off_t boff, segment_rw_hints_t *rw_hints);

int lio_read(lio_fd_t *fd, char *buf, ex_off_t size, off_t off, segment_rw_hints_t *rw_hints);
int lio_readv(lio_fd_t *fd, tbx_iovec_t *iov, int n_iov, ex_off_t size, off_t off, segment_rw_hints_t *rw_hints);
int lio_read_ex(lio_fd_t *fd, int n_iov, ex_tbx_iovec_t *iov, tbx_tbuf_t *buffer, ex_off_t boff, segment_rw_hints_t *rw_hints);
int lio_write(lio_fd_t *fd, char *buf, ex_off_t size, off_t off, segment_rw_hints_t *rw_hints);
int lio_writev(lio_fd_t *fd, tbx_iovec_t *iov, int n_iov, ex_off_t size, off_t off, segment_rw_hints_t *rw_hints);
int lio_write_ex(lio_fd_t *fd, int n_iov, ex_tbx_iovec_t *iov, tbx_tbuf_t *buffer, ex_off_t boff, segment_rw_hints_t *rw_hints);

mode_t ftype_lio2posix(int ftype);
void _lio_parse_stat_vals(char *fname, struct stat *stat, char **val, int *v_size, char *mount_prefix, char **flink);
int lio_stat(lio_config_t *lc, creds_t *creds, char *fname, struct stat *stat, char *mount_prefix, char **readlink);

ex_off_t lio_seek(lio_fd_t *fd, ex_off_t offset, int whence);
ex_off_t lio_tell(lio_fd_t *fd);
ex_off_t lio_size(lio_fd_t *fd);
op_generic_t *gop_lio_truncate(lio_fd_t *fd, ex_off_t new_size);
// NOT IMPLEMENTED op_generic_t *gop_lio_stat(lio_t *lc, const char *fname, struct stat *stat);

LIO_API op_generic_t *lio_cp_local2lio_op(FILE *sfd, lio_fd_t *dfd, ex_off_t bufsize, char *buffer, segment_rw_hints_t *rw_hints);
LIO_API op_generic_t *lio_cp_lio2local_op(lio_fd_t *sfd, FILE *dfd, ex_off_t bufsize, char *buffer, segment_rw_hints_t *rw_hints);
op_generic_t *gop_lio_cp_lio2lio(lio_fd_t *sfd, lio_fd_t *dfd, ex_off_t bufsize, char *buffer, int hints, segment_rw_hints_t *rw_hints);

//op_generic_t *gop_lio_symlink_attr(lio_config_t *lc, creds_t *creds, char *src_path, char *key_src, const char *path_dest, char *key_dest);
//op_generic_t *gop_lio_symlink_multiple_attrs(lio_config_t *lc, creds_t *creds, char **src_path, char **key_src, const char *path_dest, char **key_dest, int n);

LIO_API op_generic_t *lio_getattr_op(lio_config_t *lc, creds_t *creds, const char *path, char *id, char *key, void **val, int *v_size);
LIO_API op_generic_t *lio_setattr_op(lio_config_t *lc, creds_t *creds, const char *path, char *id, char *key, void *val, int v_size);
//op_generic_t *gop_lio_move_attr(lio_config_t *lc, creds_t *creds, const char *path, char *id, char *key_old, char *key_new);
//op_generic_t *gop_lio_copy_attr(lio_config_t *lc, creds_t *creds, const char *path_src, char *id, char *key_src, const char *path_dest, char *key_dest);
op_generic_t *gop_lio_get_multiple_attrs(lio_config_t *lc, creds_t *creds, const char *path, char *id, char **key, void **val, int *v_size, int n);
op_generic_t *gop_lio_multiple_setattr_op(lio_config_t *lc, creds_t *creds, const char *path, char *id, char **key, void **val, int *v_size, int n);
//op_generic_t *gop_lio_move_multiple_attrs(lio_config_t *lc, creds_t *creds, const char *char *id, path, char **key_old, char **key_new, int n);
//op_generic_t *gop_lio_copy_multiple_attrs(lio_config_t *lc, creds_t *creds, const char *path_src, char *id, char **key_src, const char *path_dest, char **key_dest, int n);
LIO_API int lio_getattr(lio_config_t *lc, creds_t *creds, const char *path, char *id, char *key, void **val, int *v_size);
LIO_API int lio_setattr(lio_config_t *lc, creds_t *creds, const char *path, char *id, char *key, void *val, int v_size);
int lio_get_multiple_attrs(lio_config_t *lc, creds_t *creds, const char *path, char *id, char **key, void **val, int *v_size, int n);
LIO_API int lio_multiple_setattr_op(lio_config_t *lc, creds_t *creds, const char *path, char *id, char **key, void **val, int *v_size, int n);

os_attr_iter_t *lio_create_attr_iter(lio_config_t *lc, creds_t *creds, const char *path, os_regex_table_t *attr, int v_max);
LIO_API int lio_next_attr(lio_config_t *lc, os_attr_iter_t *it, char **key, void **val, int *v_size);
void lio_destroy_attr_iter(lio_config_t *lc, os_attr_iter_t *it);

LIO_API int lio_encode_error_counts(segment_errors_t *serr, char **key, char **val, char *buf, int *v_size, int mode);
LIO_API void lio_get_error_counts(lio_config_t *lc, segment_t *seg, segment_errors_t *serr);
int lio_update_error_counts(lio_config_t *lc, creds_t *creds, char *path, segment_t *seg, int mode);
int lio_update_exnode_attrs(lio_config_t *lc, creds_t *creds, exnode_t *ex, segment_t *seg, char *fname, segment_errors_t *serr);

LIO_API int lio_next_fsck(lio_config_t *lc, lio_fsck_iter_t *oit, char **bad_fname, int *bad_atype);
LIO_API lio_fsck_iter_t *lio_create_fsck_iter(lio_config_t *lc, creds_t *creds, char *path, int owner_mode, char *owner, int exnode_mode);
LIO_API void lio_destroy_fsck_iter(lio_config_t *lc, lio_fsck_iter_t *oit);
LIO_API ex_off_t lio_fsck_visited_count(lio_config_t *lc, lio_fsck_iter_t *oit);
LIO_API op_generic_t *lio_fsck_op(lio_config_t *lc, creds_t *creds, char *fname, int ftype, int owner_mode, char *owner, int exnode_mode);


//-----
op_generic_t *lioc_create_object(lio_config_t *lc, creds_t *creds, char *path, int type, char *ex, char *id);
op_generic_t *lioc_remove_object(lio_config_t *lc, creds_t *creds, char *path, char *ex_optional, int ftype_optional);
op_generic_t *lioc_remove_regex_object(lio_config_t *lc, creds_t *creds, os_regex_table_t *rpath, os_regex_table_t *robj, int obj_types, int recurse_depth, int np);


void lio_set_timestamp(char *id, char **val, int *v_size);
void lio_get_timestamp(char *val, int *timestamp, char **id);
LIO_API int lioc_exists(lio_config_t *lc, creds_t *creds, char *path);
int lioc_set_multiple_attrs(lio_config_t *lc, creds_t *creds, char *path, char *id, char **key, void **val, int *v_size, int n);
LIO_API int lioc_setattr(lio_config_t *lc, creds_t *creds, char *path, char *id, char *key, void *val, int v_size);
int lioc_get_multiple_attrs(lio_config_t *lc, creds_t *creds, char *path, char *id, char **key, void **val, int *v_size, int n_keys);
LIO_API int lioc_getattr(lio_config_t *lc, creds_t *creds, char *path, char *id, char *key, void **val, int *v_size);
int lioc_encode_error_counts(segment_errors_t *serr, char **key, char **val, char *buf, int *v_size, int mode);
void lioc_get_error_counts(lio_config_t *lc, segment_t *seg, segment_errors_t *serr);
int lioc_update_error_counts(lio_config_t *lc, creds_t *creds, char *path, segment_t *seg, int mode);
int lioc_update_exnode_attrs(lio_config_t *lc, creds_t *creds, exnode_t *ex, segment_t *seg, char *fname, segment_errors_t *serr);
op_generic_t *lioc_remove_object(lio_config_t *lc, creds_t *creds, char *path, char *ex_optional, int ftype_optional);
LIO_API unified_object_iter_t *lio_unified_object_iter_create(lio_path_tuple_t tuple, os_regex_table_t *path_regex, os_regex_table_t *obj_regex, int obj_types, int rd);
LIO_API void lio_unified_object_iter_destroy(unified_object_iter_t *it);
LIO_API int lio_unified_next_object(unified_object_iter_t *it, char **fname, int *prefix_len);
op_status_t cp_lio2lio(lio_cp_file_t *cp);
op_status_t cp_local2lio(lio_cp_file_t *cp);
op_status_t cp_lio2local(lio_cp_file_t *cp);
LIO_API op_status_t lio_file_copy_op(void *arg, int id);
int lio_cp_create_dir(tbx_list_t *table, lio_path_tuple_t tuple);
LIO_API op_status_t lio_path_copy_op(void *arg, int id);
LIO_API op_generic_t *lioc_truncate(lio_path_tuple_t *tuple, ex_off_t new_size);

void lc_object_remove_unused(int remove_all_unused);
LIO_API void lio_path_release(lio_path_tuple_t *tuple);
LIO_API void lio_path_local_make_absolute(lio_path_tuple_t *tuple);
LIO_API int lio_path_wildcard_auto_append(lio_path_tuple_t *tuple);
lio_path_tuple_t lio_path_auto_fuse_convert(lio_path_tuple_t *tuple);
LIO_API lio_path_tuple_t lio_path_resolve_base(char *startpath);
LIO_API lio_path_tuple_t lio_path_resolve(int auto_fuse_convert, char *startpath);
int lio_parse_path(char *startpath, char **user, char **service, char **path);
lio_fn_t *lio_core_create();
void lio_core_destroy(lio_config_t *lio);
LIO_API void lio_print_path_options(FILE *fd);
LIO_API int lio_parse_path_options(int *argc, char **argv, int auto_mode, lio_path_tuple_t *tuple, os_regex_table_t **rp, os_regex_table_t **ro);
LIO_API void lio_print_options(FILE *fd);
LIO_API int lio_init(int *argc, char ***argv);
LIO_API int lio_shutdown();
lio_config_t *lio_create(char *fname, char *section, char *user, char *exe_name);
void lio_destroy(lio_config_t *lio);
const char *lio_client_version();


#ifdef __cplusplus
}
#endif

#endif

