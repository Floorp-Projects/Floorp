/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/cpumonitor.h"

#include <string>

#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/systeminfo.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"

#if defined(WEBRTC_WIN)
#include "webrtc/base/win32.h"
#include <winternl.h>
#endif

#if defined(WEBRTC_POSIX)
#include <sys/time.h>
#endif

#if defined(WEBRTC_MAC)
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/host_info.h>
#include <mach/task.h>
#endif  // defined(WEBRTC_MAC)

#if defined(WEBRTC_LINUX)
#include <sys/resource.h>
#include <errno.h>
#include <stdio.h>
#include "webrtc/base/fileutils.h"
#include "webrtc/base/pathutils.h"
#endif // defined(WEBRTC_LINUX)

#if defined(WEBRTC_MAC)
static uint64 TimeValueTToInt64(const time_value_t &time_value) {
  return rtc::kNumMicrosecsPerSec * time_value.seconds +
      time_value.microseconds;
}
#endif  // defined(WEBRTC_MAC)

// How CpuSampler works
// When threads switch, the time they spent is accumulated to system counters.
// The time can be treated as user, kernel or idle.
// user time is applications.
// kernel time is the OS, including the thread switching code itself.
//   typically kernel time indicates IO.
// idle time is a process that wastes time when nothing is ready to run.
//
// User time is broken down by process (application).  One of the applications
// is the current process.  When you add up all application times, this is
// system time.  If only your application is running, system time should be the
// same as process time.
//
// All cores contribute to these accumulators.  A dual core process is able to
// process twice as many cycles as a single core.  The actual code efficiency
// may be worse, due to contention, but the available cycles is exactly twice
// as many, and the cpu load will reflect the efficiency.  Hyperthreads behave
// the same way.  The load will reflect 200%, but the actual amount of work
// completed will be much less than a true dual core.
//
// Total available performance is the sum of all accumulators.
// If you tracked this for 1 second, it would essentially give you the clock
// rate - number of cycles per second.
// Speed step / Turbo Boost is not considered, so infact more processing time
// may be available.

namespace rtc {

// Note Tests on Windows show 600 ms is minimum stable interval for Windows 7.
static const int32 kDefaultInterval = 950;  // Slightly under 1 second.

CpuSampler::CpuSampler()
    : min_load_interval_(kDefaultInterval)
#if defined(WEBRTC_WIN)
      , get_system_times_(NULL),
      nt_query_system_information_(NULL),
      force_fallback_(false)
#endif
    {
}

CpuSampler::~CpuSampler() {
}

// Set minimum interval in ms between computing new load values. Default 950.
void CpuSampler::set_load_interval(int min_load_interval) {
  min_load_interval_ = min_load_interval;
}

bool CpuSampler::Init() {
  sysinfo_.reset(new SystemInfo);
  cpus_ = sysinfo_->GetMaxCpus();
  if (cpus_ == 0) {
    return false;
  }
#if defined(WEBRTC_WIN)
  // Note that GetSystemTimes is available in Windows XP SP1 or later.
  // http://msdn.microsoft.com/en-us/library/ms724400.aspx
  // NtQuerySystemInformation is used as a fallback.
  if (!force_fallback_) {
    get_system_times_ = GetProcAddress(GetModuleHandle(L"kernel32.dll"),
        "GetSystemTimes");
  }
  nt_query_system_information_ = GetProcAddress(GetModuleHandle(L"ntdll.dll"),
      "NtQuerySystemInformation");
  if ((get_system_times_ == NULL) && (nt_query_system_information_ == NULL)) {
    return false;
  }
#endif
#if defined(WEBRTC_LINUX)
  Pathname sname("/proc/stat");
  sfile_.reset(Filesystem::OpenFile(sname, "rb"));
  if (!sfile_) {
    LOG_ERR(LS_ERROR) << "open proc/stat failed:";
    return false;
  }
  if (!sfile_->DisableBuffering()) {
    LOG_ERR(LS_ERROR) << "could not disable buffering for proc/stat";
    return false;
  }
#endif // defined(WEBRTC_LINUX)
  GetProcessLoad();  // Initialize values.
  GetSystemLoad();
  // Help next user call return valid data by recomputing load.
  process_.prev_load_time_ = 0u;
  system_.prev_load_time_ = 0u;
  return true;
}

float CpuSampler::UpdateCpuLoad(uint64 current_total_times,
                                uint64 current_cpu_times,
                                uint64 *prev_total_times,
                                uint64 *prev_cpu_times) {
  float result = 0.f;
  if (current_total_times < *prev_total_times ||
      current_cpu_times < *prev_cpu_times) {
    LOG(LS_ERROR) << "Inconsistent time values are passed. ignored";
  } else {
    const uint64 cpu_diff = current_cpu_times - *prev_cpu_times;
    const uint64 total_diff = current_total_times - *prev_total_times;
    result = (total_diff == 0ULL ? 0.f :
              static_cast<float>(1.0f * cpu_diff / total_diff));
    if (result > static_cast<float>(cpus_)) {
      result = static_cast<float>(cpus_);
    }
    *prev_total_times = current_total_times;
    *prev_cpu_times = current_cpu_times;
  }
  return result;
}

float CpuSampler::GetSystemLoad() {
  uint32 timenow = Time();
  int elapsed = static_cast<int>(TimeDiff(timenow, system_.prev_load_time_));
  if (min_load_interval_ != 0 && system_.prev_load_time_ != 0u &&
      elapsed < min_load_interval_) {
    return system_.prev_load_;
  }
#if defined(WEBRTC_WIN)
  uint64 total_times, cpu_times;

  typedef BOOL (_stdcall *GST_PROC)(LPFILETIME, LPFILETIME, LPFILETIME);
  typedef NTSTATUS (WINAPI *QSI_PROC)(SYSTEM_INFORMATION_CLASS,
      PVOID, ULONG, PULONG);

  GST_PROC get_system_times = reinterpret_cast<GST_PROC>(get_system_times_);
  QSI_PROC nt_query_system_information = reinterpret_cast<QSI_PROC>(
      nt_query_system_information_);

  if (get_system_times) {
    FILETIME idle_time, kernel_time, user_time;
    if (!get_system_times(&idle_time, &kernel_time, &user_time)) {
      LOG(LS_ERROR) << "::GetSystemTimes() failed: " << ::GetLastError();
      return 0.f;
    }
    // kernel_time includes Kernel idle time, so no need to
    // include cpu_time as total_times
    total_times = ToUInt64(kernel_time) + ToUInt64(user_time);
    cpu_times = total_times - ToUInt64(idle_time);

  } else {
    if (nt_query_system_information) {
      ULONG returned_length = 0;
      scoped_ptr<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[]> processor_info(
          new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[cpus_]);
      nt_query_system_information(
          ::SystemProcessorPerformanceInformation,
          reinterpret_cast<void*>(processor_info.get()),
          cpus_ * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION),
          &returned_length);

      if (returned_length !=
          (cpus_ * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION))) {
        LOG(LS_ERROR) << "NtQuerySystemInformation has unexpected size";
        return 0.f;
      }

      uint64 current_idle = 0;
      uint64 current_kernel = 0;
      uint64 current_user = 0;
      for (int ix = 0; ix < cpus_; ++ix) {
        current_idle += processor_info[ix].IdleTime.QuadPart;
        current_kernel += processor_info[ix].UserTime.QuadPart;
        current_user += processor_info[ix].KernelTime.QuadPart;
      }
      total_times = current_kernel + current_user;
      cpu_times = total_times - current_idle;
    } else {
      return 0.f;
    }
  }
