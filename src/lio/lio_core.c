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

#define _log_module_index 189

#include <gop/gop.h>
#include <gop/mq.h>
#include <gop/opque.h>
#include <gop/tp.h>
#include <gop/types.h>
#include <lio/segment.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tbx/fmttypes.h>
#include <tbx/log.h>
#include <tbx/string_token.h>
#include <tbx/type_malloc.h>
#include <unistd.h>

#include "authn.h"
#include "blacklist.h"
#include "cache.h"
#include "ex3.h"
#include "ex3/types.h"
#include "lio.h"
#include "os.h"
#include "os/file.h"

//***********************************************************************
// Core LIO functionality
//***********************************************************************


#define  _n_lioc_file_keys 7
#define  _n_lioc_dir_keys 6
#define _n_lioc_create_keys 7

static char *_lioc_create_keys[] = { "system.owner", "os.timestamp.system.create", "os.timestamp.system.modify_data", "os.timestamp.system.modify_attr", "system.inode", "system.exnode", "system.exnode.size"};

typedef struct {
    lio_config_t *lc;
    lio_creds_t *creds;
    char *src_path;
    char *dest_path;
    char *id;
    char *ex;
    int type;
} lioc_mk_mv_rm_t;

typedef struct {
    lio_config_t *lc;
    lio_creds_t *creds;
    lio_os_regex_table_t *rpath;
    lio_os_regex_table_t *robj;
    int recurse_depth;
    int obj_types;
    int np;
} lioc_remove_regex_t;

typedef struct {
    lio_path_tuple_t tuple;
    ex_off_t new_size;
} lioc_trunc_t;

//***********************************************************************
// lioc_free_mk_mv_rm
//***********************************************************************

void lioc_free_mk_mv_rm(void *arg)
{
    lioc_mk_mv_rm_t *op = (lioc_mk_mv_rm_t *)arg;

    if (op->src_path != NULL) free(op->src_path);
    if (op->dest_path != NULL) free(op->dest_path);
    if (op->id != NULL) free(op->id);
    if (op->ex != NULL) free(op->ex);

    free(op);
}

//***********************************************************************
// lioc_exists - Returns the filetype of the object or 0 if it
//   doesn't exist
//***********************************************************************

gop_op_generic_t *gop_lioc_exists(lio_config_t *lc, lio_creds_t *creds, char *path)
{
    return(os_exists(lc->os, creds, path));
}

//***********************************************************************
// lioc_exists - Returns the filetype of the object or 0 if it
//   doesn't exist
//***********************************************************************

int lioc_exists(lio_config_t *lc, lio_creds_t *creds, char *path)
{
    gop_op_status_t status;

    status = gop_sync_exec_status(os_exists(lc->os, creds, path));
    return(status.error_code);
}

//***********************************************************************
//  lio_parse_path - Parses a path ofthe form: user@service:/my/path
//        The user and service are optional
//
//  Returns 1 if @: are encountered and 0 otherwise
//***********************************************************************

int lio_parse_path(char *startpath, char **user, char **service, char **path)
{
    int i, j, found, n, ptype;

    *user = *service = *path = NULL;
    n = strlen(startpath);
    ptype = 0;
    found = -1;
    for (i=0; i<n; i++) {
        if (startpath[i] == '@') {
            found = i;
            ptype = 1;
            break;
        }
    }

    if (found == -1) {
        *path = strdup(startpath);
        return(ptype);
    }

    if (found > 0) { //** Got a valid user
        *user = strndup(startpath, found);
    }

    j = found+1;
    found = -1;
    for (i=j; i<n; i++) {
        if (startpath[i] == ':') {
            found = i;
            break;
        }
    }

    if (found == -1) {  //**No path.  Just a service
        if (j < n) {
            *service = strdup(&(startpath[j]));
        }
        return(ptype);
    }

    i = found - j;
    *service = (i == 0) ? NULL : strndup(&(startpath[j]), i);

    //** Everything else is the path
    j = found + 1;
    if (found < n) {
        *path = strdup(&(startpath[j]));
    }

    return(ptype);
}

//***********************************************************************
// lio_set_timestamp - Sets the timestamp val/size for a attr put
//***********************************************************************

void lio_set_timestamp(char *id, char **val, int *v_size)
{
    *val = id;
    *v_size = (id == NULL) ? 0 : strlen(id);
    return;
}

//***********************************************************************
// lio_get_timestamp - Splits the timestamp ts/id field
//***********************************************************************

void lio_get_timestamp(char *val, int *timestamp, char **id)
{
    char *bstate;
    int fin;

    *timestamp = 0;
    sscanf(tbx_stk_string_token(val, "|", &bstate, &fin), "%d", timestamp);
    if (id != NULL) *id = tbx_stk_string_token(NULL, "|", &bstate, &fin);
    return;
}

//***********************************************************************
// lioc_get_multiple_attrs - Returns Multiple attributes
//***********************************************************************

int lioc_get_multiple_attrs(lio_config_t *lc, lio_creds_t *creds, char *path, char *id, char **key, void **val, int *v_size, int n_keys)
{
    int err, serr;
    os_fd_t *fd;

    err = gop_sync_exec(os_open_object(lc->os, creds, path, OS_MODE_READ_IMMEDIATE, id, &fd, lc->timeout));
    if (err != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR opening object=%s\n", path);
        return(err);
    }

    //** IF the attribute doesn't exist *val == NULL an *v_size = 0
    serr = gop_sync_exec(os_get_multiple_attrs(lc->os, creds, fd, key, val, v_size, n_keys));

    //** Close the parent
    err = gop_sync_exec(os_close_object(lc->os, fd));
    if (err != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR closing object=%s\n", path);
    }

    if (serr != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR getting attributes object=%s\n", path);
        err = OP_STATE_FAILURE;
    }

    return(err);
}

//***********************************************************************
// lioc_getattr - Returns an attribute
//***********************************************************************

int lioc_getattr(lio_config_t *lc, lio_creds_t *creds, char *path, char *id, char *key, void **val, int *v_size)
{
    int err, serr;
    os_fd_t *fd;

    err = gop_sync_exec(os_open_object(lc->os, creds, path, OS_MODE_READ_IMMEDIATE, id, &fd, lc->timeout));
    if (err != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR opening object=%s\n", path);
        return(err);
    }

    //** IF the attribute doesn't exist *val == NULL an *v_size = 0
    serr = gop_sync_exec(os_get_attr(lc->os, creds, fd, key, val, v_size));

    //** Close the parent
    err = gop_sync_exec(os_close_object(lc->os, fd));
    if (err != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR closing object=%s\n", path);
    }

    if (serr != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR getting attribute object=%s\n", path);
        err = OP_STATE_FAILURE;
    }

    return(err);
}

//***********************************************************************
// lioc_set_multiple_attrs_real - Returns an attribute
//***********************************************************************

