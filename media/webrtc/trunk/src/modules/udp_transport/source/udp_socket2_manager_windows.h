/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET2_MANAGER_WINDOWS_H_
#define WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET2_MANAGER_WINDOWS_H_

#if _MSC_VER > 1000
#pragma once
#endif

#include <winsock2.h>

#include "atomic32_wrapper.h"
#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "list_wrapper.h"
#include "thread_wrapper.h"
#include "udp_socket2_windows.h"
#include "udp_socket_manager_wrapper.h"

#define MAX_IO_BUFF_SIZE 1600

namespace webrtc {
enum IO_OPERATION {
    OP_READ,
    OP_WRITE
};

class UdpSocket2Windows;

// Struct used for all socket I/O operations.
struct PerIoContext {
    WSAOVERLAPPED overlapped;
    char buffer[MAX_IO_BUFF_SIZE];
    WSABUF wsabuf;
    int nTotalBytes;
    int nSentBytes;
    int bytes;
    IO_OPERATION ioOperation;
    SocketAddress from;
    int fromLen;
    // Should be set to true if the I/O context was passed to the system by
    // a thread not controlled by the socket implementation.
    bool ioInitiatedByThreadWrapper;
    // TODO (hellner): Not used. Delete it.
    PerIoContext* pNextFree;
};

struct IoContextPoolItem;
struct IoContextPoolItemPayload
{
    PerIoContext    ioContext;
    IoContextPoolItem* base;
};

struct IoContextPoolItem
{
    // Atomic single linked list entry header.
    SLIST_ENTRY itemEntry;
    // Atomic single linked list payload
    IoContextPoolItemPayload payload;
};

class IoContextPool
{
public:
    IoContextPool();
    virtual ~IoContextPool();
    virtual WebRtc_Word32 Init(WebRtc_UWord32 increaseSize = 128);
    // Re-use an old unused IO context or create a new one.
    virtual PerIoContext* PopIoContext();
    virtual WebRtc_Word32 PushIoContext(PerIoContext* pIoContext);
    virtual inline WebRtc_Word32 GetSize(WebRtc_UWord32* inUse = 0)
    {return _size.Value();}
    virtual WebRtc_Word32 Free();
private:
    // Sample code for use of msfts single linked atomic list can be found here:
    // http://msdn.microsoft.com/en-us/library/ms686962(VS.85).aspx

    // Atomic single linked list head.
    PSLIST_HEADER _pListHead;

    bool _init;
    Atomic32Wrapper _size;
    Atomic32Wrapper _inUse;
};


class UdpSocket2ManagerWindows : public UdpSocketManager
{
public:
    UdpSocket2ManagerWindows();
    virtual ~UdpSocket2ManagerWindows();

    virtual bool Init(WebRtc_Word32 id, WebRtc_UWord8& numOfWorkThreads);
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    virtual bool Start();
    virtual bool Stop();

    virtual inline bool AddSocket(UdpSocketWrapper* s)
    {if(s) return AddSocketPrv(reinterpret_cast<UdpSocket2Windows*>(s));
     return false;}
    virtual bool RemoveSocket(UdpSocketWrapper* s)
    {if(s) return RemoveSocketPrv(reinterpret_cast<UdpSocket2Windows*>(s));
     return false;}

    PerIoContext* PopIoContext(void);
    WebRtc_Word32 PushIoContext(PerIoContext* pIoContext);

private:
    bool StopWorkerThreads();
    bool StartWorkerThreads();
    bool AddSocketPrv(UdpSocket2Windows* s);
    bool RemoveSocketPrv(UdpSocket2Windows* s);

    static WebRtc_UWord32 _numOfActiveManagers;
    static bool _wsaInit;

    WebRtc_Word32 _id;
    CriticalSectionWrapper* _pCrit;
    WebRtc_Word32 _managerNumber;
    volatile bool _stopped;
    bool _init;
    WebRtc_Word32 _numActiveSockets;
    ListWrapper _workerThreadsList;
    EventWrapper* _event;

    HANDLE _ioCompletionHandle;
    IoContextPool _ioContextPool;
};

class UdpSocket2WorkerWindows
{
public:
    UdpSocket2WorkerWindows(HANDLE ioCompletionHandle);
    virtual ~UdpSocket2WorkerWindows();

    virtual bool Start();
    virtual bool Stop();
    virtual WebRtc_Word32 Init();
    virtual void SetNotAlive();
protected:
    static bool Run(ThreadObj obj);
    bool Process();
private:
    HANDLE _ioCompletionHandle;
    ThreadWrapper*_pThread;
    static WebRtc_Word32 _numOfWorkers;
    WebRtc_Word32 _workerNumber;
    volatile bool _stop;
    bool _init;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET2_MANAGER_WINDOWS_H_
