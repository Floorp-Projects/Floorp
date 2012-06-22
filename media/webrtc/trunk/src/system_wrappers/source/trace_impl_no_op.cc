/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "trace.h"

namespace webrtc {

void Trace::CreateTrace()
{
}

void Trace::ReturnTrace()
{
}

WebRtc_Word32 Trace::SetLevelFilter(WebRtc_UWord32 /*filter*/)
{
    return 0;
}

WebRtc_Word32 Trace::LevelFilter(WebRtc_UWord32& /*filter*/)
{
    return 0;
}

WebRtc_Word32 Trace::TraceFile(
    char/*fileName*/[1024])
{
    return -1;
}

WebRtc_Word32 Trace::SetTraceFile(const char* /*fileName*/,
                                  const bool /*addFileCounter*/)
{
    return -1;
}

WebRtc_Word32 Trace::SetTraceCallback(TraceCallback* /*callback*/)
{
    return -1;
}

void Trace::Add(const TraceLevel /*level*/, const TraceModule /*module*/,
                const WebRtc_Word32 /*id*/, const char* /*msg*/, ...)

{
}

} // namespace webrtc
