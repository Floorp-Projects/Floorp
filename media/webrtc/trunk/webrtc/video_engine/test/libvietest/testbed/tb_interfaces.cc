/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

TbInterfaces::TbInterfaces(const std::string& test_name) :
    video_engine(NULL),
    base(NULL),
    capture(NULL),
    render(NULL),
    rtp_rtcp(NULL),
    codec(NULL),
    network(NULL),
    image_process(NULL)
{
    std::string complete_path =
        webrtc::test::OutputPath() + test_name + "_trace.txt";

    video_engine = webrtc::VideoEngine::Create();
    EXPECT_TRUE(video_engine != NULL);

    EXPECT_EQ(0, video_engine->SetTraceFile(complete_path.c_str()));
    EXPECT_EQ(0, video_engine->SetTraceFilter(webrtc::kTraceAll));

    base = webrtc::ViEBase::GetInterface(video_engine);
    EXPECT_TRUE(base != NULL);

    EXPECT_EQ(0, base->Init());

    capture = webrtc::ViECapture::GetInterface(video_engine);
    EXPECT_TRUE(capture != NULL);

    rtp_rtcp = webrtc::ViERTP_RTCP::GetInterface(video_engine);
    EXPECT_TRUE(rtp_rtcp != NULL);

    render = webrtc::ViERender::GetInterface(video_engine);
    EXPECT_TRUE(render != NULL);

    codec = webrtc::ViECodec::GetInterface(video_engine);
    EXPECT_TRUE(codec != NULL);

    network = webrtc::ViENetwork::GetInterface(video_engine);
    EXPECT_TRUE(network != NULL);

    image_process = webrtc::ViEImageProcess::GetInterface(video_engine);
    EXPECT_TRUE(image_process != NULL);
}

TbInterfaces::~TbInterfaces(void)
{
    EXPECT_EQ(0, image_process->Release());
    image_process = NULL;
    EXPECT_EQ(0, codec->Release());
    codec = NULL;
    EXPECT_EQ(0, capture->Release());
    capture = NULL;
    EXPECT_EQ(0, render->Release());
    render = NULL;
    EXPECT_EQ(0, rtp_rtcp->Release());
    rtp_rtcp = NULL;
    EXPECT_EQ(0, network->Release());
    network = NULL;
    EXPECT_EQ(0, base->Release());
    base = NULL;
    EXPECT_TRUE(webrtc::VideoEngine::Delete(video_engine)) <<
        "Since we have released all interfaces at this point, deletion "
        "should be successful.";
    video_engine = NULL;
}
