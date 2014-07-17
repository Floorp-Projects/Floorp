/*
*  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_APP_CAPTURER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_APP_CAPTURER_H_

#include <vector>
#include <string>

#include "webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class DesktopCaptureOptions;

class AppCapturer : public DesktopCapturer {
public:
    typedef webrtc::ProcessId ProcessId;
    struct App {
        ProcessId id;
        // Application Name in UTF-8 encoding.
        std::string title;
    };
    typedef std::vector<App> AppList;

    static AppCapturer* Create(const DesktopCaptureOptions& options);
    static AppCapturer* Create();

    virtual ~AppCapturer() {}

    //AppCapturer Interfaces
    virtual bool GetAppList(AppList* apps) = 0;
    virtual bool SelectApp(ProcessId id) = 0;
    virtual bool BringAppToFront() = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_APP_CAPTURER_H_