int lioc_set_multiple_attrs_real(lio_config_t *lc, lio_creds_t *creds, char *path, char *id, char **key, void **val, int *v_size, int n)
{
    int err, serr;
    os_fd_t *fd;

    err = gop_sync_exec(os_open_object(lc->os, creds, path, OS_MODE_READ_IMMEDIATE, id, &fd, lc->timeout));
    if (err != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR opening object=%s\n", path);
        return(err);
    }

    serr = gop_sync_exec(os_set_multiple_attrs(lc->os, creds, fd, key, val, v_size, n));

    //** Close the parent
    err = gop_sync_exec(os_close_object(lc->os, fd));
    if (err != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR closing object=%s\n", path);
    }

    if (serr != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR setting attributes object=%s\n", path);
        err = OP_STATE_FAILURE;
    }

    return(err);
}

//***********************************************************************
// lioc_set_multiple_attrs - Returns an attribute
//***********************************************************************

int lioc_set_multiple_attrs(lio_config_t *lc, lio_creds_t *creds, char *path, char *id, char **key, void **val, int *v_size, int n)
{
    int err;

    err = lioc_set_multiple_attrs_real(lc, creds, path, id, key, val, v_size, n);
    if (err != OP_STATE_SUCCESS) {  //** Got an error
        sleep(1);  //** Wait a bit before retrying
        err = lioc_set_multiple_attrs_real(lc, creds, path, id, key, val, v_size, n);
    }

    return(err);
}

//***********************************************************************
// lioc_setattr_real - Sets an attribute
//***********************************************************************

int lioc_setattr_real(lio_config_t *lc, lio_creds_t *creds, char *path, char *id, char *key, void *val, int v_size)
{
    int err, serr;
    os_fd_t *fd;

    err = gop_sync_exec(os_open_object(lc->os, creds, path, OS_MODE_READ_IMMEDIATE, id, &fd, lc->timeout));
    if (err != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR opening object=%s\n", path);
        return(err);
    }

    serr = gop_sync_exec(os_set_attr(lc->os, creds, fd, key, val, v_size));

    //** Close the parent
    err = gop_sync_exec(os_close_object(lc->os, fd));
    if (err != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR closing object=%s\n", path);
    }

    if (serr != OP_STATE_SUCCESS) {
        log_printf(1, "ERROR setting attribute object=%s\n", path);
        err = OP_STATE_FAILURE;
    }

    return(err);
}

//***********************************************************************
// lioc_setattr - Sets a single attribute
//***********************************************************************

int lioc_setattr(lio_config_t *lc, lio_creds_t *creds, char *path, char *id, char *key, void *val, int v_size)
{
    int err;

    err = lioc_setattr_real(lc, creds, path, id, key, val, v_size);
    if (err != OP_STATE_SUCCESS) {  //** Got an error
        sleep(1);  //** Wait a bit before retrying
        err = lioc_setattr_real(lc, creds, path, id, key, val, v_size);
    }

    return(err);
}

//***********************************************************************
// lioc_encode_error_counts - Encodes the error counts for a setattr call
//
//  The keys, val, and v_size arrays should have 3 elements. Buf is used
//  to store the error numbers.  It's assumed to have at least 3*32 bytes.
//  mode is used to determine how to handle 0 error values
//  (-1=remove attr, 0=no update, 1=store 0 value).
//  On return the number of attributes stored is returned.
//***********************************************************************

int lioc_encode_error_counts(lio_segment_errors_t *serr, char **key, char **val, char *buf, int *v_size, int mode)
{
    char *ekeys[] = { "system.hard_errors", "system.soft_errors",  "system.write_errors" };
    int err[3];
    int i, n, k;

    k = n = 0;

    //** So I can do this in a loop
    err[0] = serr->hard;
    err[1] = serr->soft;
    err[2] = serr->write;

    for (i=0; i<3; i++) {
        if ((err[i] != 0) || (mode == 1)) {  //** Always store
            val[n] = &(buf[k]);
            k += snprintf(val[n], 32, "%d", err[i]) + 1;
            v_size[n] = strlen(val[n]);
            key[n] = ekeys[i];
            n++;
        } else if (mode == -1) { //** Remove the attribute
            val[n] = NULL;
            v_size[n] = -1;
            key[n] = ekeys[i];
            n++;
        }
    }

    return(n);
}

//***********************************************************************
// lioc_get_error_counts - Gets the error counts
//***********************************************************************

void lioc_get_error_counts(lio_config_t *lc, lio_segment_t *seg, lio_segment_errors_t *serr)
{
    gop_op_generic_t *gop;
    gop_op_status_t status;

    gop = segment_inspect(seg, lc->da, lio_ifd, INSPECT_HARD_ERRORS, 0, NULL, 1);
    gop_waitall(gop);
    status = gop_get_status(gop);
    gop_free(gop, OP_DESTROY);
    serr->hard = status.error_code;

    gop = segment_inspect(seg, lc->da, lio_ifd, INSPECT_SOFT_ERRORS, 0, NULL, 1);
    gop_waitall(gop);
    status = gop_get_status(gop);
    gop_free(gop, OP_DESTROY);
    serr->soft = status.error_code;

    gop = segment_inspect(seg, lc->da, lio_ifd, INSPECT_WRITE_ERRORS, 0, NULL, 1);
    gop_waitall(gop);
    status = gop_get_status(gop);
    gop_free(gop, OP_DESTROY);
    serr->write = status.error_code;

    return;
}

//***********************************************************************
// lioc_update_error_count - Updates the error count attributes if needed
//***********************************************************************

int lioc_update_error_counts(lio_config_t *lc, lio_creds_t *creds, char *path, lio_segment_t *seg, int mode)
{
    char *keys[3];
    char *val[3];
    char buf[128];
    int v_size[3];
    int n;
    lio_segment_errors_t serr;

    lioc_get_error_counts(lc, seg, &serr);
    n = lioc_encode_error_counts(&serr, keys, val, buf, v_size, mode);
    if (n > 0) {
        lioc_set_multiple_attrs(lc, creds, path, NULL, keys, (void **)val, v_size, n);
    }

    return(serr.hard);
}

//***********************************************************************
// lioc_update_exnode_attrs - Updates the exnode and system.error_* attributes
//***********************************************************************

int lioc_update_exnode_attrs(lio_config_t *lc, lio_creds_t *creds, lio_exnode_t *ex, lio_segment_t *seg, char *fname, lio_segment_errors_t *serr)
{
    ex_off_t ssize;
    char buffer[32];
    char *key[6] = {"system.exnode", "system.exnode.size", "os.timestamp.system.modify_data", NULL, NULL, NULL };
    char *val[6];
    lio_exnode_exchange_t *exp;
    int n, err, ret, v_size[6];
    lio_segment_errors_t my_serr;
    char ebuf[128];

    ret = 0;
    if (serr == NULL) serr = &my_serr; //** If caller doesn't care about errors use my own space

    //** Serialize the exnode
    exp = lio_exnode_exchange_create(EX_TEXT);
    lio_exnode_serialize(ex, exp);
    ssize = segment_size(seg);

    //** Get any errors that may have occured
    lioc_get_error_counts(lc, seg, serr);

    //** Update the exnode
    n = 3;
    val[0] = exp->text.text;
    v_size[0] = strlen(val[0]);
    sprintf(buffer, XOT, ssize);
    val[1] = buffer;
    v_size[1] = strlen(val[1]);
    val[2] = NULL;
    v_size[2] = 0;

    n += lioc_encode_error_counts(serr, &(key[3]), &(val[3]), ebuf, &(v_size[3]), 0);
    if ((serr->hard>0) || (serr->soft>0) || (serr->write>0)) {
        log_printf(1, "ERROR: fname=%s hard_errors=%d soft_errors=%d write_errors=%d\n", fname, serr->hard, serr->soft, serr->write);
        ret += 1;
    }


    err = lioc_set_multiple_attrs(lc, creds, fname, NULL, key, (void **)val, v_size, n);
    if (err != OP_STATE_SUCCESS) {
        log_printf(0, "ERROR updating exnode+attrs! fname=%s\n", fname);
        ret += 2;
    }

    lio_exnode_exchange_destroy(exp);

    return(ret);
}

