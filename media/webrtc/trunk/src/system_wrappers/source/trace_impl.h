/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_TRACE_IMPL_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_TRACE_IMPL_H_

#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/event_wrapper.h"
#include "system_wrappers/interface/file_wrapper.h"
#include "system_wrappers/interface/static_instance.h"
#include "system_wrappers/interface/trace.h"
#include "system_wrappers/interface/thread_wrapper.h"

namespace webrtc {

// TODO (pwestin) WEBRTC_TRACE_MAX_QUEUE needs to be tweaked
// TODO (hellner) the buffer should be close to how much the system can write to
//                file. Increasing the buffer will not solve anything. Sooner or
//                later the buffer is going to fill up anyways.
#if defined(MAC_IPHONE)
    #define WEBRTC_TRACE_MAX_QUEUE  2000
#else
    #define WEBRTC_TRACE_MAX_QUEUE  8000
#endif
#define WEBRTC_TRACE_NUM_ARRAY 2
#define WEBRTC_TRACE_MAX_MESSAGE_SIZE 256
// Total buffer size is WEBRTC_TRACE_NUM_ARRAY (number of buffer partitions) *
// WEBRTC_TRACE_MAX_QUEUE (number of lines per buffer partition) *
// WEBRTC_TRACE_MAX_MESSAGE_SIZE (number of 1 byte charachters per line) =
// 1 or 4 Mbyte

#define WEBRTC_TRACE_MAX_FILE_SIZE 100*1000
// Number of rows that may be written to file. On average 110 bytes per row (max
// 256 bytes per row). So on average 110*100*1000 = 11 Mbyte, max 256*100*1000 =
// 25.6 Mbyte

class TraceImpl : public Trace
{
public:
    virtual ~TraceImpl();

    static TraceImpl* CreateInstance();
    static TraceImpl* GetTrace(const TraceLevel level = kTraceAll);

    WebRtc_Word32 SetTraceFileImpl(const char* fileName,
                                   const bool addFileCounter);
    WebRtc_Word32 TraceFileImpl(
        char fileName[FileWrapper::kMaxFileNameSize]);

    WebRtc_Word32 SetTraceCallbackImpl(TraceCallback* callback);

    void AddImpl(const TraceLevel level, const TraceModule module,
                 const WebRtc_Word32 id, const char* msg);

    bool StopThread();

    bool TraceCheck(const TraceLevel level) const;

protected:
    TraceImpl();

    static TraceImpl* StaticInstance(CountOperation count_operation,
        const TraceLevel level = kTraceAll);

    WebRtc_Word32 AddThreadId(char* traceMessage) const;

    // OS specific implementations
    virtual WebRtc_Word32 AddTime(char* traceMessage,
                                  const TraceLevel level) const = 0;

    virtual WebRtc_Word32 AddBuildInfo(char* traceMessage) const = 0;
    virtual WebRtc_Word32 AddDateTimeInfo(char* traceMessage) const = 0;

    static bool Run(void* obj);
    bool Process();

private:
    friend class Trace;

    WebRtc_Word32 AddLevel(char* szMessage, const TraceLevel level) const;

    WebRtc_Word32 AddModuleAndId(char* traceMessage, const TraceModule module,
                                 const WebRtc_Word32 id) const;

    WebRtc_Word32 AddMessage(char* traceMessage,
                             const char msg[WEBRTC_TRACE_MAX_MESSAGE_SIZE],
                             const WebRtc_UWord16 writtenSoFar) const;

    void AddMessageToList(
        const char traceMessage[WEBRTC_TRACE_MAX_MESSAGE_SIZE],
        const WebRtc_UWord16 length,
        const TraceLevel level);

    bool UpdateFileName(
        const char fileNameUTF8[FileWrapper::kMaxFileNameSize],
        char fileNameWithCounterUTF8[FileWrapper::kMaxFileNameSize],
        const WebRtc_UWord32 newCount) const;

    bool CreateFileName(
        const char fileNameUTF8[FileWrapper::kMaxFileNameSize],
        char fileNameWithCounterUTF8[FileWrapper::kMaxFileNameSize],
        const WebRtc_UWord32 newCount) const;

    void WriteToFile();

    CriticalSectionWrapper* _critsectInterface;
    TraceCallback* _callback;
    WebRtc_UWord32 _rowCountText;
    WebRtc_UWord32 _fileCountText;

    FileWrapper& _traceFile;
    ThreadWrapper& _thread;
    EventWrapper& _event;

    // _critsectArray protects _activeQueue
    CriticalSectionWrapper* _critsectArray;
    WebRtc_UWord16 _nextFreeIdx[WEBRTC_TRACE_NUM_ARRAY];
    TraceLevel _level[WEBRTC_TRACE_NUM_ARRAY][WEBRTC_TRACE_MAX_QUEUE];
    WebRtc_UWord16 _length[WEBRTC_TRACE_NUM_ARRAY][WEBRTC_TRACE_MAX_QUEUE];
    char* _messageQueue[WEBRTC_TRACE_NUM_ARRAY][WEBRTC_TRACE_MAX_QUEUE];
    WebRtc_UWord8 _activeQueue;
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_TRACE_IMPL_H_
