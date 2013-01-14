/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UDP_TRANSPORT_INTERFACE_UDP_TRANSPORT_H_
#define WEBRTC_MODULES_UDP_TRANSPORT_INTERFACE_UDP_TRANSPORT_H_

#include "common_types.h"
#include "module.h"
#include "typedefs.h"

/*
 *  WARNING
 *  This code is not use in production/testing and might have security issues
 *  for example: http://code.google.com/p/webrtc/issues/detail?id=1028
 *
 */


#define SS_MAXSIZE 128
#define SS_ALIGNSIZE (sizeof (WebRtc_UWord64))
#define SS_PAD1SIZE  (SS_ALIGNSIZE - sizeof(WebRtc_Word16))
#define SS_PAD2SIZE  (SS_MAXSIZE - (sizeof(WebRtc_Word16) + SS_PAD1SIZE +\
                                    SS_ALIGNSIZE))

// BSD requires use of HAVE_STRUCT_SOCKADDR_SA_LEN
namespace webrtc {
struct SocketAddressIn
{
    // sin_family should be either AF_INET (IPv4) or AF_INET6 (IPv6)
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
    WebRtc_Word8      sin_length;
    WebRtc_Word8      sin_family;
#else
    WebRtc_Word16     sin_family;
#endif
    WebRtc_UWord16    sin_port;
    WebRtc_UWord32    sin_addr;
    WebRtc_Word8      sin_zero[8];
};

struct Version6InAddress
{
    union
    {
        WebRtc_UWord8     _s6_u8[16];
        WebRtc_UWord32    _s6_u32[4];
        WebRtc_UWord64    _s6_u64[2];
    } Version6AddressUnion;
};

struct SocketAddressInVersion6
{
    // sin_family should be either AF_INET (IPv4) or AF_INET6 (IPv6)
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
    WebRtc_Word8      sin_length;
    WebRtc_Word8      sin_family;
#else
    WebRtc_Word16     sin_family;
#endif
    // Transport layer port number.
    WebRtc_UWord16 sin6_port;
    // IPv6 traffic class and flow info or ip4 address.
    WebRtc_UWord32 sin6_flowinfo;
    // IPv6 address
    struct Version6InAddress sin6_addr;
    // Set of interfaces for a scope.
    WebRtc_UWord32 sin6_scope_id;
};

struct SocketAddressStorage
{
    // sin_family should be either AF_INET (IPv4) or AF_INET6 (IPv6)
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
    WebRtc_Word8   sin_length;
    WebRtc_Word8   sin_family;
#else
    WebRtc_Word16  sin_family;
#endif
    WebRtc_Word8   __ss_pad1[SS_PAD1SIZE];
    WebRtc_UWord64 __ss_align;
    WebRtc_Word8   __ss_pad2[SS_PAD2SIZE];
};

struct SocketAddress
{
    union
    {
        struct SocketAddressIn _sockaddr_in;
        struct SocketAddressInVersion6 _sockaddr_in6;
        struct SocketAddressStorage _sockaddr_storage;
    };
};

// Callback class that receives packets from UdpTransport.
class UdpTransportData
{
public:
    virtual ~UdpTransportData()  {};

    virtual void IncomingRTPPacket(const WebRtc_Word8* incomingRtpPacket,
                                   const WebRtc_Word32 rtpPacketLength,
                                   const char* fromIP,
                                   const WebRtc_UWord16 fromPort) = 0;

    virtual void IncomingRTCPPacket(const WebRtc_Word8* incomingRtcpPacket,
                                    const WebRtc_Word32 rtcpPacketLength,
                                    const char* fromIP,
                                    const WebRtc_UWord16 fromPort) = 0;
};


class UdpTransport : public Module, public Transport
{
public:
    enum
    {
        kIpAddressVersion6Length = 64,
        kIpAddressVersion4Length = 16
    };
    enum ErrorCode
    {
        kNoSocketError            = 0,
        kFailedToBindPort         = 1,
        kIpAddressInvalid         = 2,
        kAddressInvalid           = 3,
        kSocketInvalid            = 4,
        kPortInvalid              = 5,
        kTosInvalid               = 6,
        kMulticastAddressInvalid  = 7,
        kQosError                 = 8,
        kSocketAlreadyInitialized = 9,
        kIpVersion6Error          = 10,
        FILTER_ERROR              = 11,
        kStartReceiveError        = 12,
        kStopReceiveError         = 13,
        kCannotFindLocalIp        = 14,
        kTosError                 = 16,
        kNotInitialized           = 17,
        kPcpError                 = 18
    };