//***********************************************************************
// lioc_remove_object - Removes an object
//***********************************************************************

gop_op_status_t lioc_remove_object_fn(void *arg, int id)
{
    lioc_mk_mv_rm_t *op = (lioc_mk_mv_rm_t *)arg;
    char *ex_data, *val[2];
    char *hkeys[] = { "os.link_count", "system.exnode" };
    lio_exnode_exchange_t *exp;
    lio_exnode_t *ex;
    int err, v_size, ex_remove, vs[2], n;
    gop_op_status_t status = gop_success_status;

    //** First remove and data associated with the object
    v_size = -op->lc->max_attr;

    //** If no object type need to retrieve it
    if (op->type == 0) op->type = lioc_exists(op->lc, op->creds, op->src_path);

    ex_remove = 0;
    if ((op->type & OS_OBJECT_HARDLINK_FLAG) > 0) { //** Got a hard link so check if we do a data removal
        val[0] = val[1] = NULL;
        vs[0] = vs[1] = -op->lc->max_attr;
        lioc_get_multiple_attrs(op->lc, op->creds, op->src_path, op->id, hkeys, (void **)val, vs, 2);

        if (val[0] == NULL) {
            log_printf(15, "Missing link count for fname=%s\n", op->src_path);
            if (val[1] != NULL) free(val[1]);
            return(gop_failure_status);
        }

        n = 100;
        sscanf(val[0], "%d", &n);
        free(val[0]);
        if (n <= 1) {
            ex_remove = 1;
            if (op->ex == NULL) {
                op->ex = val[1];
            } else {
                if (val[1] != NULL) free(val[1]);
            }
        } else {
            if (val[1] != NULL) free(val[1]);
        }
    } else if ((op->type & (OS_OBJECT_SYMLINK_FLAG|OS_OBJECT_DIR_FLAG)) == 0) {
        ex_remove = 1;
    }

    ex_data = op->ex;
    if ((op->ex == NULL) && (ex_remove == 1)) {
        lioc_getattr(op->lc, op->creds, op->src_path, op->id, "system.exnode", (void **)&ex_data, &v_size);
    }

    //** Load the exnode and remove it if needed.
    //** Only done for normal files.  No links or dirs
    if ((ex_remove == 1) && (ex_data != NULL)) {
        //** Deserialize it
        exp = lio_lio_exnode_exchange_text_parse(ex_data);
        ex = lio_exnode_create();
        if (lio_exnode_deserialize(ex, exp, op->lc->ess) != 0) {
            log_printf(15, "ERROR removing data for object fname=%s\n", op->src_path);
            status = gop_failure_status;
        } else {  //** Execute the remove operation since we have a good exnode
            err = gop_sync_exec(exnode_remove(op->lc->tpc_unlimited, ex, op->lc->da, op->lc->timeout));
            if (err != OP_STATE_SUCCESS) {
                log_printf(15, "ERROR removing data for object fname=%s\n", op->src_path);
                status = gop_failure_status;
            }
        }

        //** Clean up
        if (op->ex != NULL) exp->text.text = NULL;  //** The inital exnode is free() by the TP op
        lio_exnode_exchange_destroy(exp);
        lio_exnode_destroy(ex);
    }

    //** Now we can remove the OS entry
    err = gop_sync_exec(os_remove_object(op->lc->os, op->creds, op->src_path));
    if (err != OP_STATE_SUCCESS) {
        log_printf(0, "ERROR: removing file: %s err=%d\n", op->src_path, err);
        status = gop_failure_status;
    }


    return(status);
}

//***********************************************************************
// lc_remove_object - Generates an object removal
//***********************************************************************

gop_op_generic_t *lioc_remove_object(lio_config_t *lc, lio_creds_t *creds, char *path, char *ex_optional, int ftype_optional)
{
    lioc_mk_mv_rm_t *op;

    tbx_type_malloc_clear(op, lioc_mk_mv_rm_t, 1);

    op->lc = lc;
    op->creds = creds;
    op->src_path = strdup(path);
    op->ex = ex_optional;
    op->type = ftype_optional;
    return(gop_tp_op_new(lc->tpc_unlimited, NULL, lioc_remove_object_fn, (void *)op, lioc_free_mk_mv_rm, 1));
}

//***********************************************************************
// lioc_remove_regex_object - Removes objects using regex's
//***********************************************************************

gop_op_status_t lioc_remove_regex_object_fn(void *arg, int id)
{
    lioc_remove_regex_t *op = (lioc_remove_regex_t *)arg;
    os_object_iter_t *it;
    gop_opque_t *q;
    gop_op_generic_t *gop;
    int n, nfailed, atype, prefix_len;
    char *ex, *fname;
    char *key[1];
    int v_size[1];
    gop_op_status_t status2;
    gop_op_status_t status = gop_success_status;

    key[0] = "system.exnode";
    ex = NULL;
    v_size[0] = -op->lc->max_attr;
    it = os_create_object_iter_alist(op->lc->os, op->creds, op->rpath, op->robj, op->obj_types, op->recurse_depth, key, (void **)&ex, v_size, 1);
    if (it == NULL) {
        log_printf(0, "ERROR: Failed with object_iter creation\n");
        return(gop_failure_status);
    }

    //** Cycle through removing the objects
    q = gop_opque_new();
    n = 0;
    nfailed = 0;
    while ((atype = os_next_object(op->lc->os, it, &fname, &prefix_len)) > 0) {

        //** If it's a directory so we need to flush all existing rm's first
        //** Otherwire the rmdir will see pending files
        if ((atype & OS_OBJECT_DIR_FLAG) > 0) {
            opque_waitall(q);
        }

        gop = lioc_remove_object(op->lc, op->creds, fname, ex, atype);
        ex = NULL;  //** Freed in lioc_remove_object
        free(fname);
        gop_opque_add(q, gop);

        if (gop_opque_tasks_left(q) > op->np) {
            gop = opque_waitany(q);
            status2 = gop_get_status(gop);
            if (status2.op_status != OP_STATE_SUCCESS) {
                printf("Failed with gid=%d\n", gop_id(gop));
                nfailed++;
            }
            gop_free(gop, OP_DESTROY);
        }

        n++;
    }

    os_destroy_object_iter(op->lc->os, it);

    opque_waitall(q);
    nfailed += gop_opque_tasks_failed(q);
    gop_opque_free(q, OP_DESTROY);

    status.op_status = (nfailed > 0) ? OP_STATE_FAILURE : OP_STATE_SUCCESS;
    status.error_code = n;
    return(status);
}

