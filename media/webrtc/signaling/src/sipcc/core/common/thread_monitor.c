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
static boolean wait_list[THREADMON_MAX];

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
  wait_list[monitor_id] = TRUE;
}

/*
 * thread_ended
 *
 * Must be called by thread itself on THREAD_UNLOAD to unblock join_all_threads()
 *
 * Alerts if init_thread_monitor() has not been called.
 *
 * @param[in]  monitor_id     - enum of which thread created
 */

/*
 * init_thread_monitor
 *
 * Thread-monitor supports a way to let threads notify the joiner when it is
 * safe to join, IF it is initialized with a dispatcher and a waiter function.
 *
 * Example: To use SyncRunnable or otherwise block on the main thread without
 * deadlocking on shutdown, pass in the following:
 *
 * static void dispatcher(thread_ended_funct fun, thread_monitor_id_t id)
 * {
 *   nsresult rv = gMain->Dispatch(WrapRunnableNM(fun, id), NS_DISPATCH_NORMAL);
 *   MOZ_ASSERT(NS_SUCCEEDED(rv));
 * }
 *
 * static void waiter() { NS_ProcessPendingEvents(gMain); }
 *
 * The created thread must then call thread_ended() on receiving THREAD_UNLOAD.
 */

static thread_ended_dispatcher_funct dispatcher = NULL;
static join_wait_funct waiter = NULL;

void init_thread_monitor(thread_ended_dispatcher_funct dispatch,
                         join_wait_funct wait) {
  dispatcher = dispatch;
  waiter = wait;
}

static void thread_ended_m(thread_monitor_id_t monitor_id) {
  MOZ_ASSERT(dispatcher);
  MOZ_ASSERT(waiter);
  MOZ_ASSERT(monitor_id < THREADMON_MAX);
  if (monitor_id >= THREADMON_MAX) {
    return;
  }

  /* Should already be started */
  MOZ_ASSERT(thread_list[monitor_id]);
  MOZ_ASSERT(wait_list[monitor_id]);

  wait_list[monitor_id] = FALSE;
}

void thread_ended(thread_monitor_id_t monitor_id) {
  MOZ_ASSERT(dispatcher);
  dispatcher (&thread_ended_m, monitor_id);
}

/*
 * join_all_threads
 *
 * Join all threads that were started.
 */
void join_all_threads() {
  int i;
  MOZ_ASSERT(dispatcher);
  MOZ_ASSERT(waiter);

  for (i = 0; i < THREADMON_MAX; i++) {
    if (thread_list[i] != NULL) {
      while (wait_list[i]) {
        waiter();
      }
      cprJoinThread(thread_list[i]);
      cpr_free(thread_list[i]);
      thread_list[i] = NULL;
    }
  }
}
