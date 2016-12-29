/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/source/event_timer_posix.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "webrtc/base/checks.h"

namespace webrtc {

// static
EventTimerWrapper* EventTimerWrapper::Create() {
  return new EventTimerPosix();
}

const long int E6 = 1000000;
const long int E9 = 1000 * E6;

EventTimerPosix::EventTimerPosix()
    : event_set_(false),
      timer_thread_(nullptr),
      created_at_(),
      periodic_(false),
      time_(0),
      count_(0) {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mutex_, &attr);
#ifdef WEBRTC_CLOCK_TYPE_REALTIME
  pthread_cond_init(&cond_, 0);
#else
  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
  pthread_cond_init(&cond_, &cond_attr);
  pthread_condattr_destroy(&cond_attr);
#endif
}

EventTimerPosix::~EventTimerPosix() {
  StopTimer();
  pthread_cond_destroy(&cond_);
  pthread_mutex_destroy(&mutex_);
}

// TODO(pbos): Make this void.
bool EventTimerPosix::Set() {
  RTC_CHECK_EQ(0, pthread_mutex_lock(&mutex_));
  event_set_ = true;
  pthread_cond_signal(&cond_);
  pthread_mutex_unlock(&mutex_);
  return true;
}

EventTypeWrapper EventTimerPosix::Wait(unsigned long timeout) {
  int ret_val = 0;
  RTC_CHECK_EQ(0, pthread_mutex_lock(&mutex_));

  if (!event_set_) {
    if (WEBRTC_EVENT_INFINITE != timeout) {
      timespec end_at;
#ifndef WEBRTC_MAC
#ifdef WEBRTC_CLOCK_TYPE_REALTIME
      clock_gettime(CLOCK_REALTIME, &end_at);
#else
      clock_gettime(CLOCK_MONOTONIC, &end_at);
#endif
#else
      timeval value;
      struct timezone time_zone;
      time_zone.tz_minuteswest = 0;
      time_zone.tz_dsttime = 0;
      gettimeofday(&value, &time_zone);
      TIMEVAL_TO_TIMESPEC(&value, &end_at);
#endif
      end_at.tv_sec  += timeout / 1000;
      end_at.tv_nsec += (timeout - (timeout / 1000) * 1000) * E6;

      if (end_at.tv_nsec >= E9) {
        end_at.tv_sec++;
        end_at.tv_nsec -= E9;
      }
      while (ret_val == 0 && !event_set_)
        ret_val = pthread_cond_timedwait(&cond_, &mutex_, &end_at);
    } else {
      while (ret_val == 0 && !event_set_)
        ret_val = pthread_cond_wait(&cond_, &mutex_);
    }
  }

  RTC_DCHECK(ret_val == 0 || ret_val == ETIMEDOUT);

  // Reset and signal if set, regardless of why the thread woke up.
  if (event_set_) {
    ret_val = 0;
    event_set_ = false;
  }
  pthread_mutex_unlock(&mutex_);

  return ret_val == 0 ? kEventSignaled : kEventTimeout;
}

EventTypeWrapper EventTimerPosix::Wait(timespec* end_at) {
  int ret_val = 0;
  RTC_CHECK_EQ(0, pthread_mutex_lock(&mutex_));

  while (ret_val == 0 && !event_set_)
    ret_val = pthread_cond_timedwait(&cond_, &mutex_, end_at);

  RTC_DCHECK(ret_val == 0 || ret_val == ETIMEDOUT);

  // Reset and signal if set, regardless of why the thread woke up.
  if (event_set_) {
    ret_val = 0;
    event_set_ = false;
  }
  pthread_mutex_unlock(&mutex_);

  return ret_val == 0 ? kEventSignaled : kEventTimeout;
}

bool EventTimerPosix::StartTimer(bool periodic, unsigned long time) {
  pthread_mutex_lock(&mutex_);
  if (timer_thread_) {
    if (periodic_) {
      // Timer already started.
      pthread_mutex_unlock(&mutex_);
      return false;
    } else  {
      // New one shot timer
      time_ = time;
      created_at_.tv_sec = 0;
      timer_event_->Set();
      pthread_mutex_unlock(&mutex_);
      return true;
    }
  }

  // Start the timer thread
  timer_event_.reset(new EventTimerPosix());
  const char* thread_name = "WebRtc_event_timer_thread";
  timer_thread_.reset(new rtc::PlatformThread(Run, this, thread_name));
  periodic_ = periodic;
  time_ = time;
  timer_thread_->Start();
  timer_thread_->SetPriority(rtc::kRealtimePriority);
  pthread_mutex_unlock(&mutex_);

  return true;
}

bool EventTimerPosix::Run(void* obj) {
  return static_cast<EventTimerPosix*>(obj)->Process();
}

bool EventTimerPosix::Process() {
  pthread_mutex_lock(&mutex_);
  if (created_at_.tv_sec == 0) {
#ifndef WEBRTC_MAC
#ifdef WEBRTC_CLOCK_TYPE_REALTIME
    clock_gettime(CLOCK_REALTIME, &created_at_);
#else
    clock_gettime(CLOCK_MONOTONIC, &created_at_);
#endif
#else
    timeval value;
    struct timezone time_zone;
    time_zone.tz_minuteswest = 0;
    time_zone.tz_dsttime = 0;
    gettimeofday(&value, &time_zone);
    TIMEVAL_TO_TIMESPEC(&value, &created_at_);
#endif
    count_ = 0;
  }

  timespec end_at;
  unsigned long long time = time_ * ++count_;
  end_at.tv_sec  = created_at_.tv_sec + time / 1000;
  end_at.tv_nsec = created_at_.tv_nsec + (time - (time / 1000) * 1000) * E6;

  if (end_at.tv_nsec >= E9) {
    end_at.tv_sec++;
    end_at.tv_nsec -= E9;
  }

  pthread_mutex_unlock(&mutex_);
  if (timer_event_->Wait(&end_at) == kEventSignaled)
    return true;

  pthread_mutex_lock(&mutex_);
  if (periodic_ || count_ == 1)
    Set();
  pthread_mutex_unlock(&mutex_);

  return true;
}

bool EventTimerPosix::StopTimer() {
  if (timer_event_) {
    timer_event_->Set();
  }
  if (timer_thread_) {
    timer_thread_->Stop();
    timer_thread_.reset();
  }
  timer_event_.reset();

  // Set time to zero to force new reference time for the timer.
  memset(&created_at_, 0, sizeof(created_at_));
  count_ = 0;
  return true;
}

}  // namespace webrtc