#endif  // WEBRTC_WIN 

#if defined(WEBRTC_MAC)
  mach_port_t mach_host = mach_host_self();
  host_cpu_load_info_data_t cpu_info;
  mach_msg_type_number_t info_count = HOST_CPU_LOAD_INFO_COUNT;
  kern_return_t kr = host_statistics(mach_host, HOST_CPU_LOAD_INFO,
                                     reinterpret_cast<host_info_t>(&cpu_info),
                                     &info_count);
  mach_port_deallocate(mach_task_self(), mach_host);
  if (KERN_SUCCESS != kr) {
    LOG(LS_ERROR) << "::host_statistics() failed";
    return 0.f;
  }

  const uint64 cpu_times = cpu_info.cpu_ticks[CPU_STATE_NICE] +
      cpu_info.cpu_ticks[CPU_STATE_SYSTEM] +
      cpu_info.cpu_ticks[CPU_STATE_USER];
  const uint64 total_times = cpu_times + cpu_info.cpu_ticks[CPU_STATE_IDLE];
#endif  // defined(WEBRTC_MAC)

#if defined(WEBRTC_LINUX)
  if (!sfile_) {
    LOG(LS_ERROR) << "Invalid handle for proc/stat";
    return 0.f;
  }
  std::string statbuf;
  sfile_->SetPosition(0);
  if (!sfile_->ReadLine(&statbuf)) {
    LOG_ERR(LS_ERROR) << "Could not read proc/stat file";
    return 0.f;
  }

  unsigned long long user;
  unsigned long long nice;
  unsigned long long system;
  unsigned long long idle;
  if (sscanf(statbuf.c_str(), "cpu %Lu %Lu %Lu %Lu",
             &user, &nice,
             &system, &idle) != 4) {
    LOG_ERR(LS_ERROR) << "Could not parse cpu info";
    return 0.f;
  }
  const uint64 cpu_times = nice + system + user;
  const uint64 total_times = cpu_times + idle;
#endif  // defined(WEBRTC_LINUX)

#if defined(__native_client__)
  // TODO(ryanpetrie): Implement this via PPAPI when it's available.
  const uint64 cpu_times = 0;
  const uint64 total_times = 0;
#endif  // defined(__native_client__)

  system_.prev_load_time_ = timenow;
  system_.prev_load_ = UpdateCpuLoad(total_times,
                                     cpu_times * cpus_,
                                     &system_.prev_total_times_,
                                     &system_.prev_cpu_times_);
  return system_.prev_load_;
}

