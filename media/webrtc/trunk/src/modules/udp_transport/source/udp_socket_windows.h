/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_WINDOWS_H_
#define WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_WINDOWS_H_

#include <Winsock2.h>

#include "list_wrapper.h"
#include "udp_socket_manager_wrapper.h"
#include "udp_socket_wrapper.h"

namespace webrtc {
class TrafficControlWindows;

class UdpSocketWindows : public UdpSocketWrapper
{
public:
    UdpSocketWindows(const WebRtc_Word32 id, UdpSocketManager* mgr,
                     bool ipV6Enable = false);
    virtual ~UdpSocketWindows();

    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    virtual bool ValidHandle();

    virtual bool SetCallback(CallbackObj obj, IncomingSocketCallback cb);

    virtual bool Bind(const SocketAddress& name);

    virtual bool SetSockopt(WebRtc_Word32 level, WebRtc_Word32 optname,
                            const WebRtc_Word8* optval, WebRtc_Word32 optlen);

    virtual WebRtc_Word32 SendTo(const WebRtc_Word8* buf, WebRtc_Word32 len,
                                 const SocketAddress& to);

    virtual SOCKET GetFd() {return _socket;}
    virtual WebRtc_Word32 GetError() {return _error;}

    virtual bool SetQos(WebRtc_Word32 serviceType, WebRtc_Word32 tokenRate,
                        WebRtc_Word32 bucketSize, WebRtc_Word32 peekBandwith,
                        WebRtc_Word32 minPolicedSize, WebRtc_Word32 maxSduSize,
                        const SocketAddress& stRemName,
                        WebRtc_Word32 overrideDSCP = 0);

    virtual WebRtc_Word32 SetTOS(const WebRtc_Word32 serviceType);

protected:
    void CleanUp();
    void HasIncoming();
    // Socket is free to process pending packets.
    void SetWritable();
    bool IsWritable() {return _notSentPackets.Empty();}
    bool WantsIncoming() {return _wantsIncoming;}

private:
    friend class UdpSocketManagerWindows;

    WebRtc_Word32 CreateFlowSpec(WebRtc_Word32 serviceType,
                                 WebRtc_Word32 tokenRate,
                                 WebRtc_Word32 bucketSize,
                                 WebRtc_Word32 peekBandwith,
                                 WebRtc_Word32 minPolicedSize,
                                 WebRtc_Word32 maxSduSize, FLOWSPEC* f);

    WebRtc_Word32 SetTOSByte(WebRtc_Word32 serviceType, FLOWSPEC* send,
                             FLOWSPEC* recv);

    WebRtc_Word32 _id;
    IncomingSocketCallback _incomingCb;

    CallbackObj _obj;
    bool _qos;
    WebRtc_Word32 _error;

    volatile bool _addedToMgr;

    SocketAddress _remoteAddr;
    SocketAddress _localAddr;
    SOCKET _socket;
    UdpSocketManager* _mgr;

    ListWrapper _notSentPackets;
    bool _terminate;

    // QoS handles.
    HANDLE _clientHandle;
    HANDLE _flowHandle;
    HANDLE _filterHandle;

    // TOS implementation.
    TrafficControlWindows* _gtc;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_WINDOWS_H_