//***********************************************************************
// lc_remove_regex_object - Generates an object removal op
//***********************************************************************

gop_op_generic_t *lioc_remove_regex_object(lio_config_t *lc, lio_creds_t *creds, lio_os_regex_table_t *rpath, lio_os_regex_table_t *robj, int obj_types, int recurse_depth, int np)
{
    lioc_remove_regex_t *op;

    tbx_type_malloc_clear(op, lioc_remove_regex_t, 1);

    op->lc = lc;
    op->creds = creds;
    op->rpath = rpath;
    op->robj = robj;
    op->obj_types = obj_types;
    op->recurse_depth = recurse_depth;
    op->np = np;

    return(gop_tp_op_new(lc->tpc_unlimited, NULL, lioc_remove_regex_object_fn, (void *)op, free, 1));
}


//***********************************************************************
// lioc_create_object_fn - Does the actual object creation
//***********************************************************************

gop_op_status_t lioc_create_object_fn(void *arg, int id)
{
    lioc_mk_mv_rm_t *op = (lioc_mk_mv_rm_t *)arg;
    os_fd_t *fd;
    char *dir, *fname;
    lio_exnode_exchange_t *exp;
    lio_exnode_t *ex, *cex;
    ex_id_t ino;
    char inode[32];
    char *val[_n_lioc_create_keys];
    gop_op_status_t status;
    int v_size[_n_lioc_create_keys];
    int err;
    int ex_key = 5;

    status = gop_success_status;

    val[ex_key] = NULL;

    log_printf(15, "START op->ex=%p !!!!!!!!!\n fname=%s\n",  op->ex, op->src_path);

    //** Make the base object
    err = gop_sync_exec(os_create_object(op->lc->os, op->creds, op->src_path, op->type, op->id));
    if (err != OP_STATE_SUCCESS) {
        log_printf(15, "ERROR creating object fname=%s\n", op->src_path);
        status = gop_failure_status;
        goto fail_bad;
    }

    //** Get the parent exnode to dup
    if (op->ex == NULL) {
        lio_os_path_split(op->src_path, &dir, &fname);
        log_printf(15, "dir=%s\n fname=%s\n", dir, fname);

        err = gop_sync_exec(os_open_object(op->lc->os, op->creds, dir, OS_MODE_READ_IMMEDIATE, op->id, &fd, op->lc->timeout));
        if (err != OP_STATE_SUCCESS) {
            log_printf(15, "ERROR opening parent=%s\n", dir);
            free(dir);
            status = gop_failure_status;
            goto fail;
        }
        free(fname);

        v_size[0] = -op->lc->max_attr;
        err = gop_sync_exec(os_get_attr(op->lc->os, op->creds, fd, "system.exnode", (void **)&(val[ex_key]), &(v_size[0])));
        if (err != OP_STATE_SUCCESS) {
            log_printf(15, "ERROR opening parent=%s\n", dir);
            free(dir);
            status = gop_failure_status;
            goto fail;
        }

        //** Close the parent
        err = gop_sync_exec(os_close_object(op->lc->os, fd));
        if (err != OP_STATE_SUCCESS) {
            log_printf(15, "ERROR closing parent fname=%s\n", dir);
            free(dir);
            status = gop_failure_status;
            goto fail;
        }

        free(dir);
    } else {
        val[ex_key] = op->ex;
    }

    //** For a directory we can just copy the exnode.  For a file we have to
    //** Clone it to get unique IDs
    if ((op->type & OS_OBJECT_DIR_FLAG) == 0) {
        //** If this has a caching segment we need to disable it from being adding
        //** to the global cache table cause there could be multiple copies of the
        //** same segment being serialized/deserialized.

        //** Deserialize it
        exp = lio_lio_exnode_exchange_text_parse(val[ex_key]);
        ex = lio_exnode_create();
        if (lio_exnode_deserialize(ex, exp, op->lc->ess_nocache) != 0) {
            log_printf(15, "ERROR parsing parent exnode of op->src_path=%s\n", op->src_path);
            status = gop_failure_status;
            lio_exnode_exchange_destroy(exp);
            lio_exnode_destroy(ex);
            goto fail;
        }

        //** Execute the clone operation
        err = gop_sync_exec(lio_exnode_clone(op->lc->tpc_unlimited, ex, op->lc->da, &cex, NULL, CLONE_STRUCTURE, op->lc->timeout));
        if (err != OP_STATE_SUCCESS) {
            log_printf(15, "ERROR cloning parent src_path=%s\n", op->src_path);
            status = gop_failure_status;
            lio_exnode_exchange_destroy(exp);
            lio_exnode_destroy(ex);
            lio_exnode_destroy(cex);
            goto fail;
        }

        //** Serialize it for storage
        exnode_exchange_free(exp);
        lio_exnode_serialize(cex, exp);
        val[ex_key] = exp->text.text;
        exp->text.text = NULL;
        lio_exnode_exchange_destroy(exp);
        lio_exnode_destroy(ex);
        lio_exnode_destroy(cex);
    }


    //** Open the object so I can add the required attributes
    err = gop_sync_exec(os_open_object(op->lc->os, op->creds, op->src_path, OS_MODE_WRITE_IMMEDIATE, op->id, &fd, op->lc->timeout));
    if (err != OP_STATE_SUCCESS) {
        log_printf(15, "ERROR opening object fname=%s\n", op->src_path);
        status = gop_failure_status;
        goto fail;
    }

    //** Now add the required attributes
    val[0] = an_cred_get_id(op->creds);
    v_size[0] = strlen(val[0]);
    val[1] = op->id;
    v_size[1] = (op->id == NULL) ? 0 : strlen(op->id);
    val[2] = op->id;
    v_size[2] = v_size[1];
    val[3] = op->id;
    v_size[3] = v_size[1];
    ino = 0;
    generate_ex_id(&ino);
    snprintf(inode, 32, XIDT, ino);
    val[4] = inode;
    v_size[4] = strlen(inode);
    v_size[ex_key] = strlen(val[ex_key]);
    val[6] = "0";
    v_size[6] = 1;

    log_printf(15, "NEW ino=%s exnode=%s\n", val[4], val[ex_key]);
    tbx_log_flush();

    err = gop_sync_exec(os_set_multiple_attrs(op->lc->os, op->creds, fd, _lioc_create_keys, (void **)val, v_size, (op->type & OS_OBJECT_FILE_FLAG) ? _n_lioc_file_keys : _n_lioc_dir_keys));
    if (err != OP_STATE_SUCCESS) {
        log_printf(15, "ERROR setting default attr fname=%s\n", op->src_path);
        status = gop_failure_status;
    }


    //** Close the file
    err = gop_sync_exec(os_close_object(op->lc->os, fd));
    if (err != OP_STATE_SUCCESS) {
        log_printf(15, "ERROR closing object fname=%s\n", op->src_path);
        status = gop_failure_status;
    }

fail:
    if (status.op_status != OP_STATE_SUCCESS) gop_sync_exec(os_remove_object(op->lc->os, op->creds, op->src_path));

fail_bad:
    if (val[ex_key] != NULL) free(val[ex_key]);

    return(status);
}


