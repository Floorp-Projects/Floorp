/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_MANAGER_WINDOWS_H_
#define WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_MANAGER_WINDOWS_H_

#define FD_SETSIZE 1024

#include <winsock2.h>
#include <map>
#include <list>

// Don't change the include order.
// TODO (hellner): all header files should be compilable separately. Fix this.
#include "udp_socket_manager_wrapper.h"
#include "thread_wrapper.h"
#include "critical_section_wrapper.h"

namespace webrtc {
class UdpSocketWindows;

class UdpSocketManagerWindows : public UdpSocketManager
{
public:
    UdpSocketManagerWindows();
    virtual ~UdpSocketManagerWindows();

    virtual bool Init(WebRtc_Word32 id,
                      WebRtc_UWord8& numOfWorkThreads);

    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    virtual bool Start();
    virtual bool Stop();

    virtual bool AddSocket(UdpSocketWrapper* s);
    virtual bool RemoveSocket(UdpSocketWrapper* s);

protected:
    static bool Run(ThreadObj obj);
    bool Process();

private:
    WebRtc_Word32 _id;
    ThreadWrapper* _thread;

    fd_set _readFds;
    fd_set _writeFds;
    fd_set _exceptFds;

    CriticalSectionWrapper* _critSectList;

    std::map<SOCKET, UdpSocketWindows*> _socketMap;
    std::list<UdpSocketWindows*> _addList;
    std::list<SOCKET> _removeList;

    static WebRtc_UWord32 _numOfActiveManagers;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_MANAGER_WINDOWS_H_
