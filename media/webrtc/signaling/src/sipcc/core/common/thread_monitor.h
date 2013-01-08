/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef THREAD_MONITOR_H_
#define THREAD_MONITOR_H_

#include "cpr_threads.h"

/*
 * This should be a list of threads that could be created by SIPCC
 * On Windows, the MSGQ thread is not started.
 */
typedef enum {
  THREADMON_CCAPP,
  THREADMON_SIP,
  THREADMON_MSGQ,
  THREADMON_GSM,
  THREADMON_MAX
} thread_monitor_id_t;

/*
 * thread_started
 *
 * Should be called exactly once for each thread created.
 *
 * @param[in]  monitor_id     - enum of which thread created
 * @param[in]  thread         - created thread
 */
void thread_started(thread_monitor_id_t monitor_id, cprThread_t thread);

/*
 * join_all_threads
 *
 * Join all threads that were started.
 */
void join_all_threads();

#endif

