/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "trace_impl.h"

#include <cassert>
#include <string.h> // memset

#ifdef _WIN32
#include "trace_win.h"
#else
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include "trace_posix.h"
#endif // _WIN32

#define KEY_LEN_CHARS 31

#ifdef _WIN32
    #pragma warning(disable:4355)
// VS 2005: Disable warnings for default initialized arrays.
    #pragma warning(disable:4351)
#endif // _WIN32

namespace webrtc {
static WebRtc_UWord32 levelFilter = kTraceDefault;

// Construct On First Use idiom. Avoids "static initialization order fiasco".
TraceImpl* TraceImpl::StaticInstance(CountOperation count_operation,
                                     const TraceLevel level)
{
    // Sanities to avoid taking lock unless absolutely necessary (for
    // performance reasons).
    // count_operation == kAddRefNoCreate implies that a message will be
    // written to file.
    if((level != kTraceAll) && (count_operation == kAddRefNoCreate))
    {
        if(!(level & levelFilter))
        {
            return NULL;
        }
    }
    TraceImpl* impl =
        GetStaticInstance<TraceImpl>(count_operation);
    return impl;
}

TraceImpl* TraceImpl::GetTrace(const TraceLevel level)
{
    return StaticInstance(kAddRefNoCreate, level);
}

TraceImpl* TraceImpl::CreateInstance()
{
#if defined(_WIN32)
    return new TraceWindows();
#else
    return new TracePosix();
#endif
}

TraceImpl::TraceImpl()
    : _critsectInterface(CriticalSectionWrapper::CreateCriticalSection()),
      _callback(NULL),
      _rowCountText(0),
      _fileCountText(0),
      _traceFile(*FileWrapper::Create()),
      _thread(*ThreadWrapper::CreateThread(TraceImpl::Run, this,
                                           kHighestPriority, "Trace")),
      _event(*EventWrapper::Create()),
      _critsectArray(CriticalSectionWrapper::CreateCriticalSection()),
      _nextFreeIdx(),
      _level(),
      _length(),
      _messageQueue(),
      _activeQueue(0)
{
    _nextFreeIdx[0] = 0;
    _nextFreeIdx[1] = 0;

    unsigned int tid = 0;
    _thread.Start(tid);

    for(int m = 0; m < WEBRTC_TRACE_NUM_ARRAY; m++)
    {
        for(int n = 0; n < WEBRTC_TRACE_MAX_QUEUE; n++)
        {
            _messageQueue[m][n] = new
                char[WEBRTC_TRACE_MAX_MESSAGE_SIZE];
        }
    }
}

bool TraceImpl::StopThread()
{
    // Release the worker thread so that it can flush any lingering messages.
    _event.Set();

    // Allow 10 ms for pending messages to be flushed out.
    // TODO (hellner): why not use condition variables to do this? Or let the
    //                 worker thread die and let this thread flush remaining
    //                 messages?
#ifdef _WIN32
    Sleep(10);
#else
    timespec t;
    t.tv_sec = 0;
    t.tv_nsec = 10*1000000;
    nanosleep(&t,NULL);
#endif

    _thread.SetNotAlive();
    // Make sure the thread finishes as quickly as possible (instead of having
    // to wait for the timeout).
    _event.Set();
    bool stopped = _thread.Stop();

    CriticalSectionScoped lock(_critsectInterface);
    _traceFile.Flush();
    _traceFile.CloseFile();
    return stopped;
}

TraceImpl::~TraceImpl()
{
    StopThread();
    delete &_event;
    delete &_traceFile;
    delete &_thread;
    delete _critsectInterface;
    delete _critsectArray;

    for(int m = 0; m < WEBRTC_TRACE_NUM_ARRAY; m++)
    {
        for(int n = 0; n < WEBRTC_TRACE_MAX_QUEUE; n++)
        {
            delete [] _messageQueue[m][n];
        }
    }
}

WebRtc_Word32 TraceImpl::AddThreadId(char* traceMessage) const {
  WebRtc_UWord32 threadId = ThreadWrapper::GetThreadId();
  // Messages is 12 characters.
  return sprintf(traceMessage, "%10u; ", threadId);
}

WebRtc_Word32 TraceImpl::AddLevel(char* szMessage, const TraceLevel level) const
{
    switch (level)
    {
        case kTraceStateInfo:
            sprintf (szMessage, "STATEINFO ; ");
            break;
        case kTraceWarning:
            sprintf (szMessage, "WARNING   ; ");
            break;
        case kTraceError:
            sprintf (szMessage, "ERROR     ; ");
            break;
        case kTraceCritical:
            sprintf (szMessage, "CRITICAL  ; ");
            break;
        case kTraceInfo:
            sprintf (szMessage, "DEBUGINFO ; ");
            break;
        case kTraceModuleCall:
            sprintf (szMessage, "MODULECALL; ");
            break;
        case kTraceMemory:
            sprintf (szMessage, "MEMORY    ; ");
            break;
        case kTraceTimer:
            sprintf (szMessage, "TIMER     ; ");
            break;
        case kTraceStream:
            sprintf (szMessage, "STREAM    ; ");
            break;
        case kTraceApiCall:
            sprintf (szMessage, "APICALL   ; ");
            break;
        case kTraceDebug:
            sprintf (szMessage, "DEBUG     ; ");
            break;
        default:
            assert(false);
            return 0;
    }
    // All messages are 12 characters.
    return 12;
}

WebRtc_Word32 TraceImpl::AddModuleAndId(char* traceMessage,
                                        const TraceModule module,
                                        const WebRtc_Word32 id) const
{
    // Use long int to prevent problems with different definitions of
    // WebRtc_Word32.
    // TODO (hellner): is this actually a problem? If so, it should be better to
    //                 clean up WebRtc_Word32
    const long int idl = id;
    if(idl != -1)
    {
        const unsigned long int idEngine = id>>16;
        const unsigned long int idChannel = id & 0xffff;

        switch (module)
        {
            case kTraceVoice:
                sprintf(traceMessage, "       VOICE:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceVideo:
                sprintf(traceMessage, "       VIDEO:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceUtility:
                sprintf(traceMessage, "     UTILITY:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceRtpRtcp:
                sprintf(traceMessage, "    RTP/RTCP:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceTransport:
                sprintf(traceMessage, "   TRANSPORT:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceAudioCoding:
                sprintf(traceMessage, "AUDIO CODING:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceSrtp:
                sprintf(traceMessage, "        SRTP:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceAudioMixerServer:
                sprintf(traceMessage, " AUDIO MIX/S:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceAudioMixerClient:
                sprintf(traceMessage, " AUDIO MIX/C:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceVideoCoding:
                sprintf(traceMessage, "VIDEO CODING:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceVideoMixer:
                // Print sleep time and API call
                sprintf(traceMessage, "   VIDEO MIX:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceFile:
                sprintf(traceMessage, "        FILE:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceAudioProcessing:
                sprintf(traceMessage, "  AUDIO PROC:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceAudioDevice:
                sprintf(traceMessage, "AUDIO DEVICE:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceVideoRenderer:
                sprintf(traceMessage, "VIDEO RENDER:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceVideoCapture:
                sprintf(traceMessage, "VIDEO CAPTUR:%5ld %5ld;", idEngine,
                        idChannel);
                break;
            case kTraceVideoPreocessing:
                sprintf(traceMessage, "  VIDEO PROC:%5ld %5ld;", idEngine,
                        idChannel);
                break;
        }
    } else {
        switch (module)
        {
            case kTraceVoice:
                sprintf (traceMessage, "       VOICE:%11ld;", idl);
                break;
            case kTraceVideo:
                sprintf (traceMessage, "       VIDEO:%11ld;", idl);
                break;
            case kTraceUtility:
                sprintf (traceMessage, "     UTILITY:%11ld;", idl);
                break;
            case kTraceRtpRtcp:
                sprintf (traceMessage, "    RTP/RTCP:%11ld;", idl);
                break;
            case kTraceTransport:
                sprintf (traceMessage, "   TRANSPORT:%11ld;", idl);
                break;
            case kTraceAudioCoding:
                sprintf (traceMessage, "AUDIO CODING:%11ld;", idl);
                break;
            case kTraceSrtp:
                sprintf (traceMessage, "        SRTP:%11ld;", idl);
                break;
            case kTraceAudioMixerServer:
                sprintf (traceMessage, " AUDIO MIX/S:%11ld;", idl);
                break;
            case kTraceAudioMixerClient:
                sprintf (traceMessage, " AUDIO MIX/C:%11ld;", idl);
                break;
            case kTraceVideoCoding:
                sprintf (traceMessage, "VIDEO CODING:%11ld;", idl);
                break;
            case kTraceVideoMixer:
                sprintf (traceMessage, "   VIDEO MIX:%11ld;", idl);
                break;
            case kTraceFile:
                sprintf (traceMessage, "        FILE:%11ld;", idl);
                break;
            case kTraceAudioProcessing:
                sprintf (traceMessage, "  AUDIO PROC:%11ld;", idl);
                break;
            case kTraceAudioDevice:
                sprintf (traceMessage, "AUDIO DEVICE:%11ld;", idl);
                break;
            case kTraceVideoRenderer:
                sprintf (traceMessage, "VIDEO RENDER:%11ld;", idl);
                break;
            case kTraceVideoCapture:
                sprintf (traceMessage, "VIDEO CAPTUR:%11ld;", idl);
                break;
            case kTraceVideoPreocessing:
                sprintf (traceMessage, "  VIDEO PROC:%11ld;", idl);
                break;
        }
    }
    // All messages are 25 characters.
    return 25;
}

WebRtc_Word32 TraceImpl::SetTraceFileImpl(const char* fileNameUTF8,
                                          const bool addFileCounter)
{
    CriticalSectionScoped lock(_critsectInterface);

    _traceFile.Flush();
    _traceFile.CloseFile();

    if(fileNameUTF8)
    {
        if(addFileCounter)
        {
            _fileCountText = 1;

            char fileNameWithCounterUTF8[FileWrapper::kMaxFileNameSize];
            CreateFileName(fileNameUTF8, fileNameWithCounterUTF8,
                           _fileCountText);
            if(_traceFile.OpenFile(fileNameWithCounterUTF8, false, false,
                                   true) == -1)
            {
                return -1;
            }
        }else {
            _fileCountText = 0;
            if(_traceFile.OpenFile(fileNameUTF8, false, false, true) == -1)
            {
                return -1;
            }
        }
    }
    _rowCountText = 0;
    return 0;
}

WebRtc_Word32 TraceImpl::TraceFileImpl(
    char fileNameUTF8[FileWrapper::kMaxFileNameSize])
{
    CriticalSectionScoped lock(_critsectInterface);
    return _traceFile.FileName(fileNameUTF8, FileWrapper::kMaxFileNameSize);
}

WebRtc_Word32 TraceImpl::SetTraceCallbackImpl(TraceCallback* callback)
{
    CriticalSectionScoped lock(_critsectInterface);
    _callback = callback;
    return 0;
}

WebRtc_Word32 TraceImpl::AddMessage(
    char* traceMessage,
    const char msg[WEBRTC_TRACE_MAX_MESSAGE_SIZE],
    const WebRtc_UWord16 writtenSoFar) const

{
    int length = 0;
    if(writtenSoFar >= WEBRTC_TRACE_MAX_MESSAGE_SIZE)
    {
        return -1;
    }
    // - 2 to leave room for newline and NULL termination
#ifdef _WIN32
    length = _snprintf(traceMessage,
                       WEBRTC_TRACE_MAX_MESSAGE_SIZE - writtenSoFar - 2,
                       "%s",msg);
    if(length < 0)
    {
        length = WEBRTC_TRACE_MAX_MESSAGE_SIZE - writtenSoFar - 2;
        traceMessage[length] = 0;
    }
#else
    length = snprintf(traceMessage,
                      WEBRTC_TRACE_MAX_MESSAGE_SIZE-writtenSoFar-2, "%s",msg);
    if(length < 0 || length > WEBRTC_TRACE_MAX_MESSAGE_SIZE-writtenSoFar - 2)
    {
        length = WEBRTC_TRACE_MAX_MESSAGE_SIZE - writtenSoFar - 2;
        traceMessage[length] = 0;
    }
#endif
    // Length with NULL termination.
    return length+1;
}

void TraceImpl::AddMessageToList(
    const char traceMessage[WEBRTC_TRACE_MAX_MESSAGE_SIZE],
    const WebRtc_UWord16 length,
    const TraceLevel level) {
#ifdef WEBRTC_DIRECT_TRACE
    if (_callback) {
      _callback->Print(level, traceMessage, length);
    }
    return;
#endif

    CriticalSectionScoped lock(_critsectArray);

    if(_nextFreeIdx[_activeQueue] >= WEBRTC_TRACE_MAX_QUEUE)
    {
        if( ! _traceFile.Open() &&
            !_callback)
        {
            // Keep at least the last 1/4 of old messages when not logging.
            // TODO (hellner): isn't this redundant. The user will make it known
            //                 when to start logging. Why keep messages before
            //                 that?
            for(int n = 0; n < WEBRTC_TRACE_MAX_QUEUE/4; n++)
            {
                const int lastQuarterOffset = (3*WEBRTC_TRACE_MAX_QUEUE/4);
                memcpy(_messageQueue[_activeQueue][n],
                       _messageQueue[_activeQueue][n + lastQuarterOffset],
                       WEBRTC_TRACE_MAX_MESSAGE_SIZE);
            }
            _nextFreeIdx[_activeQueue] = WEBRTC_TRACE_MAX_QUEUE/4;
        } else {
            // More messages are being written than there is room for in the
            // buffer. Drop any new messages.
            // TODO (hellner): its probably better to drop old messages instead
            //                 of new ones. One step further: if this happens
            //                 it's due to writing faster than what can be
            //                 processed. Maybe modify the filter at this point.
            //                 E.g. turn of STREAM.
            return;
        }
    }

    WebRtc_UWord16 idx = _nextFreeIdx[_activeQueue];
    _nextFreeIdx[_activeQueue]++;

    _level[_activeQueue][idx] = level;
    _length[_activeQueue][idx] = length;
    memcpy(_messageQueue[_activeQueue][idx], traceMessage, length);

    if(_nextFreeIdx[_activeQueue] == WEBRTC_TRACE_MAX_QUEUE-1)
    {
        // Logging more messages than can be worked off. Log a warning.
        const char warning_msg[] = "WARNING MISSING TRACE MESSAGES\n";
        _level[_activeQueue][_nextFreeIdx[_activeQueue]] = kTraceWarning;
        _length[_activeQueue][_nextFreeIdx[_activeQueue]] = strlen(warning_msg);
        memcpy(_messageQueue[_activeQueue][_nextFreeIdx[_activeQueue]],
               warning_msg, _length[_activeQueue][idx]);
        _nextFreeIdx[_activeQueue]++;
    }
}

bool TraceImpl::Run(void* obj)
{
    return static_cast<TraceImpl*>(obj)->Process();
}

bool TraceImpl::Process()
{
    if(_event.Wait(1000) == kEventSignaled)
    {
        if(_traceFile.Open() || _callback)
        {
            // File mode (not calback mode).
            WriteToFile();
        }
    } else {
        _traceFile.Flush();
    }
    return true;
}

void TraceImpl::WriteToFile()
{
    WebRtc_UWord8 localQueueActive = 0;
    WebRtc_UWord16 localNextFreeIdx = 0;

    // There are two buffer. One for reading (for writing to file) and one for
    // writing (for storing new messages). Let new messages be posted to the
    // unused buffer so that the current buffer can be flushed safely.
    {
        CriticalSectionScoped lock(_critsectArray);
        localNextFreeIdx = _nextFreeIdx[_activeQueue];
        _nextFreeIdx[_activeQueue] = 0;
        localQueueActive = _activeQueue;
        if(_activeQueue == 0)
        {
            _activeQueue = 1;
        } else
        {
            _activeQueue = 0;
        }
    }
    if(localNextFreeIdx == 0)
    {
        return;
    }

    CriticalSectionScoped lock(_critsectInterface);

    for(WebRtc_UWord16 idx = 0; idx <localNextFreeIdx; idx++)
    {
        TraceLevel localLevel = _level[localQueueActive][idx];
        if(_callback)
        {
            _callback->Print(localLevel, _messageQueue[localQueueActive][idx],
                             _length[localQueueActive][idx]);
        }
        if(_traceFile.Open())
        {
            if(_rowCountText > WEBRTC_TRACE_MAX_FILE_SIZE)
            {
                // wrap file
                _rowCountText = 0;
                _traceFile.Flush();

                if(_fileCountText == 0)
                {
                    _traceFile.Rewind();
                } else
                {
                    char oldFileName[FileWrapper::kMaxFileNameSize];
                    char newFileName[FileWrapper::kMaxFileNameSize];

                    // get current name
                    _traceFile.FileName(oldFileName,
                                        FileWrapper::kMaxFileNameSize);
                    _traceFile.CloseFile();

                    _fileCountText++;

                    UpdateFileName(oldFileName, newFileName, _fileCountText);

                    if(_traceFile.OpenFile(newFileName, false, false,
                                           true) == -1)
                    {
                        return;
                    }
                }
            }
            if(_rowCountText ==  0)
            {
                char message[WEBRTC_TRACE_MAX_MESSAGE_SIZE + 1];
                WebRtc_Word32 length = AddDateTimeInfo(message);
                if(length != -1)
                {
                    message[length] = 0;
                    message[length-1] = '\n';
                    _traceFile.Write(message, length);
                    _rowCountText++;
                }
                length = AddBuildInfo(message);
                if(length != -1)
                {
                    message[length+1] = 0;
                    message[length] = '\n';
                    message[length-1] = '\n';
                    _traceFile.Write(message, length+1);
                    _rowCountText++;
                    _rowCountText++;
                }
            }
            WebRtc_UWord16 length = _length[localQueueActive][idx];
            _messageQueue[localQueueActive][idx][length] = 0;
            _messageQueue[localQueueActive][idx][length-1] = '\n';
            _traceFile.Write(_messageQueue[localQueueActive][idx], length);
            _rowCountText++;
        }
    }
}

void TraceImpl::AddImpl(const TraceLevel level, const TraceModule module,
                        const WebRtc_Word32 id,
                        const char msg[WEBRTC_TRACE_MAX_MESSAGE_SIZE])
{
    if (TraceCheck(level))
    {
        char traceMessage[WEBRTC_TRACE_MAX_MESSAGE_SIZE];
        char* meassagePtr = traceMessage;

        WebRtc_Word32 len = 0;
        WebRtc_Word32 ackLen = 0;

        len = AddLevel(meassagePtr, level);
        if(len == -1)
        {
            return;
        }
        meassagePtr += len;
        ackLen += len;

        len = AddTime(meassagePtr, level);
        if(len == -1)
        {
            return;
        }
        meassagePtr += len;
        ackLen += len;

        len = AddModuleAndId(meassagePtr, module, id);
        if(len == -1)
        {
            return;
        }
        meassagePtr += len;
        ackLen += len;

        len = AddThreadId(meassagePtr);
        if(len < 0)
        {
            return;
        }
        meassagePtr += len;
        ackLen += len;

        len = AddMessage(meassagePtr, msg, (WebRtc_UWord16)ackLen);
        if(len == -1)
        {
            return;
        }
        ackLen += len;
        AddMessageToList(traceMessage,(WebRtc_UWord16)ackLen, level);

        // Make sure that messages are written as soon as possible.
        _event.Set();
    }
}

bool TraceImpl::TraceCheck(const TraceLevel level) const
{
    return (level & levelFilter)? true:false;
}

bool TraceImpl::UpdateFileName(
    const char fileNameUTF8[FileWrapper::kMaxFileNameSize],
    char fileNameWithCounterUTF8[FileWrapper::kMaxFileNameSize],
    const WebRtc_UWord32 newCount) const
{
    WebRtc_Word32 length = (WebRtc_Word32)strlen(fileNameUTF8);
    if(length < 0)
    {
        return false;
    }

    WebRtc_Word32 lengthWithoutFileEnding = length-1;
    while(lengthWithoutFileEnding > 0)
    {
        if(fileNameUTF8[lengthWithoutFileEnding] == '.')
        {
            break;
        } else {
            lengthWithoutFileEnding--;
        }
    }
    if(lengthWithoutFileEnding == 0)
    {
        lengthWithoutFileEnding = length;
    }
    WebRtc_Word32 lengthTo_ = lengthWithoutFileEnding - 1;
    while(lengthTo_ > 0)
    {
        if(fileNameUTF8[lengthTo_] == '_')
        {
            break;
        } else {
            lengthTo_--;
        }
    }

    memcpy(fileNameWithCounterUTF8, fileNameUTF8, lengthTo_);
    sprintf(fileNameWithCounterUTF8+lengthTo_, "_%lu%s",
            static_cast<long unsigned int> (newCount),
            fileNameUTF8+lengthWithoutFileEnding);
    return true;
}

bool TraceImpl::CreateFileName(
    const char fileNameUTF8[FileWrapper::kMaxFileNameSize],
    char fileNameWithCounterUTF8[FileWrapper::kMaxFileNameSize],
    const WebRtc_UWord32 newCount) const
{
    WebRtc_Word32 length = (WebRtc_Word32)strlen(fileNameUTF8);
    if(length < 0)
    {
        return false;
    }

    WebRtc_Word32 lengthWithoutFileEnding = length-1;
    while(lengthWithoutFileEnding > 0)
    {
        if(fileNameUTF8[lengthWithoutFileEnding] == '.')
        {
            break;
        }else
        {
            lengthWithoutFileEnding--;
        }
    }
    if(lengthWithoutFileEnding == 0)
    {
        lengthWithoutFileEnding = length;
    }
    memcpy(fileNameWithCounterUTF8, fileNameUTF8, lengthWithoutFileEnding);
    sprintf(fileNameWithCounterUTF8+lengthWithoutFileEnding, "_%lu%s",
            static_cast<long unsigned int> (newCount),
            fileNameUTF8+lengthWithoutFileEnding);
    return true;
}

void Trace::CreateTrace()
{
    TraceImpl::StaticInstance(kAddRef);
}

void Trace::ReturnTrace()
{
    TraceImpl::StaticInstance(kRelease);
}

WebRtc_Word32 Trace::SetLevelFilter(WebRtc_UWord32 filter)
{
    levelFilter = filter;
    return 0;
}

WebRtc_Word32 Trace::LevelFilter(WebRtc_UWord32& filter)
{
    filter = levelFilter;
    return 0;
}

WebRtc_Word32 Trace::TraceFile(char fileName[FileWrapper::kMaxFileNameSize])
{
    TraceImpl* trace = TraceImpl::GetTrace();
    if(trace)
    {
        int retVal = trace->TraceFileImpl(fileName);
        ReturnTrace();
        return retVal;
    }
    return -1;
}

WebRtc_Word32 Trace::SetTraceFile(const char* fileName,
                                  const bool addFileCounter)
{
    TraceImpl* trace = TraceImpl::GetTrace();
    if(trace)
    {
        int retVal = trace->SetTraceFileImpl(fileName, addFileCounter);
        ReturnTrace();
        return retVal;
    }
    return -1;
}

WebRtc_Word32 Trace::SetTraceCallback(TraceCallback* callback)
{
    TraceImpl* trace = TraceImpl::GetTrace();
    if(trace)
    {
        int retVal = trace->SetTraceCallbackImpl(callback);
        ReturnTrace();
        return retVal;
    }
    return -1;
}

void Trace::Add(const TraceLevel level, const TraceModule module,
                const WebRtc_Word32 id, const char* msg, ...)

{
    TraceImpl* trace = TraceImpl::GetTrace(level);
    if(trace)
    {
        if(trace->TraceCheck(level))
        {
            char tempBuff[WEBRTC_TRACE_MAX_MESSAGE_SIZE];
            char* buff = 0;
            if(msg)
            {
                va_list args;
                va_start(args, msg);
#ifdef _WIN32
                _vsnprintf(tempBuff,WEBRTC_TRACE_MAX_MESSAGE_SIZE-1,msg,args);
#else
                vsnprintf(tempBuff,WEBRTC_TRACE_MAX_MESSAGE_SIZE-1,msg,args);
#endif
                va_end(args);
                buff = tempBuff;
            }
            trace->AddImpl(level, module, id, buff);
        }
        ReturnTrace();
    }
}

} // namespace webrtc