    // Factory method. Constructor disabled.
    static UdpTransport* Create(const WebRtc_Word32 id,
                                WebRtc_UWord8& numSocketThreads);
    static void Destroy(UdpTransport* module);

    // Prepares the class for sending RTP packets to ipAddr:rtpPort and RTCP
    // packets to ipAddr:rtpPort+1 if rtcpPort is zero. Otherwise to
    // ipAddr:rtcpPort.
    virtual WebRtc_Word32 InitializeSendSockets(
        const char* ipAddr,
        const WebRtc_UWord16 rtpPort,
        const WebRtc_UWord16 rtcpPort = 0) = 0;

    // Register packetCallback for receiving incoming packets. Set the local
    // RTP port to rtpPort. Bind local IP address to ipAddr. If ipAddr is NULL
    // bind to local IP ANY. Set the local rtcp port to rtcpPort or rtpPort + 1
    // if rtcpPort is 0.
    virtual WebRtc_Word32 InitializeReceiveSockets(
        UdpTransportData* const packetCallback,
        const WebRtc_UWord16 rtpPort,
        const char* ipAddr = NULL,
        const char* multicastIpAddr = NULL,
        const WebRtc_UWord16 rtcpPort = 0) = 0;

    // Set local RTP port to rtpPort and RTCP port to rtcpPort or rtpPort + 1 if
    // rtcpPort is 0. These ports will be used for sending instead of the local
    // ports set by InitializeReceiveSockets(..).
    virtual WebRtc_Word32 InitializeSourcePorts(
        const WebRtc_UWord16 rtpPort,
        const WebRtc_UWord16 rtcpPort = 0) = 0;

    // Retrieve local ports used for sending if other than the ports specified
    // by InitializeReceiveSockets(..). rtpPort is set to the RTP port.
    // rtcpPort is set to the RTCP port.
    virtual WebRtc_Word32 SourcePorts(WebRtc_UWord16& rtpPort,
                                      WebRtc_UWord16& rtcpPort) const = 0;

    // Set ipAddr to the IP address that is currently being listened on. rtpPort
    // to the RTP port listened to. rtcpPort to the RTCP port listened on.
    // multicastIpAddr to the multicast IP address group joined (the address
    // is NULL terminated).
    virtual WebRtc_Word32 ReceiveSocketInformation(
        char ipAddr[kIpAddressVersion6Length],
        WebRtc_UWord16& rtpPort,
        WebRtc_UWord16& rtcpPort,
        char multicastIpAddr[kIpAddressVersion6Length]) const = 0;

    // Set ipAddr to the IP address being sent from. rtpPort to the local RTP
    // port used for sending and rtcpPort to the local RTCP port used for
    // sending.
    virtual WebRtc_Word32 SendSocketInformation(
        char ipAddr[kIpAddressVersion6Length],
        WebRtc_UWord16& rtpPort,
        WebRtc_UWord16& rtcpPort) const = 0;

    // Put the IP address, RTP port and RTCP port from the last received packet
    // into ipAddr, rtpPort and rtcpPort respectively.
    virtual WebRtc_Word32 RemoteSocketInformation(
        char ipAddr[kIpAddressVersion6Length],
        WebRtc_UWord16& rtpPort,
        WebRtc_UWord16& rtcpPort) const = 0;

    // Enable/disable quality of service if QoS is true or false respectively.
    // Set the type of service to serviceType, max bitrate in kbit/s to
    // maxBitrate and override DSCP if overrideDSCP is not 0.
    // Note: Must be called both InitializeSendSockets() and
    // InitializeReceiveSockets() has been called.
    virtual WebRtc_Word32 SetQoS(const bool QoS,
                                 const WebRtc_Word32 serviceType,
                                 const WebRtc_UWord32 maxBitrate = 0,
                                 const WebRtc_Word32 overrideDSCP = 0,
                                 const bool audio = false) = 0;

