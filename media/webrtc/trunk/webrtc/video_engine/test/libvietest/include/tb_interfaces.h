/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_INTERFACES_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_INTERFACES_H_

#include <string>

#include "constructor_magic.h"
#include "common_types.h"
#include "video_engine/include/vie_base.h"
#include "video_engine/include/vie_capture.h"
#include "video_engine/include/vie_codec.h"
#include "video_engine/include/vie_image_process.h"
#include "video_engine/include/vie_network.h"
#include "video_engine/include/vie_render.h"
#include "video_engine/include/vie_rtp_rtcp.h"
#include "video_engine/include/vie_encryption.h"
#include "video_engine/vie_defines.h"

// This class deals with all the tedium of setting up video engine interfaces.
// It does its work in constructor and destructor, so keeping it in scope is
// enough. It also sets up tracing.
class TbInterfaces
{
public:
    // Sets up all interfaces and creates a trace file
    TbInterfaces(const std::string& test_name);
    ~TbInterfaces(void);

    webrtc::VideoEngine* video_engine;
    webrtc::ViEBase* base;
    webrtc::ViECapture* capture;
    webrtc::ViERender* render;
    webrtc::ViERTP_RTCP* rtp_rtcp;
    webrtc::ViECodec* codec;
    webrtc::ViENetwork* network;
    webrtc::ViEImageProcess* image_process;
    webrtc::ViEEncryption* encryption;

    int LastError() {
        return base->LastError();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(TbInterfaces);
};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_INTERFACES_H_
