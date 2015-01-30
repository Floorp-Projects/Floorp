/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iomanip>
#include <iostream>
#include <vector>

#if defined(WEBRTC_WIN)
#include "webrtc/base/win32.h"
#endif

#include "webrtc/base/cpumonitor.h"
#include "webrtc/base/flags.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/timing.h"
#include "webrtc/test/testsupport/gtest_disable.h"

namespace rtc {

static const int kMaxCpus = 1024;
static const int kSettleTime = 100;  // Amount of time to between tests.
static const int kIdleTime = 500;  // Amount of time to be idle in ms.
static const int kBusyTime = 1000;  // Amount of time to be busy in ms.
static const int kLongInterval = 2000;  // Interval longer than busy times

class BusyThread : public rtc::Thread {
 public:
  BusyThread(double load, double duration, double interval) :
    load_(load), duration_(duration), interval_(interval) {
  }
  virtual ~BusyThread() {
    Stop();
  }
  void Run() {
    Timing time;
    double busy_time = interval_ * load_ / 100.0;
    for (;;) {
      time.BusyWait(busy_time);
      time.IdleWait(interval_ - busy_time);
      if (duration_) {
        duration_ -= interval_;
        if (duration_ <= 0) {
          break;
        }
      }
    }
  }
 private:
  double load_;
  double duration_;
  double interval_;
};

class CpuLoadListener : public sigslot::has_slots<> {
 public:
  CpuLoadListener()
      : current_cpus_(0),
        cpus_(0),
        process_load_(.0f),
        system_load_(.0f),
        count_(0) {
  }

  void OnCpuLoad(int current_cpus, int cpus, float proc_load, float sys_load) {
    current_cpus_ = current_cpus;
    cpus_ = cpus;
    process_load_ = proc_load;
    system_load_ = sys_load;
    ++count_;
  }

  int current_cpus() const { return current_cpus_; }
  int cpus() const { return cpus_; }
  float process_load() const { return process_load_; }
  float system_load() const { return system_load_; }
  int count() const { return count_; }

