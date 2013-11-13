/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_MANAGER_POSIX_H_
#define WEBRTC_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_MANAGER_POSIX_H_

#include <sys/types.h>
#include <unistd.h>

#include <map>

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/list_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/test/channel_transport/udp_socket_manager_wrapper.h"
#include "webrtc/test/channel_transport/udp_socket_wrapper.h"

namespace webrtc {

class ConditionVariableWrapper;

namespace test {

class UdpSocketPosix;
class UdpSocketManagerPosixImpl;
#define MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX 8

class UdpSocketManagerPosix : public UdpSocketManager
{
public:
    UdpSocketManagerPosix();
    virtual ~UdpSocketManagerPosix();

    virtual bool Init(int32_t id, uint8_t& numOfWorkThreads) OVERRIDE;

    virtual int32_t ChangeUniqueId(const int32_t id) OVERRIDE;

    virtual bool Start() OVERRIDE;
    virtual bool Stop() OVERRIDE;

    virtual bool AddSocket(UdpSocketWrapper* s) OVERRIDE;
    virtual bool RemoveSocket(UdpSocketWrapper* s) OVERRIDE;
private:
    int32_t _id;
    CriticalSectionWrapper* _critSect;
    uint8_t _numberOfSocketMgr;
    uint8_t _incSocketMgrNextTime;
    uint8_t _nextSocketMgrToAssign;
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

    std::map<SOCKET, UdpSocketPosix*> _socketMap;
    ListWrapper _addList;
    ListWrapper _removeList;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_MANAGER_POSIX_H_