//***********************************************************************
// lc_create_object - Generates an object creation task
//***********************************************************************

gop_op_generic_t *lioc_create_object(lio_config_t *lc, lio_creds_t *creds, char *path, int type, char *ex, char *id)
{
    lioc_mk_mv_rm_t *op;

    tbx_type_malloc_clear(op, lioc_mk_mv_rm_t, 1);

    op->lc = lc;
    op->creds = creds;
    op->src_path = strdup(path);
    op->type = type;
    op->id = (id != NULL) ? strdup(id) : NULL;
    op->ex = (ex != NULL) ? strdup(ex) : NULL;
    return(gop_tp_op_new(lc->tpc_unlimited, NULL, lioc_create_object_fn, (void *)op, lioc_free_mk_mv_rm, 1));
}


//***********************************************************************
// lioc_link_object_fn - Does the actual object creation
//***********************************************************************

gop_op_status_t lioc_link_object_fn(void *arg, int id)
{
    lioc_mk_mv_rm_t *op = (lioc_mk_mv_rm_t *)arg;
    os_fd_t *dfd;
    gop_opque_t *q;
    int err;
    ex_id_t ino;
    char inode[32];
    gop_op_status_t status;
    char *lkeys[] = {"system.exnode", "system.exnode.size"};
    char *spath[2];
    char *vkeys[] = {"system.owner", "system.inode", "os.timestamp.system.create", "os.timestamp.system.modify_data", "os.timestamp.system.modify_attr"};
    char *val[5];
    int vsize[5];

    //** Link the base object
    if (op->type == 1) { //** Symlink
        err = gop_sync_exec(os_symlink_object(op->lc->os, op->creds, op->src_path, op->dest_path, op->id));
    } else {
        err = gop_sync_exec(os_hardlink_object(op->lc->os, op->creds, op->src_path, op->dest_path, op->id));
    }
    if (err != OP_STATE_SUCCESS) {
        log_printf(15, "ERROR linking base object sfname=%s dfname=%s\n", op->src_path, op->dest_path);
        status = gop_failure_status;
        goto finished;
    }

    if (op->type == 0) {  //** HArd link so exit
        status = gop_success_status;
        goto finished;
    }

    q = gop_opque_new();

    //** Open the Destination object
    gop_opque_add(q, os_open_object(op->lc->os, op->creds, op->dest_path, OS_MODE_READ_IMMEDIATE, op->id, &dfd, op->lc->timeout));
    err = opque_waitall(q);
    if (err != OP_STATE_SUCCESS) {
        log_printf(15, "ERROR opening src(%s) or dest(%s) file\n", op->src_path, op->dest_path);
        status = gop_failure_status;
        goto open_fail;
    }

    //** Now link the exnode and size
    spath[0] = op->src_path;
    spath[1] = op->src_path;
    gop_opque_add(q, os_symlink_multiple_attrs(op->lc->os, op->creds, spath, lkeys, dfd, lkeys, 2));

    //** Store the owner, inode, and dates
    val[0] = an_cred_get_id(op->creds);
    vsize[0] = strlen(val[0]);
    ino = 0;
    generate_ex_id(&ino);
    snprintf(inode, 32, XIDT, ino);
    val[1] = inode;
    vsize[1] = strlen(inode);
    val[2] = op->id;
    vsize[2] = (op->id == NULL) ? 0 : strlen(op->id);
    val[3] = op->id;
    vsize[3] = vsize[2];
    val[4] = op->id;
    vsize[4] = vsize[2];
    gop_opque_add(q, os_set_multiple_attrs(op->lc->os, op->creds, dfd, vkeys, (void **)val, vsize, 5));


    //** Wait for everything to complete
    err = opque_waitall(q);
    if (err != OP_STATE_SUCCESS) {
        log_printf(15, "ERROR with attr link or owner set src(%s) or dest(%s) file\n", op->src_path, op->dest_path);
        status = gop_failure_status;
        goto open_fail;
    }

    status = gop_success_status;

open_fail:
    if (dfd != NULL) gop_opque_add(q, os_close_object(op->lc->os, dfd));
    opque_waitall(q);

    gop_opque_free(q, OP_DESTROY);

finished:
    return(status);

}

//***********************************************************************
// lc_link_object - Generates a link object task
//***********************************************************************

gop_op_generic_t *lioc_link_object(lio_config_t *lc, lio_creds_t *creds, int symlink, char *src_path, char *dest_path, char *id)
{
    lioc_mk_mv_rm_t *op;

    tbx_type_malloc_clear(op, lioc_mk_mv_rm_t, 1);

    op->lc = lc;
    op->creds = creds;
    op->type = symlink;
    op->src_path = strdup(src_path);
    op->dest_path = strdup(dest_path);
    op->id = (id != NULL) ? strdup(id) : NULL;
    return(gop_tp_op_new(lc->tpc_unlimited, NULL, lioc_link_object_fn, (void *)op, lioc_free_mk_mv_rm, 1));
}

//***********************************************************************
// lc_move_object - Generates a move object task
//***********************************************************************

gop_op_generic_t *lioc_move_object(lio_config_t *lc, lio_creds_t *creds, char *src_path, char *dest_path)
{
    return(os_move_object(lc->os, creds, src_path, dest_path));
}


//-------------------------------------------------------------------------
//------- Universal Object Iterators
//-------------------------------------------------------------------------


//*************************************************************************
//  lio_unified_object_iter_create - Create an ls object iterator
//*************************************************************************

lio_unified_object_iter_t *lio_unified_object_iter_create(lio_path_tuple_t tuple, lio_os_regex_table_t *path_regex, lio_os_regex_table_t *obj_regex, int obj_types, int rd)
{
    lio_unified_object_iter_t *it;

    tbx_type_malloc_clear(it, lio_unified_object_iter_t, 1);

    it->tuple = tuple;
    if (tuple.is_lio == 1) {
        it->oit = os_create_object_iter(tuple.lc->os, tuple.creds, path_regex, obj_regex, obj_types, NULL, rd, NULL, 0);
    } else {
        it->lit = create_local_object_iter(path_regex, obj_regex, obj_types, rd);
    }

    return(it);
}

//*************************************************************************
//  lio_unified_object_iter_destroy - Destroys an ls object iterator
//*************************************************************************

void lio_unified_object_iter_destroy(lio_unified_object_iter_t *it)
{

    if (it->tuple.is_lio == 1) {
        os_destroy_object_iter(it->tuple.lc->os, it->oit);
    } else {
        destroy_local_object_iter(it->lit);
    }

    free(it);
}

//*************************************************************************
//  lio_unified_next_object - Returns the next object to work on
//*************************************************************************

