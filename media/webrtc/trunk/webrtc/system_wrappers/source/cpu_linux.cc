/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/source/cpu_linux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace webrtc {

CpuLinux::CpuLinux()
    : old_busy_time_(0),
      old_idle_time_(0),
      old_busy_time_multi_(NULL),
      old_idle_time_multi_(NULL),
      idle_array_(NULL),
      busy_array_(NULL),
      result_array_(NULL),
      num_cores_(0) {
  const int result = GetNumCores();
  if (result != -1) {
    num_cores_ = result;
    old_busy_time_multi_ = new long long[num_cores_];
    memset(old_busy_time_multi_, 0, sizeof(long long) * num_cores_);
    old_idle_time_multi_ = new long long[num_cores_];
    memset(old_idle_time_multi_, 0, sizeof(long long) * num_cores_);
    idle_array_ = new long long[num_cores_];
    memset(idle_array_, 0, sizeof(long long) * num_cores_);
    busy_array_ = new long long[num_cores_];
    memset(busy_array_, 0, sizeof(long long) * num_cores_);
    result_array_ = new WebRtc_UWord32[num_cores_];

    GetData(old_busy_time_, old_idle_time_, busy_array_, idle_array_);
  }
}

CpuLinux::~CpuLinux() {
  delete [] old_busy_time_multi_;
  delete [] old_idle_time_multi_;
  delete [] idle_array_;
  delete [] busy_array_;
  delete [] result_array_;
}

WebRtc_Word32 CpuLinux::CpuUsage() {
  WebRtc_UWord32 dummy = 0;
  WebRtc_UWord32* dummy_array = NULL;
  return CpuUsageMultiCore(dummy, dummy_array);
}

WebRtc_Word32 CpuLinux::CpuUsageMultiCore(WebRtc_UWord32& num_cores,
                                          WebRtc_UWord32*& core_array) {
  core_array = result_array_;
  num_cores = num_cores_;
  long long busy = 0;
  long long idle = 0;
  if (GetData(busy, idle, busy_array_, idle_array_) != 0)
    return -1;

  long long delta_busy = busy - old_busy_time_;
  long long delta_idle = idle - old_idle_time_;
  old_busy_time_ = busy;
  old_idle_time_ = idle;

  int ret_val = -1;
  if (delta_busy + delta_idle == 0) {
    ret_val = 0;
  } else {
    ret_val = (int)(100 * (delta_busy) / (delta_busy + delta_idle));
  }

  if (core_array == NULL) {
    return ret_val;
  }

  for (WebRtc_UWord32 i = 0; i < num_cores_; ++i) {
    delta_busy = busy_array_[i] - old_busy_time_multi_[i];
    delta_idle = idle_array_[i] - old_idle_time_multi_[i];
    old_busy_time_multi_[i] = busy_array_[i];
    old_idle_time_multi_[i] = idle_array_[i];
    if (delta_busy + delta_idle == 0) {
      core_array[i] = 0;
    } else {
      core_array[i] = (int)(100 * (delta_busy) / (delta_busy + delta_idle));
    }
  }
  return ret_val;
}

int CpuLinux::GetData(long long& busy, long long& idle, long long*& busy_array,
                      long long*& idle_array) {
  FILE* fp = fopen("/proc/stat", "r");
  if (!fp) {
    return -1;
  }

  char line[100];
  if (fgets(line, 100, fp) == NULL) {
    fclose(fp);
    return -1;
  }
  char first_word[100];
  if (sscanf(line, "%s ", first_word) != 1) {
    fclose(fp);
    return -1;
  }
  if (strncmp(first_word, "cpu", 3) != 0) {
    fclose(fp);
    return -1;
  }
  char s_user[100];
  char s_nice[100];
  char s_system[100];
  char s_idle[100];
  if (sscanf(line, "%s %s %s %s %s ",
             first_word, s_user, s_nice, s_system, s_idle) != 5) {
    fclose(fp);
    return -1;
  }
  long long luser = atoll(s_user);
  long long lnice = atoll(s_nice);
  long long lsystem = atoll(s_system);
  long long lidle = atoll(s_idle);

  busy = luser + lnice + lsystem;
  idle = lidle;
  for (WebRtc_UWord32 i = 0; i < num_cores_; ++i) {
    if (fgets(line, 100, fp) == NULL) {
      fclose(fp);
      return -1;
    }
    if (sscanf(line, "%s %s %s %s %s ", first_word, s_user, s_nice, s_system,
               s_idle) != 5) {
      fclose(fp);
      return -1;
    }
    luser = atoll(s_user);
    lnice = atoll(s_nice);
    lsystem = atoll(s_system);
    lidle = atoll(s_idle);
    busy_array[i] = luser + lnice + lsystem;
    idle_array[i] = lidle;
  }
  fclose(fp);
  return 0;
}

int CpuLinux::GetNumCores() {
  FILE* fp = fopen("/proc/stat", "r");
  if (!fp) {
    return -1;
  }
  // Skip first line
  char line[100];
  if (!fgets(line, 100, fp)) {
    fclose(fp);
    return -1;
  }
  int num_cores = -1;
  char first_word[100];
  do {
    num_cores++;
    if (fgets(line, 100, fp)) {
      if (sscanf(line, "%s ", first_word) != 1) {
        first_word[0] = '\0';
      }
    } else {
      break;
    }
  } while (strncmp(first_word, "cpu", 3) == 0);
  fclose(fp);
  return num_cores;
}

} // namespace webrtc