    // Set QoS to true if quality of service has been turned on. If QoS is true,
    // also set serviceType to type of service and overrideDSCP to override
    // DSCP.
    virtual WebRtc_Word32 QoS(bool& QoS,
                              WebRtc_Word32& serviceType,
                              WebRtc_Word32& overrideDSCP) const = 0;

    // Set type of service.
    virtual WebRtc_Word32 SetToS(const WebRtc_Word32 DSCP,
                                 const bool useSetSockOpt = false) = 0;

    // Get type of service configuration.
    virtual WebRtc_Word32 ToS(WebRtc_Word32& DSCP,
                              bool& useSetSockOpt) const = 0;

    // Set Priority Code Point (IEEE 802.1Q)
    // Note: for Linux this function will set the priority for the socket,
    // which then can be mapped to a PCP value with vconfig.
    virtual WebRtc_Word32 SetPCP(const WebRtc_Word32 PCP) = 0;

    // Get Priority Code Point
    virtual WebRtc_Word32 PCP(WebRtc_Word32& PCP) const = 0;

    // Enable IPv6.
    // Note: this API must be called before any call to
    // InitializeReceiveSockets() or InitializeSendSockets(). It is not
    // possible to go back to IPv4 (default) after this call.
    virtual WebRtc_Word32 EnableIpV6() = 0;

    // Return true if IPv6 has been enabled.
    virtual bool IpV6Enabled() const = 0;

    // Only allow packets received from filterIPAddress to be processed.
    // Note: must be called after EnableIPv6(), if IPv6 is used.
    virtual WebRtc_Word32 SetFilterIP(
        const char filterIPAddress[kIpAddressVersion6Length]) = 0;

    // Write the filter IP address (if any) to filterIPAddress.
    virtual WebRtc_Word32 FilterIP(
        char filterIPAddress[kIpAddressVersion6Length]) const = 0;

    // Only allow RTP packets from rtpFilterPort and RTCP packets from
    // rtcpFilterPort be processed.
    // Note: must be called after EnableIPv6(), if IPv6 is used.
    virtual WebRtc_Word32 SetFilterPorts(
        const WebRtc_UWord16 rtpFilterPort,
        const WebRtc_UWord16 rtcpFilterPort) = 0;

    // Set rtpFilterPort to the filter RTP port and rtcpFilterPort to the
    // filter RTCP port (if filtering based on port is enabled).
    virtual WebRtc_Word32 FilterPorts(WebRtc_UWord16& rtpFilterPort,
                                      WebRtc_UWord16& rtcpFilterPort) const = 0;

    // Set the number of buffers that the socket implementation may use for
    // receiving packets to numberOfSocketBuffers. I.e. the number of packets
    // that can be received in parallell.
    // Note: this API only has effect on Windows.
    virtual WebRtc_Word32 StartReceiving(
        const WebRtc_UWord32 numberOfSocketBuffers) = 0;

    // Stop receive incoming packets.
    virtual WebRtc_Word32 StopReceiving() = 0;

    // Return true incoming packets are received.
    virtual bool Receiving() const = 0;

    // Return true if send sockets have been initialized.
    virtual bool SendSocketsInitialized() const = 0;

    // Return true if local ports for sending has been set.
    virtual bool SourcePortsInitialized() const = 0;

    // Return true if receive sockets have been initialized.
    virtual bool ReceiveSocketsInitialized() const = 0;

    // Send data with size length to ip:portnr. The same port as the set
    // with InitializeSendSockets(..) is used if portnr is 0. The same IP
    // address as set with InitializeSendSockets(..) is used if ip is NULL.
    // If isRTCP is true the port used will be the RTCP port.
    virtual WebRtc_Word32 SendRaw(const WebRtc_Word8* data,
                                  WebRtc_UWord32 length,
                                  WebRtc_Word32 isRTCP,
                                  WebRtc_UWord16 portnr = 0,
                                  const char* ip = NULL) = 0;

