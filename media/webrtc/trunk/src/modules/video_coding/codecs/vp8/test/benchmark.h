/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_BENCHMARK_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_BENCHMARK_H_

#include "modules/video_coding/codecs/test_framework/benchmark.h"

class VP8Benchmark : public Benchmark
{
public:
    VP8Benchmark();
    VP8Benchmark(std::string name, std::string description);
    VP8Benchmark(std::string name, std::string description, std::string resultsFileName);

protected:
    virtual webrtc::VideoEncoder* GetNewEncoder();
    virtual webrtc::VideoDecoder* GetNewDecoder();
};

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_BENCHMARK_H_
