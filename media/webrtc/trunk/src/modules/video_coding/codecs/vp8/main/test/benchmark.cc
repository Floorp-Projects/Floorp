/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "benchmark.h"
#include "testsupport/fileutils.h"
#include "vp8.h"

using namespace webrtc;

VP8Benchmark::VP8Benchmark()
    : Benchmark("VP8Benchmark", "VP8 benchmark over a range of test cases",
                webrtc::test::OutputPath() + "VP8Benchmark.txt", "VP8") {
}

VP8Benchmark::VP8Benchmark(std::string name, std::string description)
    : Benchmark(name, description,
                webrtc::test::OutputPath() + "VP8Benchmark.txt",
                "VP8") {
}

VP8Benchmark::VP8Benchmark(std::string name, std::string description,
                           std::string resultsFileName)
    : Benchmark(name, description, resultsFileName, "VP8") {
}

VideoEncoder* VP8Benchmark::GetNewEncoder() {
    return VP8Encoder::Create();
}

VideoDecoder* VP8Benchmark::GetNewDecoder() {
    return VP8Decoder::Create();
}
