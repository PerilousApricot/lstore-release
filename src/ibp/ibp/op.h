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

/** \file
* Autogenerated public API
*/

#ifndef ACCRE_IBP_IBP_OP_H_INCLUDED
#define ACCRE_IBP_IBP_OP_H_INCLUDED

#include <ibp/visibility.h>
#include <ibp/types.h>
#include <tbx/iniparse.h>
#include <tbx/pigeon_coop.h>

#ifdef __cplusplus
extern "C" {
#endif

// Typedefs
typedef struct ibp_context_t ibp_context_t;
typedef struct ibp_op_alloc_t ibp_op_alloc_t;
typedef struct ibp_op_copy_t ibp_op_copy_t;
typedef struct ibp_op_depot_inq_t ibp_op_depot_inq_t;
typedef struct ibp_op_depot_modify_t ibp_op_depot_modify_t;
typedef struct ibp_op_get_chksum_t ibp_op_get_chksum_t;
typedef struct ibp_op_merge_alloc_t ibp_op_merge_alloc_t;
typedef struct ibp_op_modify_alloc_t ibp_op_modify_alloc_t;
typedef struct ibp_op_probe_t ibp_op_probe_t;
typedef struct ibp_op_rid_inq_t ibp_op_rid_inq_t;
typedef struct ibp_op_rw_t ibp_op_rw_t;
typedef struct ibp_op_t ibp_op_t;
typedef struct ibp_op_validate_chksum_t ibp_op_validate_chksum_t;
typedef struct ibp_op_version_t ibp_op_version_t;
typedef struct ibp_rw_buf_t ibp_rw_buf_t;

// Functions
IBP_API gop_op_generic_t *ibp_proxy_alloc_op(ibp_context_t *ic, ibp_capset_t *caps, ibp_cap_t *mcap, ibp_off_t offset, ibp_off_t size, int duration, int timeout);
IBP_API gop_op_generic_t *ibp_proxy_remove_op(ibp_context_t *ic, ibp_cap_t *cap, ibp_cap_t *mcap, int timeout);
IBP_API gop_op_generic_t *ibp_alloc_op(ibp_context_t *ic, ibp_capset_t *caps, ibp_off_t size, ibp_depot_t *depot, ibp_attributes_t *attr, int disk_cs_type, ibp_off_t disk_blocksize, int timeout);
IBP_API gop_op_generic_t *ibp_append_op(ibp_context_t *ic, ibp_cap_t *cap, tbx_tbuf_t *buffer, ibp_off_t boff, ibp_off_t len, int timeout);
IBP_API int ibp_cc_type(ibp_connect_context_t *cc);
IBP_API int ibp_chksum_set(ibp_context_t *ic, tbx_ns_chksum_t *ncs);
IBP_API int ibp_config_load(ibp_context_t *ic, tbx_inip_file_t *ifd, char *section);
IBP_API int ibp_config_load_file(ibp_context_t *ic, char *fname, char *section);
IBP_API ibp_context_t *ibp_context_create();
IBP_API void ibp_context_destroy(ibp_context_t *ic);
IBP_API gop_op_generic_t *ibp_copy_op(ibp_context_t *ic, int mode, int ns_type, char *path, ibp_cap_t *srccap, ibp_cap_t *destcap, ibp_off_t src_offset, ibp_off_t dest_offset, ibp_off_t size, int src_timeout, int dest_timeout, int dest_client_timeout);
IBP_API gop_op_generic_t *ibp_copyappend_op(ibp_context_t *ic, int ns_type, char *path, ibp_cap_t *srccap, ibp_cap_t *destcap, ibp_off_t src_offset, ibp_off_t size, int src_timeout, int  dest_timeout, int dest_client_timeout);
IBP_API gop_op_generic_t *ibp_depot_inq_op(ibp_context_t *ic, ibp_depot_t *depot, char *password, ibp_depotinfo_t *di, int timeout);
IBP_API void ibp_max_depot_threads_set(ibp_context_t *ic, int n);
IBP_API gop_op_generic_t *ibp_modify_alloc_op(ibp_context_t *ic, ibp_cap_t *cap, ibp_off_t size, int duration, int reliability, int timeout);
IBP_API gop_op_generic_t *ibp_modify_count_op(ibp_context_t *ic, ibp_cap_t *cap, int mode, int captype, int timeout);
IBP_API void ibp_op_cc_set(gop_op_generic_t *gop, ibp_connect_context_t *cc);
IBP_API void ibp_op_init(ibp_context_t *ic, ibp_op_t *op);
IBP_API void ibp_op_ncs_set(gop_op_generic_t *gop, tbx_ns_chksum_t *ncs);
IBP_API gop_op_generic_t *ibp_probe_op(ibp_context_t *ic, ibp_cap_t *cap, ibp_capstatus_t *probe, int timeout);
IBP_API gop_op_generic_t *ibp_query_resources_op(ibp_context_t *ic, ibp_depot_t *depot, ibp_ridlist_t *rlist, int timeout);
IBP_API void ibp_read_cc_set(ibp_context_t *ic, ibp_connect_context_t *cc);
IBP_API gop_op_generic_t *ibp_read_op(ibp_context_t *ic, ibp_cap_t *cap, ibp_off_t offset, tbx_tbuf_t *buffer, ibp_off_t boff, ibp_off_t len, int timeout);
IBP_API gop_op_generic_t *ibp_remove_op(ibp_context_t *ic, ibp_cap_t *cap, int timeout);
IBP_API gop_op_generic_t *ibp_rw_op(ibp_context_t *ic, int rw_type, ibp_cap_t *cap, ibp_off_t offset, tbx_tbuf_t *buffer, ibp_off_t boff, ibp_off_t len, int timeout);
IBP_API int ibp_sync_command(ibp_op_t *op);
IBP_API void ibp_tcpsize_set(ibp_context_t *ic, int n);
IBP_API gop_op_generic_t *ibp_truncate_op(ibp_context_t *ic, ibp_cap_t *cap, ibp_off_t size, int timeout);
IBP_API gop_op_generic_t *ibp_validate_chksum_op(ibp_context_t *ic, ibp_cap_t *mcap, int correct_errors, int *n_bad_blocks, int timeout);
IBP_API gop_op_generic_t *ibp_vec_read_op(ibp_context_t *ic, ibp_cap_t *cap, int n_vec, ibp_tbx_iovec_t *vec, tbx_tbuf_t *buffer, ibp_off_t boff, ibp_off_t len, int timeout);
IBP_API gop_op_generic_t *ibp_vec_write_op(ibp_context_t *ic, ibp_cap_t *cap, int n_iovec, ibp_tbx_iovec_t *iovec, tbx_tbuf_t *buffer, ibp_off_t boff, ibp_off_t len, int timeout);
IBP_API char *ibp_version();
IBP_API gop_op_generic_t *ibp_version_op(ibp_context_t *ic, ibp_depot_t *depot, char *buffer, int buffer_size, int timeout);
IBP_API void ibp_write_cc_set(ibp_context_t *ic, ibp_connect_context_t *cc);
IBP_API gop_op_generic_t *ibp_write_op(ibp_context_t *ic, ibp_cap_t *cap, ibp_off_t offset, tbx_tbuf_t *buffer, ibp_off_t boff, ibp_off_t len, int timeout);
IBP_API void ibp_set_sync_context(ibp_context_t *ic);

// Preprocessor constants
#define MAX_KEY_SIZE 256

// Preprocessor macros
#define ibp_get_iop(a) (a)->op->priv

// Exported types. To be obscured
struct ibp_op_validate_chksum_t {    //** IBP_VALIDATE_CHKSUM
    ibp_cap_t *cap;
    char       key[MAX_KEY_SIZE];
    char       typekey[MAX_KEY_SIZE];
    int correct_errors;
    int *n_bad_blocks;
};

struct ibp_op_get_chksum_t {   //** IBP_GET_CHKSUM
    ibp_cap_t *cap;
    char       key[MAX_KEY_SIZE];
    char       typekey[MAX_KEY_SIZE];
    int chksum_info_only;
    ibp_off_t bufsize;
    char *buffer;
    int *cs_type;
    int *cs_size;
    ibp_off_t *blocksize;
    ibp_off_t *nblocks;
    ibp_off_t *n_chksumbytes;
};

struct ibp_rw_buf_t {
    ibp_tbx_iovec_t *iovec;
    tbx_tbuf_t *buffer;
    ibp_off_t size;
    ibp_off_t boff;
    int n_iovec;
    ibp_tbx_iovec_t iovec_single;
};

struct ibp_op_rw_t {  //** Read/Write operation
    ibp_cap_t *cap;
    char       key[MAX_KEY_SIZE];
    char       typekey[MAX_KEY_SIZE];
    //   char *buf;
    //   ibp_off_t offset;
    //   ibp_off_t size;
    //   ibp_off_t boff;
    //   ibp_tbx_iovec_t *iovec;
    //   int   n_iovec;
    //   tbx_tbuf_t *buffer;
    int rw_mode;
    int n_ops;
    int n_tbx_iovec_total;
    ibp_off_t size;
    ibp_rw_buf_t **rwbuf;
    ibp_rw_buf_t *bs_ptr;
    tbx_pch_t rwcg_pch;
    ibp_rw_buf_t buf_single;
};

struct ibp_op_merge_alloc_t { //** MERGE allocoation op
    char mkey[MAX_KEY_SIZE];      //** Master key
    char mtypekey[MAX_KEY_SIZE];
    char ckey[MAX_KEY_SIZE];      //** Child key
    char ctypekey[MAX_KEY_SIZE];
};

struct ibp_op_alloc_t {  //**Allocate operation
    ibp_off_t size;
    ibp_off_t offset;                //** ibp_proxy_allocate
    int   duration;               //** ibp_proxy_allocate
    int   disk_chksum_type;            //** ibp_*ALLOCATE_CHKSUM
    ibp_off_t  disk_blocksize;          //** IBP_*ALLOCATE_CHKSUM
    char       key[MAX_KEY_SIZE];      //** ibp_rename/proxy_allocate
    char       typekey[MAX_KEY_SIZE];  //** ibp_rename/proxy_allocate
    ibp_cap_t *mcap;         //** This is just used for ibp_rename/ibp_split_allocate
    ibp_capset_t *caps;
    ibp_depot_t *depot;
    ibp_attributes_t *attr;
};

struct ibp_op_probe_t {  //** modify count and PROBE  operation
    int       cmd;    //** IBP_MANAGE or IBP_PROXY_MANAGE
    ibp_cap_t *cap;
    char       mkey[MAX_KEY_SIZE];     //** USed for PROXY_MANAGE
    char       mtypekey[MAX_KEY_SIZE]; //** USed for PROXY_MANAGE
    char       key[MAX_KEY_SIZE];
    char       typekey[MAX_KEY_SIZE];
    int        mode;
    int        captype;
    ibp_capstatus_t *probe;
    ibp_proxy_capstatus_t *proxy_probe;
};

struct ibp_op_modify_alloc_t {  //** modify Allocation operation
    ibp_cap_t *cap;
    char       mkey[MAX_KEY_SIZE];     //** USed for PROXY_MANAGE
    char       mtypekey[MAX_KEY_SIZE]; //** USed for PROXY_MANAGE
    char       key[MAX_KEY_SIZE];
    char       typekey[MAX_KEY_SIZE];
    ibp_off_t     offset;    //** IBP_PROXY_MANAGE
    ibp_off_t     size;
    int        duration;
    int        reliability;
};

struct ibp_op_copy_t {  //** depot depot copy operations
    char      *path;       //** Phoebus path or NULL for default
    ibp_cap_t *srccap;
    ibp_cap_t *destcap;
    char       src_key[MAX_KEY_SIZE];
    char       src_typekey[MAX_KEY_SIZE];
    ibp_off_t  src_offset;
    ibp_off_t  dest_offset;
    ibp_off_t  len;
    int        dest_timeout;
    int        dest_client_timeout;
    int        ibp_command;
    int        ctype;
};

struct ibp_op_depot_modify_t {  //** Modify a depot/RID settings
    ibp_depot_t *depot;
    char *password;
    ibp_off_t max_hard;
    ibp_off_t max_soft;
    apr_time_t max_duration;
};

struct ibp_op_depot_inq_t {  //** Modify a depot/RID settings
    ibp_depot_t *depot;
    char *password;
    ibp_depotinfo_t *di;
};

struct ibp_op_version_t {  //** Get the depot version information
    ibp_depot_t *depot;
    char *buffer;
    int buffer_size;
};

struct ibp_op_rid_inq_t {  //** Get a list of RID's for a depot
    ibp_depot_t *depot;
    ibp_ridlist_t *rlist;
};

struct ibp_op_t { //** Individual IO operation
    ibp_context_t *ic;
    gop_op_generic_t gop;
    gop_op_data_t dop;
    tbx_stack_t *hp_parent;  //** Only used for RW coalescing
    int primary_cmd;//** Primary sync IBP command family
    int sub_cmd;    //** sub command, if applicable
    tbx_ns_chksum_t ncs;  //** chksum associated with the command
    union {         //** Holds the individual commands options
        ibp_op_validate_chksum_t validate_op;
        ibp_op_get_chksum_t      get_chksum_op;
        ibp_op_alloc_t  alloc_op;
        ibp_op_merge_alloc_t  merge_op;
        ibp_op_probe_t  probe_op;
        ibp_op_rw_t     rw_op;
        ibp_op_copy_t   copy_op;
        ibp_op_depot_modify_t depot_modify_op;
        ibp_op_depot_inq_t depot_inq_op;
        ibp_op_modify_alloc_t mod_alloc_op;
        ibp_op_rid_inq_t   rid_op;
        ibp_op_version_t   ver_op;
    } ops;
};
#ifdef __cplusplus
}
#endif

#endif /* ^ ACCRE_IBP_IBP_OP_H_INCLUDED ^ */ 