 private:
  int current_cpus_;
  int cpus_;
  float process_load_;
  float system_load_;
  int count_;
};

// Set affinity (which cpu to run on), but respecting FLAG_affinity:
// -1 means no affinity - run on whatever cpu is available.
// 0 .. N means run on specific cpu.  The tool will create N threads and call
//   SetThreadAffinity on 0 to N - 1 as cpu.  FLAG_affinity sets the first cpu
//   so the range becomes affinity to affinity + N - 1
// Note that this function affects Windows scheduling, effectively giving
//   the thread with affinity for a specified CPU more priority on that CPU.
bool SetThreadAffinity(BusyThread* t, int cpu, int affinity) {
#if defined(WEBRTC_WIN)
  if (affinity >= 0) {
    return ::SetThreadAffinityMask(t->GetHandle(),
        1 << (cpu + affinity)) != FALSE;
  }
#endif
  return true;
}

bool SetThreadPriority(BusyThread* t, int prio) {
  if (!prio) {
    return true;
  }
  bool ok = t->SetPriority(static_cast<rtc::ThreadPriority>(prio));
  if (!ok) {
    std::cout << "Error setting thread priority." << std::endl;
  }
  return ok;
}

int CpuLoad(double cpuload, double duration, int numthreads,
            int priority, double interval, int affinity) {
  int ret = 0;
  std::vector<BusyThread*> threads;
  for (int i = 0; i < numthreads; ++i) {
    threads.push_back(new BusyThread(cpuload, duration, interval));
    // NOTE(fbarchard): Priority must be done before Start.
    if (!SetThreadPriority(threads[i], priority) ||
       !threads[i]->Start() ||
       !SetThreadAffinity(threads[i], i, affinity)) {
      ret = 1;
      break;
    }
  }
  // Wait on each thread
  if (ret == 0) {
    for (int i = 0; i < numthreads; ++i) {
      threads[i]->Stop();
    }
  }

  for (int i = 0; i < numthreads; ++i) {
    delete threads[i];
  }
  return ret;
}

// Make 2 CPUs busy
static void CpuTwoBusyLoop(int busytime) {
  CpuLoad(100.0, busytime / 1000.0, 2, 1, 0.050, -1);
}

// Make 1 CPUs busy
static void CpuBusyLoop(int busytime) {
  CpuLoad(100.0, busytime / 1000.0, 1, 1, 0.050, -1);
}

// Make 1 use half CPU time.
static void CpuHalfBusyLoop(int busytime) {
  CpuLoad(50.0, busytime / 1000.0, 1, 1, 0.050, -1);
}

void TestCpuSampler(bool test_proc, bool test_sys, bool force_fallback) {
  CpuSampler sampler;
  sampler.set_force_fallback(force_fallback);
  EXPECT_TRUE(sampler.Init());
  sampler.set_load_interval(100);
  int cpus = sampler.GetMaxCpus();

  // Test1: CpuSampler under idle situation.
  Thread::SleepMs(kSettleTime);
  sampler.GetProcessLoad();
  sampler.GetSystemLoad();

  Thread::SleepMs(kIdleTime);

  float proc_idle = 0.f, sys_idle = 0.f;
  if (test_proc) {
    proc_idle = sampler.GetProcessLoad();
  }
  if (test_sys) {
      sys_idle = sampler.GetSystemLoad();
  }
  if (test_proc) {
    LOG(LS_INFO) << "ProcessLoad Idle:      "
                 << std::setiosflags(std::ios_base::fixed)
                 << std::setprecision(2) << std::setw(6) << proc_idle;
    EXPECT_GE(proc_idle, 0.f);
    EXPECT_LE(proc_idle, static_cast<float>(cpus));
  }
  if (test_sys) {
    LOG(LS_INFO) << "SystemLoad Idle:       "
                 << std::setiosflags(std::ios_base::fixed)
                 << std::setprecision(2) << std::setw(6) << sys_idle;
    EXPECT_GE(sys_idle, 0.f);
    EXPECT_LE(sys_idle, static_cast<float>(cpus));
  }

  // Test2: CpuSampler with main process at 50% busy.
  Thread::SleepMs(kSettleTime);
  sampler.GetProcessLoad();
  sampler.GetSystemLoad();

  CpuHalfBusyLoop(kBusyTime);

  float proc_halfbusy = 0.f, sys_halfbusy = 0.f;
  if (test_proc) {
    proc_halfbusy = sampler.GetProcessLoad();
  }
  if (test_sys) {
    sys_halfbusy = sampler.GetSystemLoad();
  }
  if (test_proc) {
    LOG(LS_INFO) << "ProcessLoad Halfbusy:  "
                 << std::setiosflags(std::ios_base::fixed)
                 << std::setprecision(2) << std::setw(6) << proc_halfbusy;
    EXPECT_GE(proc_halfbusy, 0.f);
    EXPECT_LE(proc_halfbusy, static_cast<float>(cpus));
  }
  if (test_sys) {
    LOG(LS_INFO) << "SystemLoad Halfbusy:   "
                 << std::setiosflags(std::ios_base::fixed)
                 << std::setprecision(2) << std::setw(6) << sys_halfbusy;
    EXPECT_GE(sys_halfbusy, 0.f);
    EXPECT_LE(sys_halfbusy, static_cast<float>(cpus));
  }

  // Test3: CpuSampler with main process busy.
  Thread::SleepMs(kSettleTime);
  sampler.GetProcessLoad();
  sampler.GetSystemLoad();

  CpuBusyLoop(kBusyTime);

  float proc_busy = 0.f, sys_busy = 0.f;
  if (test_proc) {
    proc_busy = sampler.GetProcessLoad();
  }
  if (test_sys) {
    sys_busy = sampler.GetSystemLoad();
  }
  if (test_proc) {
    LOG(LS_INFO) << "ProcessLoad Busy:      "
                 << std::setiosflags(std::ios_base::fixed)
                 << std::setprecision(2) << std::setw(6) << proc_busy;
    EXPECT_GE(proc_busy, 0.f);
    EXPECT_LE(proc_busy, static_cast<float>(cpus));
  }
  if (test_sys) {
    LOG(LS_INFO) << "SystemLoad Busy:       "
                 << std::setiosflags(std::ios_base::fixed)
                 << std::setprecision(2) << std::setw(6) << sys_busy;
    EXPECT_GE(sys_busy, 0.f);
    EXPECT_LE(sys_busy, static_cast<float>(cpus));
  }

  // Test4: CpuSampler with 2 cpus process busy.
  if (cpus >= 2) {
    Thread::SleepMs(kSettleTime);
    sampler.GetProcessLoad();
    sampler.GetSystemLoad();

    CpuTwoBusyLoop(kBusyTime);

    float proc_twobusy = 0.f, sys_twobusy = 0.f;
    if (test_proc) {
      proc_twobusy = sampler.GetProcessLoad();
    }
    if (test_sys) {
      sys_twobusy = sampler.GetSystemLoad();
    }
    if (test_proc) {
      LOG(LS_INFO) << "ProcessLoad 2 CPU Busy:"
                   << std::setiosflags(std::ios_base::fixed)
                   << std::setprecision(2) << std::setw(6) << proc_twobusy;
      EXPECT_GE(proc_twobusy, 0.f);
      EXPECT_LE(proc_twobusy, static_cast<float>(cpus));
    }
    if (test_sys) {
      LOG(LS_INFO) << "SystemLoad 2 CPU Busy: "
                   << std::setiosflags(std::ios_base::fixed)
                   << std::setprecision(2) << std::setw(6) << sys_twobusy;
      EXPECT_GE(sys_twobusy, 0.f);
      EXPECT_LE(sys_twobusy, static_cast<float>(cpus));
    }
  }

  // Test5: CpuSampler with idle process after being busy.
  Thread::SleepMs(kSettleTime);
  sampler.GetProcessLoad();
  sampler.GetSystemLoad();

  Thread::SleepMs(kIdleTime);

  if (test_proc) {
    proc_idle = sampler.GetProcessLoad();
  }
  if (test_sys) {
    sys_idle = sampler.GetSystemLoad();
  }
  if (test_proc) {
    LOG(LS_INFO) << "ProcessLoad Idle:      "
                 << std::setiosflags(std::ios_base::fixed)
                 << std::setprecision(2) << std::setw(6) << proc_idle;
    EXPECT_GE(proc_idle, 0.f);
    EXPECT_LE(proc_idle, proc_busy);
  }
  if (test_sys) {
    LOG(LS_INFO) << "SystemLoad Idle:       "
                 << std::setiosflags(std::ios_base::fixed)
                 << std::setprecision(2) << std::setw(6) << sys_idle;
    EXPECT_GE(sys_idle, 0.f);
    EXPECT_LE(sys_idle, static_cast<float>(cpus));
  }
}

TEST(CpuMonitorTest, TestCpus) {
  CpuSampler sampler;
  EXPECT_TRUE(sampler.Init());
  int current_cpus = sampler.GetCurrentCpus();
  int cpus = sampler.GetMaxCpus();
  LOG(LS_INFO) << "Current Cpus:     " << std::setw(9) << current_cpus;
  LOG(LS_INFO) << "Maximum Cpus:     " << std::setw(9) << cpus;
  EXPECT_GT(cpus, 0);
  EXPECT_LE(cpus, kMaxCpus);
  EXPECT_GT(current_cpus, 0);
  EXPECT_LE(current_cpus, cpus);
}

#if defined(WEBRTC_WIN)
// Tests overall system CpuSampler using legacy OS fallback code if applicable.
TEST(CpuMonitorTest, TestGetSystemLoadForceFallback) {
  TestCpuSampler(false, true, true);
}
#endif

// Tests both process and system functions in use at same time.
TEST(CpuMonitorTest, TestGetBothLoad) {
  TestCpuSampler(true, true, false);
}

// Tests a query less than the interval produces the same value.
TEST(CpuMonitorTest, TestInterval) {
  CpuSampler sampler;
  EXPECT_TRUE(sampler.Init());

  // Test1: Set interval to large value so sampler will not update.
  sampler.set_load_interval(kLongInterval);

  sampler.GetProcessLoad();
  sampler.GetSystemLoad();

  float proc_orig = sampler.GetProcessLoad();
  float sys_orig = sampler.GetSystemLoad();

  Thread::SleepMs(kIdleTime);

  float proc_halftime = sampler.GetProcessLoad();
  float sys_halftime = sampler.GetSystemLoad();

  EXPECT_EQ(proc_orig, proc_halftime);
  EXPECT_EQ(sys_orig, sys_halftime);
}

TEST(CpuMonitorTest, TestCpuMonitor) {
  CpuMonitor monitor(Thread::Current());
  CpuLoadListener listener;
  monitor.SignalUpdate.connect(&listener, &CpuLoadListener::OnCpuLoad);
  EXPECT_TRUE(monitor.Start(10));
  // We have checked cpu load more than twice.
  EXPECT_TRUE_WAIT(listener.count() > 2, 1000);
  EXPECT_GT(listener.current_cpus(), 0);
  EXPECT_GT(listener.cpus(), 0);
  EXPECT_GE(listener.process_load(), .0f);
  EXPECT_GE(listener.system_load(), .0f);

  monitor.Stop();
  // Wait 20 ms to ake sure all signals are delivered.
  Thread::Current()->ProcessMessages(20);
  int old_count = listener.count();
  Thread::Current()->ProcessMessages(20);
  // Verfy no more siganls.
  EXPECT_EQ(old_count, listener.count());
}

}  // namespace rtc
