/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_INTERFACE_PROCESS_THREAD_H_
#define WEBRTC_MODULES_UTILITY_INTERFACE_PROCESS_THREAD_H_

#include "webrtc/typedefs.h"

namespace webrtc {
class Module;

class ProcessThread
{
public:
    static ProcessThread* CreateProcessThread();
    static void DestroyProcessThread(ProcessThread* module);

    virtual int32_t Start() = 0;
    virtual int32_t Stop() = 0;

    virtual int32_t RegisterModule(Module* module) = 0;
    virtual int32_t DeRegisterModule(const Module* module) = 0;
protected:
    virtual ~ProcessThread();
};
}  // namespace webrtc
#endif // WEBRTC_MODULES_UTILITY_INTERFACE_PROCESS_THREAD_H_