int lio_unified_next_object(lio_unified_object_iter_t *it, char **fname, int *prefix_len)
{
    int err = 0;

    if (it->tuple.is_lio == 1) {
        err = os_next_object(it->tuple.lc->os, it->oit, fname, prefix_len);
    } else {
        err = local_next_object(it->lit, fname, prefix_len);
    }

    log_printf(15, "ftype=%d\n", err);
    return(err);
}


//-------------------------------------------------------------------------
//------- LIO copy path routines ------------
//-------------------------------------------------------------------------

//*************************************************************************
// cp_lio2lio - Performs a lio->lio copy
//*************************************************************************

gop_op_status_t cp_lio2lio(lio_cp_file_t *cp)
{
    char *buffer;
    gop_op_status_t status;
    gop_op_generic_t *gop;
    gop_opque_t *q;
    int sigsize = 10*1024;
    char sig1[sigsize], sig2[sigsize];
    char *sex_data, *dex_data;
    lio_exnode_t *sex, *dex;
    lio_exnode_exchange_t *sexp, *dexp;
    lio_segment_errors_t errcnts;
    lio_segment_t *sseg, *dseg;
    os_fd_t *sfd, *dfd;
    int sv_size[2], dv_size[3];
    int dtype, err, used, hard_errors;

    info_printf(lio_ifd, 0, "copy %s@%s:%s %s@%s:%s\n", an_cred_get_id(cp->src_tuple.creds), cp->src_tuple.lc->section_name, cp->src_tuple.path, an_cred_get_id(cp->dest_tuple.creds), cp->dest_tuple.lc->section_name, cp->dest_tuple.path);

    status = gop_failure_status;
    err = status.op_status;
    q = gop_opque_new();
    hard_errors = 0;
    sexp = dexp = NULL;
    sex = dex = NULL;
    sfd = dfd = NULL;
    buffer = NULL;

    //** Check if the dest exists and if not creates it
    dtype = lioc_exists(cp->dest_tuple.lc, cp->dest_tuple.creds, cp->dest_tuple.path);

    log_printf(5, "src=%s dest=%s dtype=%d\n", cp->src_tuple.path, cp->dest_tuple.path, dtype);

    if (dtype == 0) { //** Need to create it
        err = gop_sync_exec(lio_create_op(cp->dest_tuple.lc, cp->dest_tuple.creds, cp->dest_tuple.path, OS_OBJECT_FILE_FLAG, NULL, NULL));
        if (err != OP_STATE_SUCCESS) {
            info_printf(lio_ifd, 1, "ERROR creating file(%s)!\n", cp->dest_tuple.path);
            goto finished;
        }
    } else if ((dtype & OS_OBJECT_DIR_FLAG) > 0) { //** It's a dir so fail
        info_printf(lio_ifd, 0, "Destination(%s) is a dir!\n", cp->dest_tuple.path);
        goto finished;
    }

    //** Now get both the exnodes
    gop_opque_add(q, os_open_object(cp->src_tuple.lc->os, cp->src_tuple.creds, cp->src_tuple.path, OS_MODE_READ_BLOCKING, NULL, &sfd, cp->src_tuple.lc->timeout));
    gop_opque_add(q, os_open_object(cp->dest_tuple.lc->os, cp->dest_tuple.creds, cp->dest_tuple.path, OS_MODE_READ_BLOCKING, NULL, &dfd, cp->dest_tuple.lc->timeout));

    //** Wait for the opens to complete
    err = opque_waitall(q);
    if (err != OP_STATE_SUCCESS) {
        log_printf(10, "ERROR with os_open src=%s sfd=%p dest=%s dfd=%p\n", cp->src_tuple.path, sfd, cp->dest_tuple.path, dfd);
        goto finished;
    }

    //** Get both exnodes
    sex_data = dex_data = NULL;
    sv_size[0] = -cp->src_tuple.lc->max_attr;
    dv_size[0] = -cp->dest_tuple.lc->max_attr;
    gop_opque_add(q, os_get_attr(cp->src_tuple.lc->os, cp->src_tuple.creds, sfd, "system.exnode", (void **)&sex_data, sv_size));
    gop_opque_add(q, os_get_attr(cp->dest_tuple.lc->os, cp->dest_tuple.creds, dfd, "system.exnode", (void **)&dex_data, dv_size));

    //** Wait for the exnode retrieval to complete
    err = opque_waitall(q);
    if (err != OP_STATE_SUCCESS) {
        log_printf(10, "ERROR with os_open src=%s sex=%p dest=%s dex=%p\n", cp->src_tuple.path, sex_data, cp->dest_tuple.path, dex_data);
        if (sex_data != NULL) free(sex_data);
        if (dex_data != NULL) free(dex_data);
        goto finished;
    }

    //** Deserailize them
    sexp = lio_lio_exnode_exchange_text_parse(sex_data);
    sex = lio_exnode_create();
    if (lio_exnode_deserialize(sex, sexp, cp->src_tuple.lc->ess) != 0) {
        info_printf(lio_ifd, 0, "ERROR parsing source exnode(%s)!\n", cp->src_tuple.path);
        goto finished;
    }

    sseg = lio_exnode_default_get(sex);
    if (sseg == NULL) {
        info_printf(lio_ifd, 0, "No default segment for source(%s)!\n", cp->src_tuple.path);
        if (dex_data != NULL) free(dex_data);
        goto finished;
    }

    dexp = lio_lio_exnode_exchange_text_parse(dex_data);
    dex = lio_exnode_create();
    if (lio_exnode_deserialize(dex, dexp, cp->dest_tuple.lc->ess) != 0) {
        info_printf(lio_ifd, 0, "ERROR parsing destination exnode(%s)!\n", cp->dest_tuple.path);
        goto finished;
    }

    dseg = lio_exnode_default_get(dex);
    if (dseg == NULL) {
        info_printf(lio_ifd, 0, "No default segment for source(%s)!\n", cp->dest_tuple.path);
        goto finished;
    }

    //** What kind of copy do we do
    used = 0;
    segment_signature(sseg, sig1, &used, sigsize);
    used = 0;
    segment_signature(dseg, sig2, &used, sigsize);

    if ((strcmp(sig1, sig2) == 0) && (cp->slow == 0)) {
        info_printf(lio_ifd, 1, "Cloning %s->%s\n", cp->src_tuple.path, cp->dest_tuple.path);
        gop = segment_clone(sseg, cp->dest_tuple.lc->da, &dseg, CLONE_STRUCT_AND_DATA, NULL, cp->dest_tuple.lc->timeout);
        log_printf(5, "src=%s  clone gid=%d\n", cp->src_tuple.path, gop_id(gop));
    } else {
        info_printf(lio_ifd, 1, "Slow copy:( %s->%s\n", cp->src_tuple.path, cp->dest_tuple.path);
        tbx_type_malloc(buffer, char, cp->bufsize+1);
        gop = lio_segment_copy(cp->dest_tuple.lc->tpc_unlimited, cp->dest_tuple.lc->da, cp->rw_hints, sseg, dseg, 0, 0, -1, cp->bufsize, buffer, 1, cp->dest_tuple.lc->timeout);
    }
    err = gop_waitall(gop);

    if (buffer != NULL) free(buffer);  //** Did a slow copy so clean up

    if (err != OP_STATE_SUCCESS) {
        info_printf(lio_ifd, 0, "Failed uploading data!  path=%s\n", cp->dest_tuple.path);
    }

    gop_free(gop, OP_DESTROY);

    //** Update the dest exnode and misc attributes
    hard_errors = lioc_update_exnode_attrs(cp->dest_tuple.lc, cp->dest_tuple.creds, dex, dseg, cp->dest_tuple.path, &errcnts);
    hard_errors = (hard_errors > 1) ? 1 : 0;

    //**Update the error counts if needed
    hard_errors += errcnts.hard;
    hard_errors += lioc_update_error_counts(cp->src_tuple.lc, cp->src_tuple.creds, cp->src_tuple.path, sseg, 0);

finished:

    //** Close the files
    if (sfd != NULL) gop_opque_add(q, os_close_object(cp->src_tuple.lc->os, sfd));
    if (dfd != NULL) gop_opque_add(q, os_close_object(cp->dest_tuple.lc->os, dfd));
    opque_waitall(q);

    gop_opque_free(q, OP_DESTROY);

    if (sex != NULL) lio_exnode_destroy(sex);
    if (sexp != NULL) lio_exnode_exchange_destroy(sexp);
    if (dex != NULL) lio_exnode_destroy(dex);
    if (dexp != NULL) lio_exnode_exchange_destroy(dexp);

    log_printf(15, "hard_errors=%d err=%d\n", hard_errors, err);

    if ((hard_errors == 0) && (err == OP_STATE_SUCCESS)) status = gop_success_status;

    if (status.op_status != OP_STATE_SUCCESS) { //** Destroy the file
        log_printf(5, "ERROR with copy.  Destroying file=%s\n", cp->dest_tuple.path);
        gop_sync_exec(lioc_remove_object(cp->dest_tuple.lc, cp->dest_tuple.creds, cp->dest_tuple.path, NULL, OS_OBJECT_FILE_FLAG));
    }

    return(status);
}

