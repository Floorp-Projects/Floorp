/*
 * Copyright © 2017 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 *
 *
 * Purpose
 * =============================================================================
 * In CoreAudio, the data callback will holds a mutex shared with AudioUnit
 * (mutex_AU). Thus, if the callback request another mutex M held by the another
 * function, without releasing mutex_AU, then it will cause a deadlock when the
 * another function, which holds the mutex M, request to use AudioUnit.
 *
 * The following figure illustrates the deadlock in bug 1337805:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1337805
 * (The detail analysis can be found on bug 1350511:
 *  https://bugzilla.mozilla.org/show_bug.cgi?id=1350511)
 *
 *                   holds
 *  data_callback <---------- mutext_AudioUnit(mutex_AU)
 *      |                            ^
 *      |                            |
 *      | request                    | request
 *      |                            |
 *      v           holds            |
 *  mutex_cubeb ------------> get_channel_layout
 *
 * In this example, the "audiounit_get_channel_layout" in f4edfb8:
 * https://github.com/kinetiknz/cubeb/blob/f4edfb8eea920887713325e44773f3a2d959860c/src/cubeb_audiounit.cpp#L2725
 * requests the mutex_AU to create an AudioUnit, when it holds a mutex for cubeb
 * context. Meanwhile, the data callback who holds the mutex_AU requests the
 * mutex for cubeb context. As a result, it causes a deadlock.
 *
 * The problem is solve by pull 236: https://github.com/kinetiknz/cubeb/pull/236
 * We store the latest channel layout and return it when there is an active
 * AudioUnit, otherwise, we will create an AudioUnit to get it.
 *
 * Although the problem is solved, to prevent it happens again, we add the test
 * here in case someone without such knowledge misuses the AudioUnit in
 * get_channel_layout. Moreover, it's a good way to record the known issues
 * to warn other developers.
 */

#include "gtest/gtest.h"
//#define ENABLE_NORMAL_LOG
//#define ENABLE_VERBOSE_LOG
#include "common.h"       // for layout_infos
#include "cubeb/cubeb.h"  // for cubeb utils
#include "cubeb_utils.h"  // for owned_critical_section, auto_lock
#include <iostream>       // for fprintf
#include <pthread.h>      // for pthread
#include <signal.h>       // for signal
#include <stdexcept>      // for std::logic_error
#include <string>         // for std::string
#include <unistd.h>       // for sleep, usleep
#include <atomic>         // for std::atomic

// The signal alias for calling our thread killer.
#define CALL_THREAD_KILLER SIGUSR1

// This indicator will become true when our pending task thread is killed by
// ourselves.
bool killed = false;

// This indicator will become true when the assigned task is done.
std::atomic<bool> task_done{ false };

// Indicating the data callback is fired or not.
bool called = false;

// Toggle to true when running data callback. Before data callback gets
// the mutex for cubeb context, it toggles back to false.
// The task to get channel layout should be executed when this is true.
std::atomic<bool> callbacking_before_getting_context{ false };

owned_critical_section context_mutex;
cubeb * context = nullptr;

cubeb * get_cubeb_context_unlocked()
{
  if (context) {
    return context;
  }

  int r = CUBEB_OK;
  r = common_init(&context, "Cubeb deadlock test");
  if (r != CUBEB_OK) {
    context = nullptr;
  }

  return context;
}

cubeb * get_cubeb_context()
{
  auto_lock lock(context_mutex);
  return get_cubeb_context_unlocked();
}

void state_cb_audio(cubeb_stream * /*stream*/, void * /*user*/, cubeb_state /*state*/)
{
}

// Fired by coreaudio's rendering mechanism. It holds a mutex shared with the
// current used AudioUnit.
template<typename T>
long data_cb(cubeb_stream * /*stream*/, void * /*user*/,
             const void * /*inputbuffer*/, void * outputbuffer, long nframes)
{
  called = true;

  uint64_t tid; // Current thread id.
  pthread_threadid_np(NULL, &tid);
  fprintf(stderr, "Audio output is on thread %llu\n", tid);

  if (!task_done) {
    callbacking_before_getting_context = true;
    fprintf(stderr, "[%llu] time to switch thread\n", tid);
    // Force to switch threads by sleeping 10 ms. Notice that anything over
    // 10ms would create a glitch. It's intended here for test, so the delay
    // is ok.
    usleep(10000);
    callbacking_before_getting_context = false;
  }

  fprintf(stderr, "[%llu] try getting backend id ...\n", tid);

  // Try requesting mutex for context by get_cubeb_context()
  // when holding a mutex for AudioUnit.
  char const * backend_id = cubeb_get_backend_id(get_cubeb_context());
  fprintf(stderr, "[%llu] callback on %s\n", tid, backend_id);

  // Mute the output (or get deaf)
  memset(outputbuffer, 0, nframes * 2 * sizeof(float));
  return nframes;
}