    // Send RTP data with size length to the address specified by to.
    virtual WebRtc_Word32 SendRTPPacketTo(const WebRtc_Word8* data,
                                          WebRtc_UWord32 length,
                                          const SocketAddress& to) = 0;


    // Send RTCP data with size length to the address specified by to.
    virtual WebRtc_Word32 SendRTCPPacketTo(const WebRtc_Word8* data,
                                           WebRtc_UWord32 length,
                                           const SocketAddress& to) = 0;

    // Send RTP data with size length to ip:rtpPort where ip is the ip set by
    // the InitializeSendSockets(..) call.
    virtual WebRtc_Word32 SendRTPPacketTo(const WebRtc_Word8* data,
                                          WebRtc_UWord32 length,
                                          WebRtc_UWord16 rtpPort) = 0;


    // Send RTCP data with size length to ip:rtcpPort where ip is the ip set by
    // the InitializeSendSockets(..) call.
    virtual WebRtc_Word32 SendRTCPPacketTo(const WebRtc_Word8* data,
                                           WebRtc_UWord32 length,
                                           WebRtc_UWord16 rtcpPort) = 0;

    // Set the IP address to which packets are sent to ipaddr.
    virtual WebRtc_Word32 SetSendIP(
        const char ipaddr[kIpAddressVersion6Length]) = 0;

    // Set the send RTP and RTCP port to rtpPort and rtcpPort respectively.
    virtual WebRtc_Word32 SetSendPorts(const WebRtc_UWord16 rtpPort,
                                       const WebRtc_UWord16 rtcpPort = 0) = 0;

    // Retreive the last registered error code.
    virtual ErrorCode LastError() const = 0;

    // Put the local IPv4 address in localIP.
    // Note: this API is for IPv4 only.
    static WebRtc_Word32 LocalHostAddress(WebRtc_UWord32& localIP);

    // Put the local IP6 address in localIP.
    // Note: this API is for IPv6 only.
    static WebRtc_Word32 LocalHostAddressIPV6(char localIP[16]);

    // Return a copy of hostOrder (host order) in network order.
    static WebRtc_UWord16 Htons(WebRtc_UWord16 hostOrder);

    // Return a copy of hostOrder (host order) in network order.
    static WebRtc_UWord32 Htonl(WebRtc_UWord32 hostOrder);

    // Return IPv4 address in ip as 32 bit integer.
    static WebRtc_UWord32 InetAddrIPV4(const char* ip);

    // Convert the character string src into a network address structure in
    // the af address family and put it in dst.
    // Note: same functionality as inet_pton(..)
    static WebRtc_Word32 InetPresentationToNumeric(WebRtc_Word32 af,
                                                   const char* src,
                                                   void* dst);

    // Set ip and sourcePort according to address. As input parameter ipSize
    // is the length of ip. As output parameter it's the number of characters
    // written to ip (not counting the '\0' character).
    // Note: this API is only implemented on Windows and Linux.
    static WebRtc_Word32 IPAddress(const SocketAddress& address,
                                   char* ip,
                                   WebRtc_UWord32& ipSize,
                                   WebRtc_UWord16& sourcePort);

    // Set ip and sourcePort according to address. As input parameter ipSize
    // is the length of ip. As output parameter it's the number of characters
    // written to ip (not counting the '\0' character).
    // Note: this API is only implemented on Windows and Linux.
    // Additional note: this API caches the address of the last call to it. If
    // address is likley to be the same for multiple calls it may be beneficial
    // to call this API instead of IPAddress().
    virtual WebRtc_Word32 IPAddressCached(const SocketAddress& address,
                                          char* ip,
                                          WebRtc_UWord32& ipSize,
                                          WebRtc_UWord16& sourcePort) = 0;

    // Return true if ipaddr is a valid IP address.
    // If ipV6 is false ipaddr is interpreted as an IPv4 address otherwise it
    // is interptreted as IPv6.
    static bool IsIpAddressValid(const char* ipaddr, const bool ipV6);
};
} // namespace webrtc

#endif // WEBRTC_MODULES_UDP_TRANSPORT_INTERFACE_UDP_TRANSPORT_H_
