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

#ifndef ACCRE_GOP_OPQUE_H_INCLUDED
#define ACCRE_GOP_OPQUE_H_INCLUDED

#include <gop/callback.h>
#include <gop/gop_visibility.h>
#include <gop/gop.h>
#include <gop/types.h>
#include <tbx/stack.h>

#ifdef __cplusplus
extern "C" {
#endif

// Typedefs

// Functions
GOP_API void gop_default_sort_ops(void *arg, opque_t *que);
GOP_API void gop_init_opque_system();
GOP_API int gop_opque_add(opque_t *que, op_generic_t *gop);
GOP_API void gop_opque_free(opque_t *que, int mode);
GOP_API opque_t *gop_opque_new();
GOP_API void gop_shutdown();

// Preprocessor macros
#define lock_opque(q)   log_printf(15, "lock_opque: qid=%d\n", (q)->opque->op.base.id); apr_thread_mutex_lock((q)->opque->op.base.ctl->lock)
#define unlock_opque(q) log_printf(15, "unlock_opque: qid=%d\n", (q)->opque->op.base.id); apr_thread_mutex_unlock((q)->opque->op.base.ctl->lock)
#define opque_get_gop(q) &((q)->op)
#define opque_failure_gop_cb_set(q, fn, priv) gop_cb_set(&(q->failure_cb), fn, priv)
#define opque_callback_append(q, cb) gop_callback_append(opque_get_gop(q), (cb))
#define opque_get_next_finished(q) gop_get_next_finished(opque_get_gop(q))
#define opque_get_next_failed(q) gop_get_next_failed(opque_get_gop(q))
#define opque_tasks_failed(q) gop_tasks_failed(opque_get_gop(q))
#define opque_tasks_finished(q) gop_tasks_finished(opque_get_gop(q))
#define opque_tasks_left(q) gop_tasks_left(opque_get_gop(q))
#define opque_task_count(q) q->qd.nsubmitted
#define opque_waitall(q) gop_waitall(opque_get_gop(q))
#define opque_waitany(q) gop_waitany(opque_get_gop(q))
#define opque_start_execution(q) gop_start_execution(opque_get_gop(q))
#define opque_finished_submission(q) gop_finished_submission(opque_get_gop(q))

#define opque_set_status(q, val) gop_set_status(opque_get_gop(q), val)
#define opque_get_status(q) gop_get_status(opque_get_gop(q))
#define opque_completed_successfully(q) gop_completed_successfully(q)



// Exported types. To be obscured.
struct que_data_t {
    tbx_stack_t *list;         //** List of tasks
    tbx_stack_t *finished;     //** lists that have completed and not yet processed
    tbx_stack_t *failed;       //** All lists that fail are also placed here
    int nleft;             //** Number of lists left to be processed
    int nsubmitted;        //** Nunmber of submitted tasks (doesn't count sub q's)
    int finished_submission; //** No more tasks will be submitted so it's safe to free the data when finished
    callback_t failure_cb;   //** Only used if a task fails
    opque_t *opque;
};

struct opque_t {
    op_generic_t op;
    que_data_t   qd;
};



#ifdef __cplusplus
}
#endif

#endif /* ^ ACCRE_GOP_OPQUE_H_INCLUDED ^ */ 