float CpuSampler::GetProcessLoad() {
  uint32 timenow = Time();
  int elapsed = static_cast<int>(TimeDiff(timenow, process_.prev_load_time_));
  if (min_load_interval_ != 0 && process_.prev_load_time_ != 0u &&
      elapsed < min_load_interval_) {
    return process_.prev_load_;
  }
#if defined(WEBRTC_WIN)
  FILETIME current_file_time;
  ::GetSystemTimeAsFileTime(&current_file_time);

  FILETIME create_time, exit_time, kernel_time, user_time;
  if (!::GetProcessTimes(::GetCurrentProcess(),
                         &create_time, &exit_time, &kernel_time, &user_time)) {
    LOG(LS_ERROR) << "::GetProcessTimes() failed: " << ::GetLastError();
    return 0.f;
  }

  const uint64 total_times =
      ToUInt64(current_file_time) - ToUInt64(create_time);
  const uint64 cpu_times =
      (ToUInt64(kernel_time) + ToUInt64(user_time));
#endif  // WEBRTC_WIN 

#if defined(WEBRTC_POSIX)
  // Common to both OSX and Linux.
  struct timeval tv;
  gettimeofday(&tv, NULL);
  const uint64 total_times = tv.tv_sec * kNumMicrosecsPerSec + tv.tv_usec;
#endif

#if defined(WEBRTC_MAC)
  // Get live thread usage.
  task_thread_times_info task_times_info;
  mach_msg_type_number_t info_count = TASK_THREAD_TIMES_INFO_COUNT;

  if (KERN_SUCCESS != task_info(mach_task_self(), TASK_THREAD_TIMES_INFO,
                                reinterpret_cast<task_info_t>(&task_times_info),
                                &info_count)) {
    LOG(LS_ERROR) << "::task_info(TASK_THREAD_TIMES_INFO) failed";
    return 0.f;
  }

  // Get terminated thread usage.
  task_basic_info task_term_info;
  info_count = TASK_BASIC_INFO_COUNT;
  if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO,
                                reinterpret_cast<task_info_t>(&task_term_info),
                                &info_count)) {
    LOG(LS_ERROR) << "::task_info(TASK_BASIC_INFO) failed";
    return 0.f;
  }

  const uint64 cpu_times = (TimeValueTToInt64(task_times_info.user_time) +
      TimeValueTToInt64(task_times_info.system_time) +
      TimeValueTToInt64(task_term_info.user_time) +
      TimeValueTToInt64(task_term_info.system_time));
#endif  // defined(WEBRTC_MAC)

#if defined(WEBRTC_LINUX)
  rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) < 0) {
    LOG_ERR(LS_ERROR) << "getrusage failed";
    return 0.f;
  }

  const uint64 cpu_times =
      (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * kNumMicrosecsPerSec +
      usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
#endif  // defined(WEBRTC_LINUX)

#if defined(__native_client__)
  // TODO(ryanpetrie): Implement this via PPAPI when it's available.
  const uint64 cpu_times = 0;
#endif  // defined(__native_client__)

  process_.prev_load_time_ = timenow;
  process_.prev_load_ = UpdateCpuLoad(total_times,
                                     cpu_times,
                                     &process_.prev_total_times_,
                                     &process_.prev_cpu_times_);
  return process_.prev_load_;
}

int CpuSampler::GetMaxCpus() const {
  return cpus_;
}

int CpuSampler::GetCurrentCpus() {
  return sysinfo_->GetCurCpus();
}

///////////////////////////////////////////////////////////////////
// Implementation of class CpuMonitor.
CpuMonitor::CpuMonitor(Thread* thread)
    : monitor_thread_(thread) {
}

CpuMonitor::~CpuMonitor() {
  Stop();
}

void CpuMonitor::set_thread(Thread* thread) {
  ASSERT(monitor_thread_ == NULL || monitor_thread_ == thread);
  monitor_thread_ = thread;
}

bool CpuMonitor::Start(int period_ms) {
  if (!monitor_thread_  || !sampler_.Init()) return false;

  monitor_thread_->SignalQueueDestroyed.connect(
       this, &CpuMonitor::OnMessageQueueDestroyed);

  period_ms_ = period_ms;
  monitor_thread_->PostDelayed(period_ms_, this);

  return true;
}

void CpuMonitor::Stop() {
  if (monitor_thread_) {
    monitor_thread_->Clear(this);
  }
}

void CpuMonitor::OnMessage(Message* msg) {
  int max_cpus = sampler_.GetMaxCpus();
  int current_cpus = sampler_.GetCurrentCpus();
  float process_load = sampler_.GetProcessLoad();
  float system_load = sampler_.GetSystemLoad();
  SignalUpdate(current_cpus, max_cpus, process_load, system_load);

  if (monitor_thread_) {
    monitor_thread_->PostDelayed(period_ms_, this);
  }
}

}  // namespace rtc