//*************************************************************************
//  cp_local2lio - local->lio copy
//*************************************************************************

gop_op_status_t cp_local2lio(lio_cp_file_t *cp)
{
    char *buffer;
    char *ex_data;
    lio_exnode_t *ex;
    lio_exnode_exchange_t *exp;
    lio_segment_t *seg;
    lio_segment_errors_t errcnts;
    int v_size[3], dtype, err, err2;
    gop_op_status_t status;
    FILE *fd;

    info_printf(lio_ifd, 0, "copy %s %s@%s:%s\n", cp->src_tuple.path, an_cred_get_id(cp->dest_tuple.creds), cp->dest_tuple.lc->section_name, cp->dest_tuple.path);

    status = gop_failure_status;

    //** Check if it exists and if not create it
    dtype = lioc_exists(cp->dest_tuple.lc, cp->dest_tuple.creds, cp->dest_tuple.path);

    log_printf(5, "src=%s dest=%s dtype=%d bufsize=" XOT "\n", cp->src_tuple.path, cp->dest_tuple.path, dtype, cp->bufsize);

    if (dtype == 0) { //** Need to create it
        err = gop_sync_exec(lio_create_op(cp->dest_tuple.lc, cp->dest_tuple.creds, cp->dest_tuple.path, OS_OBJECT_FILE_FLAG, NULL, NULL));
        if (err != OP_STATE_SUCCESS) {
            info_printf(lio_ifd, 1, "ERROR creating file(%s)!\n", cp->dest_tuple.path);
            goto finished;
        }
    } else if ((dtype & OS_OBJECT_DIR_FLAG) > 0) { //** It's a dir so fail
        info_printf(lio_ifd, 0, "ERROR: Destination(%s) is a dir!\n", cp->dest_tuple.path);
        goto finished;
    }

    //** Get the exnode
    v_size[0] = -cp->dest_tuple.lc->max_attr;
    err = lioc_getattr(cp->dest_tuple.lc, cp->dest_tuple.creds, cp->dest_tuple.path, NULL, "system.exnode", (void **)&ex_data, v_size);
    if (err != OP_STATE_SUCCESS) {
        info_printf(lio_ifd, 0, "ERROR: Failed retrieving exnode!  path=%s\n", cp->dest_tuple.path);
        goto finished;
    }

    fd = fopen(cp->src_tuple.path, "r");
    if (fd == NULL) {
        info_printf(lio_ifd, 0, "ERROR: Failed opening source file!  path=%s\n", cp->src_tuple.path);
        goto finished;
    }

    //** Load it
    exp = lio_lio_exnode_exchange_text_parse(ex_data);
    ex = lio_exnode_create();
    if (lio_exnode_deserialize(ex, exp, cp->dest_tuple.lc->ess) != 0) {
        info_printf(lio_ifd, 0, "ERROR parsing exnode!  Aborting!\n");
        lio_exnode_destroy(ex);
        lio_exnode_exchange_destroy(exp);
        fclose(fd);
        goto finished;
    }

    //** Get the default view to use
    seg = lio_exnode_default_get(ex);
    if (seg == NULL) {
        info_printf(lio_ifd, 0, "No default segment!  Aborting!\n");
        lio_exnode_destroy(ex);
        lio_exnode_exchange_destroy(exp);
        fclose(fd);
        goto finished;
    }

    tbx_type_malloc(buffer, char, cp->bufsize+1);

    log_printf(0, "BEFORE PUT\n");
    err = gop_sync_exec(segment_put(cp->dest_tuple.lc->tpc_unlimited, cp->dest_tuple.lc->da, cp->rw_hints, fd, seg, 0, -1, cp->bufsize, buffer, 1, 3600));
    log_printf(0, "AFTER PUT\n");

    fclose(fd);

    if (err != OP_STATE_SUCCESS) {
        info_printf(lio_ifd, 0, "ERROR: Failed uploading data!  path=%s\n", cp->dest_tuple.path);
    }

//log_printf(0, "SLEEPING for 60 seconds!!!!!!!\n");
//sleep(60);
//log_printf(0, "WAKING UP!!!!!!!!!!!\n");

    //** Update the dest exnode and misc attributes
    err2 = lioc_update_exnode_attrs(cp->dest_tuple.lc, cp->dest_tuple.creds, ex, seg, cp->dest_tuple.path, &errcnts);

    lio_exnode_destroy(ex);
    lio_exnode_exchange_destroy(exp);

    free(buffer);

    if ((errcnts.hard == 0) && (err == OP_STATE_SUCCESS) && (err2 == 0)) status = gop_success_status;

finished:

    return(status);
}

//*************************************************************************
//  cp_lio2local - lio>local copy
//*************************************************************************

