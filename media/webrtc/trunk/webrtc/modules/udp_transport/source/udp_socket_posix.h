/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_POSIX_H_
#define WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_POSIX_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "condition_variable_wrapper.h"
#include "critical_section_wrapper.h"
#include "udp_socket_wrapper.h"

#define SOCKET_ERROR -1

namespace webrtc {
class UdpSocketPosix : public UdpSocketWrapper
{
public:
    UdpSocketPosix(const WebRtc_Word32 id, UdpSocketManager* mgr,
                   bool ipV6Enable = false);

    virtual ~UdpSocketPosix();

    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    virtual bool SetCallback(CallbackObj obj, IncomingSocketCallback cb);

    virtual bool Bind(const SocketAddress& name);

    virtual bool SetSockopt(WebRtc_Word32 level, WebRtc_Word32 optname,
                            const WebRtc_Word8* optval, WebRtc_Word32 optlen);

    virtual WebRtc_Word32 SetTOS(const WebRtc_Word32 serviceType);

    virtual WebRtc_Word32 SendTo(const WebRtc_Word8* buf, WebRtc_Word32 len,
                                 const SocketAddress& to);

    // Deletes socket in addition to closing it.
    // TODO (hellner): make destructor protected.
    virtual void CloseBlocking();

    virtual SOCKET GetFd() {return _socket;}
    virtual WebRtc_Word32 GetError() {return _error;}

    virtual bool ValidHandle();

    virtual bool SetQos(WebRtc_Word32 /*serviceType*/,
                        WebRtc_Word32 /*tokenRate*/,
                        WebRtc_Word32 /*bucketSize*/,
                        WebRtc_Word32 /*peekBandwith*/,
                        WebRtc_Word32 /*minPolicedSize*/,
                        WebRtc_Word32 /*maxSduSize*/,
                        const SocketAddress& /*stRemName*/,
                        WebRtc_Word32 /*overrideDSCP*/) {return false;}

    bool CleanUp();
    void HasIncoming();
    bool WantsIncoming() {return _wantsIncoming;}
    void ReadyForDeletion();
private:
    friend class UdpSocketManagerPosix;

    WebRtc_Word32 _id;
    IncomingSocketCallback _incomingCb;
    CallbackObj _obj;
    WebRtc_Word32 _error;

    SOCKET _socket;
    UdpSocketManager* _mgr;
    ConditionVariableWrapper* _closeBlockingCompletedCond;
    ConditionVariableWrapper* _readyForDeletionCond;

    bool _closeBlockingActive;
    bool _closeBlockingCompleted;
    bool _readyForDeletion;

    CriticalSectionWrapper* _cs;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_POSIX_H_
