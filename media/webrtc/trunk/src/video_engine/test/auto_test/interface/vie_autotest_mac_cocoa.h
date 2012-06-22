/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "engine_configurations.h"

#if defined(COCOA_RENDERING)

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAC_COCOA_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAC_COCOA_H_

#include "vie_autotest_window_manager_interface.h"
#define MAC_COCOA_USE_NSRUNLOOP 1

@class CocoaRenderView;

#import <Cocoa/Cocoa.h>

class ViEAutoTestWindowManager: public ViEAutoTestWindowManagerInterface {
 public:
  ViEAutoTestWindowManager();
  virtual ~ViEAutoTestWindowManager() {}
  virtual void* GetWindow1();
  virtual void* GetWindow2();
  virtual int CreateWindows(AutoTestRect window1Size,
                            AutoTestRect window2Size,
                            void* window1Title,
                            void* window2Title);
  virtual int TerminateWindows();
  virtual bool SetTopmostWindow();

 private:
  CocoaRenderView* _cocoaRenderView1;
  CocoaRenderView* _cocoaRenderView2;
  NSWindow* outWindow1_;
  NSWindow* outWindow2_;
};

@interface AutoTestClass : NSObject {
  int    argc_;
  char** argv_;
  int    result_;
}

-(void)setArgc:(int)argc argv:(char**)argv;
-(int) result;
-(void)autoTestWithArg:(NSObject*)ignored;

@end

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAC_COCOA_H_
#endif  // COCOA_RENDERING