gop_op_status_t cp_lio2local(lio_cp_file_t *cp)
{
    int err, ftype;
    char *ex_data, *buffer;
    lio_exnode_t *ex;
    lio_exnode_exchange_t *exp;
    lio_segment_t *seg;
    int v_size, hard_errors;
    gop_op_status_t status;
    FILE *fd;

    info_printf(lio_ifd, 0, "copy %s@%s:%s %s\n", an_cred_get_id(cp->src_tuple.creds), cp->src_tuple.lc->section_name, cp->src_tuple.path, cp->dest_tuple.path);

    status = gop_failure_status;

    //** Check if it exists
    ftype = lioc_exists(cp->src_tuple.lc, cp->src_tuple.creds, cp->src_tuple.path);

    log_printf(5, "src=%s dest=%s dtype=%d\n", cp->src_tuple.path, cp->dest_tuple.path, ftype);

    if ((ftype & OS_OBJECT_FILE_FLAG) == 0) { //** Doesn't exist or is a dir
        info_printf(lio_ifd, 1, "ERROR source file(%s) doesn't exist or is a dir ftype=%d!\n", cp->src_tuple.path, ftype);
        goto finished;
    }

    //** Get the exnode
    v_size = -cp->src_tuple.lc->max_attr;
    err = lioc_getattr(cp->src_tuple.lc, cp->src_tuple.creds, cp->src_tuple.path, NULL, "system.exnode", (void **)&ex_data, &v_size);
    if (err != OP_STATE_SUCCESS) {
        info_printf(lio_ifd, 0, "ERROR: Failed retrieving exnode!  path=%s\n", cp->src_tuple.path);
        goto finished;
    }

    //** Load it
    exp = lio_lio_exnode_exchange_text_parse(ex_data);
    ex = lio_exnode_create();
    if (lio_exnode_deserialize(ex, exp, cp->src_tuple.lc->ess) != 0) {
        info_printf(lio_ifd, 0, "ERROR parsing exnode!  Aborting!\n");
        lio_exnode_destroy(ex);
        lio_exnode_exchange_destroy(exp);
        goto finished;
    }

    //** Get the default view to use
    seg = lio_exnode_default_get(ex);
    if (seg == NULL) {
        info_printf(lio_ifd, 0, "No default segment!  Aborting!\n");
        lio_exnode_destroy(ex);
        lio_exnode_exchange_destroy(exp);
        goto finished;
    }

    fd = fopen(cp->dest_tuple.path, "w");
    if (fd == NULL) {
        info_printf(lio_ifd, 0, "ERROR: Failed opending dest file!  path=%s\n", cp->dest_tuple.path);
        lio_exnode_destroy(ex);
        lio_exnode_exchange_destroy(exp);
        goto finished;
    }

    tbx_type_malloc(buffer, char, cp->bufsize+1);
    gop_sync_exec(segment_get(cp->src_tuple.lc->tpc_unlimited, cp->src_tuple.lc->da, cp->rw_hints, seg, fd, 0, -1, cp->bufsize, buffer, 3600));
    free(buffer);

    fclose(fd);

    //**Update the error counts if needed
    hard_errors = lioc_update_error_counts(cp->src_tuple.lc, cp->src_tuple.creds, cp->src_tuple.path, seg, 0);
    if (hard_errors != 0) {
        info_printf(lio_ifd, 0, "ERROR: Hard error during download! hard_errors=%d  path=%s\n", hard_errors, cp->dest_tuple.path);
    }

    lio_exnode_destroy(ex);
    lio_exnode_exchange_destroy(exp);

    if (hard_errors == 0) status = gop_success_status;

finished:
    return(status);
}

//***********************************************************************
// lioc_truncate_fn - Performs an segment truncation
//***********************************************************************

gop_op_status_t lioc_truncate_fn(void *arg, int tid)
{
    lioc_trunc_t *op = (lioc_trunc_t *)arg;
    char *ex_data, buffer[128];
    lio_exnode_t *ex;
    lio_exnode_exchange_t *exp;
    lio_segment_t *seg;
    char *key[] = {"system.exnode", "system.exnode.size", "os.timestamp.system.modify_data"};
    char *val[3];
    int v_size[3], err, hard_errors, ftype;
    gop_op_status_t status;

    status = gop_failure_status;

    //** Check if it exists
    ftype = lioc_exists(op->tuple.lc, op->tuple.creds, op->tuple.path);

    log_printf(5, "fname=%s\n", op->tuple.path);

    if ((ftype & OS_OBJECT_FILE_FLAG) == 0) { //** Doesn't exist or is a dir
        info_printf(lio_ifd, 1, "ERROR source file(%s) doesn't exist or is a dir ftype=%d!\n", op->tuple.path, ftype);
        goto finished;
    }

    //** Get the exnode
    v_size[0] = -op->tuple.lc->max_attr;
    err = lioc_getattr(op->tuple.lc, op->tuple.creds, op->tuple.path, NULL, "system.exnode", (void **)&ex_data, v_size);
    if (err != OP_STATE_SUCCESS) {
        info_printf(lio_ifd, 0, "Failed retrieving exnode!  path=%s\n", op->tuple.path);
        goto finished;
    }

    //** Load it
    exp = lio_lio_exnode_exchange_text_parse(ex_data);
    ex = lio_exnode_create();
    if (lio_exnode_deserialize(ex, exp, op->tuple.lc->ess) != 0) {
        info_printf(lio_ifd, 0, "ERROR parsing exnode!  Aborting!\n");
        lio_exnode_destroy(ex);
        lio_exnode_exchange_destroy(exp);
        goto finished;
    }

    //** Get the default view to use
    seg = lio_exnode_default_get(ex);
    if (seg == NULL) {
        info_printf(lio_ifd, 0, "No default segment!  Aborting!\n");
        lio_exnode_destroy(ex);
        lio_exnode_exchange_destroy(exp);
        goto finished;
    }

    gop_sync_exec(lio_segment_truncate(seg, op->tuple.lc->da, op->new_size, 60));

    //** Serialize the exnode
    exnode_exchange_free(exp);
    lio_exnode_serialize(ex, exp);

    //** Update the OS exnode
    val[0] = exp->text.text;
    v_size[0] = strlen(val[0]);
    sprintf(buffer, I64T, op->new_size);
    val[1] = buffer;
    v_size[1] = strlen(val[1]);
    val[2] = NULL;
    v_size[2] = 0;
    lioc_set_multiple_attrs(op->tuple.lc, op->tuple.creds, op->tuple.path, NULL, key, (void **)val, v_size, 3);

    //**Update the error counts if needed
    hard_errors = lioc_update_error_counts(op->tuple.lc, op->tuple.creds, op->tuple.path, seg, 0);

    lio_exnode_destroy(ex);
    lio_exnode_exchange_destroy(exp);

    if (hard_errors == 0) status = gop_success_status;

finished:
    return(status);
}

//***********************************************************************
// lioc_truncate - Truncates an object
//***********************************************************************

gop_op_generic_t *lioc_truncate(lio_path_tuple_t *tuple, ex_off_t new_size)
{
    lioc_trunc_t *op;

    tbx_type_malloc_clear(op, lioc_trunc_t, 1);

    op->tuple = *tuple;
    op->new_size = new_size;
    return(gop_tp_op_new(tuple->lc->tpc_unlimited, NULL, lioc_truncate_fn, (void *)op, free, 1));
}

