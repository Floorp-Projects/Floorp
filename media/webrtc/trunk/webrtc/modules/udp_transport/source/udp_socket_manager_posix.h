/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_MANAGER_POSIX_H_
#define WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_MANAGER_POSIX_H_

#include <sys/types.h>
#include <unistd.h>

#include "critical_section_wrapper.h"
#include "list_wrapper.h"
#include "map_wrapper.h"
#include "thread_wrapper.h"
#include "udp_socket_manager_wrapper.h"
#include "udp_socket_wrapper.h"

#define MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX 8

namespace webrtc {

class ConditionVariableWrapper;
class UdpSocketManagerPosixImpl;

class UdpSocketManagerPosix : public UdpSocketManager
{
public:
    UdpSocketManagerPosix();
    virtual ~UdpSocketManagerPosix();

    virtual bool Init(WebRtc_Word32 id,
                      WebRtc_UWord8& numOfWorkThreads);

    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    virtual bool Start();
    virtual bool Stop();

    virtual bool AddSocket(UdpSocketWrapper* s);
    virtual bool RemoveSocket(UdpSocketWrapper* s);
private:
    WebRtc_Word32 _id;
    CriticalSectionWrapper* _critSect;
    WebRtc_UWord8 _numberOfSocketMgr;
    WebRtc_UWord8 _incSocketMgrNextTime;
    WebRtc_UWord8 _nextSocketMgrToAssign;
    UdpSocketManagerPosixImpl* _socketMgr[MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX];
};

class UdpSocketManagerPosixImpl
{
public:
    UdpSocketManagerPosixImpl();
    virtual ~UdpSocketManagerPosixImpl();

    virtual bool Start();
    virtual bool Stop();

    virtual bool AddSocket(UdpSocketWrapper* s);
    virtual bool RemoveSocket(UdpSocketWrapper* s);

protected:
    static bool Run(ThreadObj obj);
    bool Process();
    void UpdateSocketMap();

private:
    ThreadWrapper* _thread;
    CriticalSectionWrapper* _critSectList;

    fd_set _readFds;

    MapWrapper _socketMap;
    ListWrapper _addList;
    ListWrapper _removeList;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_MANAGER_POSIX_H_
