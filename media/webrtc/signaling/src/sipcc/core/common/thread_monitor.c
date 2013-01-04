/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "thread_monitor.h"
#include "prtypes.h"
#include "mozilla/Assertions.h"

/*
 * If thread is running, should have a non-zero entry here that is the threadId
 */
static cprThread_t thread_list[THREADMON_MAX];

/*
 * thread_started
 *
 * Should be called exactly once for each thread created.
 *
 * @param[in]  monitor_id     - enum of which thread created
 * @param[in]  thread         - created thread
 */
void thread_started(thread_monitor_id_t monitor_id, cprThread_t thread) {
  MOZ_ASSERT(monitor_id < THREADMON_MAX);
  if (monitor_id >= THREADMON_MAX) {
    return;
  }

  /* Should not already be started */
  MOZ_ASSERT(thread_list[monitor_id] == NULL);

  thread_list[monitor_id] = thread;
}

/*
 * join_all_threads
 *
 * Join all threads that were started.
 */
void join_all_threads() {
  int i;

  for (i = 0; i < THREADMON_MAX; i++) {
    if (thread_list[i] != NULL) {
      cprJoinThread(thread_list[i]);
      cpr_free(thread_list[i]);
      thread_list[i] = NULL;
    }
  }
}
