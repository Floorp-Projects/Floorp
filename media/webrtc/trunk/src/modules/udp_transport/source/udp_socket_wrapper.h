/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_WRAPPER_H_
#define WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_WRAPPER_H_

#include "udp_transport.h"

namespace webrtc {
class EventWrapper;
class UdpSocketManager;

#define SOCKET_ERROR_NO_QOS -1000

#ifndef _WIN32
typedef int SOCKET;
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET  (SOCKET)(~0)

#ifndef AF_INET
#define AF_INET 2
#endif

#endif

typedef void* CallbackObj;
typedef void(*IncomingSocketCallback)(CallbackObj obj, const WebRtc_Word8* buf,
                                      WebRtc_Word32 len,
                                      const SocketAddress* from);

class UdpSocketWrapper
{
public:
    virtual ~UdpSocketWrapper();
    static UdpSocketWrapper* CreateSocket(const WebRtc_Word32 id,
                                          UdpSocketManager* mgr,
                                          CallbackObj obj,
                                          IncomingSocketCallback cb,
                                          bool ipV6Enable = false,
                                          bool disableGQOS = false);

    // Set the unique identifier of this class to id.
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id) = 0;

    // Register cb for receiving callbacks when there are incoming packets.
    // Register obj so that it will be passed in calls to cb.
    virtual bool SetCallback(CallbackObj obj, IncomingSocketCallback cb) = 0;

    // Socket to local address specified by name.
    virtual bool Bind(const SocketAddress& name) = 0;

    // Start receiving UDP data.
    virtual bool StartReceiving();
    virtual inline bool StartReceiving(const WebRtc_UWord32 /*receiveBuffers*/)
    {return StartReceiving();}
    // Stop receiving UDP data.
    virtual bool StopReceiving();

    virtual bool ValidHandle() = 0;

    // Set socket options.
    virtual bool SetSockopt(WebRtc_Word32 level, WebRtc_Word32 optname,
                            const WebRtc_Word8* optval,
                            WebRtc_Word32 optlen) = 0;

    // Set TOS for outgoing packets.
    virtual WebRtc_Word32 SetTOS(const WebRtc_Word32 serviceType) = 0;

    // Set 802.1Q PCP field (802.1p) for outgoing VLAN traffic.
    virtual WebRtc_Word32 SetPCP(const WebRtc_Word32 /*pcp*/) {return -1;}

    // Send buf of length len to the address specified by to.
    virtual WebRtc_Word32 SendTo(const WebRtc_Word8* buf, WebRtc_Word32 len,
                                 const SocketAddress& to) = 0;

    virtual void SetEventToNull();

    // Close socket and don't return until completed.
    virtual void CloseBlocking() {}

    // tokenRate is in bit/s. peakBandwidt is in byte/s
    virtual bool SetQos(WebRtc_Word32 serviceType, WebRtc_Word32 tokenRate,
                        WebRtc_Word32 bucketSize, WebRtc_Word32 peekBandwith,
                        WebRtc_Word32 minPolicedSize, WebRtc_Word32 maxSduSize,
                        const SocketAddress &stRemName,
                        WebRtc_Word32 overrideDSCP = 0) = 0;

    virtual WebRtc_UWord32 ReceiveBuffers() {return 0;};

protected:
    UdpSocketWrapper();

    bool _wantsIncoming;
    EventWrapper*  _deleteEvent;

private:
    static bool _initiated;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET_WRAPPER_H_
