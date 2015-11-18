/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_WIN_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_WIN_H_

#include "webrtc/system_wrappers/interface/thread_wrapper.h"

#include <windows.h>

#include "webrtc/base/thread_checker.h"

namespace webrtc {

class ThreadWindows : public ThreadWrapper {
 public:
  ThreadWindows(ThreadRunFunction func, void* obj, const char* thread_name);
  ~ThreadWindows() override;

  virtual bool Start() override;
  virtual bool Stop() override;

  bool SetPriority(ThreadPriority priority) override;

 protected:
  virtual void Run();

  static DWORD WINAPI StartThread(void* param);

  ThreadRunFunction const run_function_;
  void* const obj_;
  bool stop_;
  HANDLE thread_;
  const std::string name_;
  rtc::ThreadChecker main_thread_;
};

class ThreadWindowsUI : public ThreadWindows {
 public:
  ThreadWindowsUI(ThreadRunFunction func, void* obj,
		  const char* thread_name) :
  ThreadWindows(func, obj, thread_name),
  hwnd_(nullptr),
  timerid_(0),
  timeout_(0) {
 }

 virtual bool Stop() override;

 /**
  * Request an async callback soon.
  */
 void RequestCallback();

 /**
  * Request a recurring callback.
  */
 bool RequestCallbackTimer(unsigned int milliseconds);

 protected:
  virtual void Run() override;

 private:
  static LRESULT CALLBACK EventWindowProc(HWND, UINT, WPARAM, LPARAM);
  void NativeEventCallback();
  bool InternalInit();

  HWND hwnd_;
  UINT_PTR timerid_;
  unsigned int timeout_;
};


}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_WIN_H_
