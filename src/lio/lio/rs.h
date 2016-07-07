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

#ifndef ACCRE_LIO_RESOURCE_SERVICE_ABSTRACT_H_INCLUDED
#define ACCRE_LIO_RESOURCE_SERVICE_ABSTRACT_H_INCLUDED

#include <apr_thread_cond.h>
#include <apr_thread_mutex.h>
#include <gop/mq.h>
#include <lio/visibility.h>
#include <lio/ds.h>
#include <lio/ex3_fwd.h>

#ifdef __cplusplus
extern "C" {
#endif

// Typedefs
typedef struct lio_resource_service_fn_t lio_resource_service_fn_t;
typedef struct lio_rid_change_entry_t lio_rid_change_entry_t;
typedef struct lio_rs_hints_t lio_rs_hints_t;
typedef struct lio_rs_mapping_notify_t lio_rs_mapping_notify_t;
typedef struct lio_rs_request_t lio_rs_request_t;
typedef struct lio_rs_space_t lio_rs_space_t;
typedef void rs_query_t;
typedef char *(*lio_rs_get_rid_value_fn_t)(lio_resource_service_fn_t *arg, char *rid_key, char *key);
typedef char *(*lio_rs_get_rid_config_fn_t)(lio_resource_service_fn_t *arg);
typedef void (*lio_rs_translate_cap_set_fn_t)(lio_resource_service_fn_t *arg, char *rid_key, data_cap_set_t *cap);
typedef void (*lio_rs_register_mapping_updates_fn_t)(lio_resource_service_fn_t *arg, lio_rs_mapping_notify_t *map_version);
typedef void (*lio_rs_unregister_mapping_updates_fn_t)(lio_resource_service_fn_t *arg, lio_rs_mapping_notify_t *map_version);
typedef int  (*lio_rs_query_add_fn_t)(lio_resource_service_fn_t *arg, rs_query_t **q, int op, char *key, int key_op, char *val, int val_op);
typedef void (*lio_rs_query_append_fn_t)(lio_resource_service_fn_t *arg, rs_query_t *q, rs_query_t *qappend);
typedef rs_query_t *(*lio_rs_query_dup_fn_t)(lio_resource_service_fn_t *arg, rs_query_t *q);
typedef rs_query_t *(*lio_rs_query_new_fn_t)(lio_resource_service_fn_t *arg);
typedef void (*lio_rs_query_destroy_fn_t)(lio_resource_service_fn_t *arg, rs_query_t *q);
typedef gop_op_generic_t *(*lio_rs_data_request_fn_t)(lio_resource_service_fn_t *arg, data_attr_t *da, rs_query_t *q, data_cap_set_t **caps, lio_rs_request_t *req, int req_size, lio_rs_hints_t *hints_list, int fixed_size, int n_rid, int ignore_fixed_err, int timeout);
typedef rs_query_t *(*lio_rs_query_parse_fn_t)(lio_resource_service_fn_t *arg, char *value);
typedef char *(*lio_rs_query_print_fn_t)(lio_resource_service_fn_t *arg, rs_query_t *q);
typedef void (*lio_rs_destroy_service_fn_t)(lio_resource_service_fn_t *rs);
 
// FIXME:leaky
typedef struct lio_rsq_base_ele_t lio_rsq_base_ele_t;
typedef struct lio_rsq_base_t lio_rsq_base_t;
typedef struct lio_rs_remote_client_priv_t lio_rs_remote_client_priv_t;
typedef struct lio_rs_remote_server_priv_t lio_rs_remote_server_priv_t;
typedef struct lio_rs_simple_priv_t lio_rs_simple_priv_t;
typedef struct lio_rss_check_entry_t lio_rss_check_entry_t;
typedef struct lio_rss_rid_entry_t lio_rss_rid_entry_t;

// Functions

// Preprocessor constants
// FIXME: leaky
#define RSQ_BASE_OP_KV      1
#define RSQ_BASE_OP_NOT     2
#define RSQ_BASE_OP_AND     3

#define RSQ_BASE_KV_EXACT   1
#define RSQ_BASE_KV_ANY     3

// Preprocessor macros
#define rs_get_rid_config(rs) (rs)->get_rid_config(rs)
#define rs_query_add(rs, q, op, key, kop, val, vop) (rs)->query_add(rs, q, op, key, kop, val, vop)
#define rs_query_append(rs, q, qappend) (rs)->query_append(rs, q, qappend)
#define rs_query_destroy(rs, q) (rs)->query_destroy(rs, q)
#define rs_query_new(rs) (rs)->query_new(rs)
#define rs_query_parse(rs, value) (rs)->query_parse(rs, value)
#define rs_query_print(rs, q) (rs)->query_print(rs, q)
#define rs_register_mapping_updates(rs, notify) (rs)->register_mapping_updates(rs, notify)
#define rs_unregister_mapping_updates(rs, notify) (rs)->unregister_mapping_updates(rs, notify)

// Exported types. To be obscured
struct lio_rs_mapping_notify_t {
    apr_thread_mutex_t *lock;
    apr_thread_cond_t *cond;
    int map_version;
    int status_version;
};

struct lio_resource_service_fn_t {
    void *priv;
    char *type;
    lio_rs_get_rid_value_fn_t get_rid_value;
    lio_rs_get_rid_config_fn_t get_rid_config;
    lio_rs_translate_cap_set_fn_t translate_cap_set;
    lio_rs_register_mapping_updates_fn_t register_mapping_updates;
    lio_rs_unregister_mapping_updates_fn_t unregister_mapping_updates;
    lio_rs_query_add_fn_t query_add;
    lio_rs_query_append_fn_t query_append;
    lio_rs_query_dup_fn_t query_dup;
    lio_rs_query_new_fn_t query_new;
    lio_rs_query_destroy_fn_t query_destroy;
    lio_rs_data_request_fn_t data_request;
    lio_rs_query_parse_fn_t query_parse;
    lio_rs_query_print_fn_t query_print;
    lio_rs_destroy_service_fn_t destroy_service;
};

struct lio_rid_change_entry_t {
    char *rid_key;      //** RID key
    char *ds_key;       //** Data service key
    int state;          //** Tweaking state
    ex_off_t delta;     //** How much to change the space by in bytes.  Negative means remove and postive means add space to the RID
    ex_off_t tolerance; //** Tolerance in bytes.  When abs(delta)<tolerance we stop tweaking the RID
};

#ifdef __cplusplus
}
#endif

#endif /* ^ ACCRE_LIO_RESOURCE_SERVICE_ABSTRACT_H_INCLUDED ^ */ 
