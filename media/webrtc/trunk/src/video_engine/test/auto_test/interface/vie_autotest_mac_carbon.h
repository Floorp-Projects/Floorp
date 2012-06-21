/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "EngineConfigurations.h"

#if defined(CARBON_RENDERING)
#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAC_CARBON_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAC_CARBON_H_

#include "vie_autotest_window_manager_interface.h"

// #define HIVIEWREF_MODE 1

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

class ViEAutoTestWindowManager: public ViEAutoTestWindowManagerInterface
{
public:
    ViEAutoTestWindowManager();
    virtual ~ViEAutoTestWindowManager();
    virtual void* GetWindow1();
    virtual void* GetWindow2();
    virtual int CreateWindows(AutoTestRect window1Size,
                              AutoTestRect window2Size, char* window1Title,
                              char* window2Title);
    virtual int TerminateWindows();
    virtual bool SetTopmostWindow();

    // event handler static methods
static pascal OSStatus HandleWindowEvent (EventHandlerCallRef nextHandler,
    EventRef theEvent, void* userData);
static pascal OSStatus HandleHIViewEvent (EventHandlerCallRef nextHandler,
    EventRef theEvent, void* userData);
private:
    WindowRef* _carbonWindow1;
    WindowRef* _carbonWindow2;
    HIViewRef* _hiView1;
    HIViewRef* _hiView2;

    EventHandlerRef _carbonWindow1EventHandlerRef;
    EventHandlerRef _carbonWindow2EventHandlerRef;
    EventHandlerRef _carbonHIView1EventHandlerRef;
    EventHandlerRef _carbonHIView2EventHandlerRef;

};

@interface AutoTestClass : NSObject
{
}

-(void)autoTestWithArg:(NSString*)answerFile;
@end

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAC_CARBON_H_
#endif CARBON_RENDERING
