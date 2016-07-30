/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#if !defined(CUBEB_UTILS_UNIX)
#define CUBEB_UTILS_UNIX

#include <pthread.h>
#include <errno.h>
#include <stdio.h>

/* This wraps a critical section to track the owner in debug mode. */
class owned_critical_section
{
public:
  owned_critical_section()
  {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
#ifdef DEBUG
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
#else
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
#endif

#ifdef DEBUG
    int r =
#endif
    pthread_mutex_init(&mutex, &attr);
#ifdef DEBUG
    assert(r == 0);
#endif

    pthread_mutexattr_destroy(&attr);
  }

  ~owned_critical_section()
  {
#ifdef DEBUG
    int r =
#endif
    pthread_mutex_destroy(&mutex);
#ifdef DEBUG
    assert(r == 0);
#endif
  }

  void enter()
  {
#ifdef DEBUG
    int r =
#endif
    pthread_mutex_lock(&mutex);
#ifdef DEBUG
    assert(r == 0 && "Deadlock");
#endif
  }

  void leave()
  {
#ifdef DEBUG
    int r =
#endif
    pthread_mutex_unlock(&mutex);
#ifdef DEBUG
    assert(r == 0 && "Unlocking unlocked mutex");
#endif
  }

  void assert_current_thread_owns()
  {
#ifdef DEBUG
    int r = pthread_mutex_lock(&mutex);
    assert(r == EDEADLK);
#endif
  }

private:
  pthread_mutex_t mutex;
};

#endif /* CUBEB_UTILS_UNIX */
