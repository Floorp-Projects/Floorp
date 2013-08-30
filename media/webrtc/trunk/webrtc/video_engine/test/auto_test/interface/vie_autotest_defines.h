/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// vie_autotest_defines.h
//


#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_DEFINES_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_DEFINES_H_

#include <cassert>
#include <stdarg.h>
#include <stdio.h>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/system_wrappers/interface/sleep.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined (WEBRTC_ANDROID)
#include <android/log.h>
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#endif

// Choose how to log
//#define VIE_LOG_TO_FILE
#define VIE_LOG_TO_STDOUT

// Choose one way to test error
#define VIE_ASSERT_ERROR

#define VIE_LOG_FILE_NAME "ViEAutotestLog.txt"

#undef RGB
#define RGB(r,g,b) r|g<<8|b<<16

enum {
  kAutoTestSleepTimeMs = 5000
};

struct AutoTestSize {
  unsigned int width;
  unsigned int height;
  AutoTestSize() :
    width(0), height(0) {
  }
  AutoTestSize(unsigned int iWidth, unsigned int iHeight) :
    width(iWidth), height(iHeight) {
  }
};

struct AutoTestOrigin {
  unsigned int x;
  unsigned int y;
  AutoTestOrigin() :
    x(0), y(0) {
  }
  AutoTestOrigin(unsigned int iX, unsigned int iY) :
    x(iX), y(iY) {
  }
};

struct AutoTestRect {
  AutoTestSize size;
  AutoTestOrigin origin;
  AutoTestRect() :
    size(), origin() {
  }

  AutoTestRect(unsigned int iX, unsigned int iY, unsigned int iWidth, unsigned int iHeight) :
    size(iX, iY), origin(iWidth, iHeight) {
  }

  void Copy(AutoTestRect iRect) {
    origin.x = iRect.origin.x;
    origin.y = iRect.origin.y;
    size.width = iRect.size.width;
    size.height = iRect.size.height;
  }
};

// ============================================

class ViETest {
 public:
  static int Init() {
#ifdef VIE_LOG_TO_FILE
    log_file_ = fopen(VIE_LOG_FILE_NAME, "w+t");
#else
    log_file_ = NULL;
#endif
    log_str_ = new char[kMaxLogSize];
    memset(log_str_, 0, kMaxLogSize);
    return 0;
  }

  static int Terminate() {
    if (log_file_) {
      fclose(log_file_);
      log_file_ = NULL;
    }
    if (log_str_) {
      delete[] log_str_;
      log_str_ = NULL;
    }
    return 0;
  }

  static void Log(const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    memset(log_str_, 0, kMaxLogSize);
    vsprintf(log_str_, fmt, va);
    va_end(va);

    WriteToSuitableOutput(log_str_);
  }

  // Writes to a suitable output, depending on platform and log mode.
  static void WriteToSuitableOutput(const char* message) {
#ifdef VIE_LOG_TO_FILE
    if (log_file_)
    {
      fwrite(log_str_, 1, strlen(log_str_), log_file_);
      fwrite("\n", 1, 1, log_file_);
      fflush(log_file_);
    }
#endif
#ifdef VIE_LOG_TO_STDOUT
#if WEBRTC_ANDROID
    __android_log_write(ANDROID_LOG_DEBUG, "*WebRTCN*", log_str_);
#else
    printf("%s\n", log_str_);
#endif
#endif
  }

  // Deprecated(phoglund): Prefer to use googletest macros in all cases
  // except the custom call case.
  static int TestError(bool expr, const char* fmt, ...) {
    if (!expr) {
      va_list va;
      va_start(va, fmt);
      memset(log_str_, 0, kMaxLogSize);
      vsprintf(log_str_, fmt, va);
#ifdef WEBRTC_ANDROID
      __android_log_write(ANDROID_LOG_ERROR, "*WebRTCN*", log_str_);
#endif
      WriteToSuitableOutput(log_str_);
      va_end(va);

      AssertError(log_str_);
      return 1;
    }
    return 0;
  }

  // Returns a suitable path to write trace and result files to.
  // You should always use this when you want to write output files.
  // The returned path is guaranteed to end with a path separator.
  // This function may be run at any time during the program's execution.
  // Implemented in vie_autotest.cc
  static std::string GetResultOutputPath();

private:
  static void AssertError(const char* message) {
#ifdef VIE_ASSERT_ERROR
    assert(false);
#endif
  }

  static FILE* log_file_;
  enum {
    kMaxLogSize = 512
  };
  static char* log_str_;
};

#define AutoTestSleep webrtc::SleepMs

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_DEFINES_H_