// Called by wait_to_get_layout, which is run out of main thread.
void get_preferred_channel_layout()
{
  auto_lock lock(context_mutex);
  cubeb * context = get_cubeb_context_unlocked();
  ASSERT_TRUE(!!context);

  // We will cause a deadlock if cubeb_get_preferred_channel_layout requests
  // mutex for AudioUnit when it holds mutex for context.
  cubeb_channel_layout layout;
  int r = cubeb_get_preferred_channel_layout(context, &layout);
  ASSERT_EQ(r == CUBEB_OK, layout != CUBEB_LAYOUT_UNDEFINED);
  fprintf(stderr, "layout is %s\n", layout_infos[layout].name);
}

void * wait_to_get_layout(void *)
{
  uint64_t tid; // Current thread id.
  pthread_threadid_np(NULL, &tid);

  while(!callbacking_before_getting_context) {
    fprintf(stderr, "[%llu] waiting for data callback ...\n", tid);
    usleep(1000); // Force to switch threads by sleeping 1 ms.
  }

  fprintf(stderr, "[%llu] try getting channel layout ...\n", tid);
  get_preferred_channel_layout(); // Deadlock checkpoint.
  task_done = true;

  return NULL;
}

void * watchdog(void * s)
{
  uint64_t tid; // Current thread id.
  pthread_threadid_np(NULL, &tid);

  pthread_t subject = *((pthread_t *) s);
  uint64_t stid; // task thread id.
  pthread_threadid_np(subject, &stid);

  unsigned int sec = 2;
  fprintf(stderr, "[%llu] sleep %d seconds before checking task for thread %llu\n", tid, sec, stid);
  sleep(sec); // Force to switch threads.

  fprintf(stderr, "[%llu] check task for thread %llu now\n", tid, stid);
  if (!task_done) {
    fprintf(stderr, "[%llu] kill the task thread %llu\n", tid, stid);
    pthread_kill(subject, CALL_THREAD_KILLER);
    pthread_detach(subject);
    // pthread_kill doesn't release the mutex held by the killed thread,
    // so we need to unlock it manually.
    context_mutex.unlock();
  }
  fprintf(stderr, "[%llu] the assigned task for thread %llu is %sdone\n", tid, stid, (task_done) ? "" : "not ");

  return NULL;
}

void thread_killer(int signal)
{
  ASSERT_EQ(signal, CALL_THREAD_KILLER);
  fprintf(stderr, "task thread is killed!\n");
  killed = true;
}

TEST(cubeb, run_deadlock_test)
{
#if !defined(__APPLE__)
  FAIL() << "Deadlock test is only for OSX now";
#endif

  cubeb * ctx = get_cubeb_context();
  ASSERT_TRUE(!!ctx);

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  cubeb_stream_params params;
  params.format = CUBEB_SAMPLE_FLOAT32NE;
  params.rate = 44100;
  params.channels = 2;
  params.layout = CUBEB_LAYOUT_STEREO;
  params.prefs = CUBEB_STREAM_PREF_NONE;

  cubeb_stream * stream = NULL;
  int r = cubeb_stream_init(ctx, &stream, "test deadlock", NULL, NULL, NULL,
                            &params, 512, &data_cb<float>, state_cb_audio, NULL);
  ASSERT_EQ(r, CUBEB_OK);

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_stream_at_exit(stream, cubeb_stream_destroy);

  // Install signal handler.
  signal(CALL_THREAD_KILLER, thread_killer);

  pthread_t subject, detector;
  pthread_create(&subject, NULL, wait_to_get_layout, NULL);
  pthread_create(&detector, NULL, watchdog, (void *) &subject);

  uint64_t stid, dtid;
  pthread_threadid_np(subject, &stid);
  pthread_threadid_np(detector, &dtid);
  fprintf(stderr, "task thread %llu, monitor thread %llu are created\n", stid, dtid);

  cubeb_stream_start(stream);

  pthread_join(subject, NULL);
  pthread_join(detector, NULL);

  ASSERT_TRUE(called);

  fprintf(stderr, "\n%sDeadlock detected!\n", (called && !task_done.load()) ? "" : "No ");

  // Check the task is killed by ourselves if deadlock happends.
  // Otherwise, thread_killer should not be triggered.
  ASSERT_NE(task_done.load(), killed);

  ASSERT_TRUE(task_done.load());

  cubeb_stream_stop(stream);
}
