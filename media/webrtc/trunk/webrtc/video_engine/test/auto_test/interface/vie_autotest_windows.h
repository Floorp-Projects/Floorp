/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_WINDOWS_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_WINDOWS_H_

#include "webrtc/engine_configurations.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_window_manager_interface.h"

#include <windows.h>
#define TITLE_LENGTH 1024

// Forward declaration
namespace webrtc {
class CriticalSectionWrapper;
}

class ViEAutoTestWindowManager: public ViEAutoTestWindowManagerInterface
{
public:
    ViEAutoTestWindowManager();
    virtual ~ViEAutoTestWindowManager();
    virtual void* GetWindow1();
    virtual void* GetWindow2();
    virtual int CreateWindows(AutoTestRect window1Size,
                              AutoTestRect window2Size, void* window1Title,
                              void* window2Title);
    virtual int TerminateWindows();
    virtual bool SetTopmostWindow();
protected:
    static bool EventProcess(void* obj);
    bool EventLoop();

private:
    int ViECreateWindow(HWND &hwndMain, int xPos, int yPos, int width,
                        int height, TCHAR* className);
    int ViEDestroyWindow(HWND& hwnd);

    void* _window1;
    void* _window2;

    bool _terminate;
    rtc::scoped_ptr<webrtc::ThreadWrapper> _eventThread;
    webrtc::CriticalSectionWrapper& _crit;
    HWND _hwndMain;
    HWND _hwnd1;
    HWND _hwnd2;

    AutoTestRect _hwnd1Size;
    AutoTestRect _hwnd2Size;
    TCHAR _hwnd1Title[TITLE_LENGTH];
    TCHAR _hwnd2Title[TITLE_LENGTH];

};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_WINDOWS_H_
