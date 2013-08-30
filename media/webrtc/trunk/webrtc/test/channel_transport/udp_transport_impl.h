/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_CHANNEL_TRANSPORT_UDP_TRANSPORT_IMPL_H_
#define WEBRTC_TEST_CHANNEL_TRANSPORT_UDP_TRANSPORT_IMPL_H_

#include "webrtc/test/channel_transport/udp_socket_wrapper.h"
#include "webrtc/test/channel_transport/udp_transport.h"

namespace webrtc {

class CriticalSectionWrapper;
class RWLockWrapper;

namespace test {

class UdpSocketManager;

class UdpTransportImpl : public UdpTransport
{
public:
    // A factory that returns a wrapped UDP socket or equivalent.
    class SocketFactoryInterface {
    public:
        virtual ~SocketFactoryInterface() {}
        virtual UdpSocketWrapper* CreateSocket(const int32_t id,
                                               UdpSocketManager* mgr,
                                               CallbackObj obj,
                                               IncomingSocketCallback cb,
                                               bool ipV6Enable,
                                               bool disableGQOS) = 0;
    };

    // Constructor, only called by UdpTransport::Create and tests.
    // The constructor takes ownership of the "maker".
    // The constructor does not take ownership of socket_manager.
    UdpTransportImpl(const int32_t id,
                     SocketFactoryInterface* maker,
                     UdpSocketManager* socket_manager);
    virtual ~UdpTransportImpl();

    // UdpTransport functions
    virtual int32_t InitializeSendSockets(
        const char* ipAddr,
        const uint16_t rtpPort,
        const uint16_t rtcpPort = 0);
    virtual int32_t InitializeReceiveSockets(
        UdpTransportData* const packetCallback,
        const uint16_t rtpPort,
        const char* ipAddr = NULL,
        const char* multicastIpAddr = NULL,
        const uint16_t rtcpPort = 0);
    virtual int32_t InitializeSourcePorts(
        const uint16_t rtpPort,
        const uint16_t rtcpPort = 0);
    virtual int32_t SourcePorts(uint16_t& rtpPort, uint16_t& rtcpPort) const;
    virtual int32_t ReceiveSocketInformation(
        char ipAddr[kIpAddressVersion6Length],
        uint16_t& rtpPort,
        uint16_t& rtcpPort,
        char multicastIpAddr[kIpAddressVersion6Length]) const;
    virtual int32_t SendSocketInformation(
        char ipAddr[kIpAddressVersion6Length],
        uint16_t& rtpPort,
        uint16_t& rtcpPort) const;
    virtual int32_t RemoteSocketInformation(
        char ipAddr[kIpAddressVersion6Length],
        uint16_t& rtpPort,
        uint16_t& rtcpPort) const;
    virtual int32_t SetQoS(const bool QoS,
                           const int32_t serviceType,
                           const uint32_t maxBitrate = 0,
                           const int32_t overrideDSCP = 0,
                           const bool audio = false);
    virtual int32_t QoS(bool& QoS, int32_t& serviceType,
                        int32_t& overrideDSCP) const;
    virtual int32_t SetToS(const int32_t DSCP,
                           const bool useSetSockOpt = false);
    virtual int32_t ToS(int32_t& DSCP, bool& useSetSockOpt) const;
    virtual int32_t SetPCP(const int32_t PCP);
    virtual int32_t PCP(int32_t& PCP) const;
    virtual int32_t EnableIpV6();
    virtual bool IpV6Enabled() const;
    virtual int32_t SetFilterIP(
        const char filterIPAddress[kIpAddressVersion6Length]);
    virtual int32_t FilterIP(
        char filterIPAddress[kIpAddressVersion6Length]) const;
    virtual int32_t SetFilterPorts(const uint16_t rtpFilterPort,
                                   const uint16_t rtcpFilterPort);
    virtual int32_t FilterPorts(uint16_t& rtpFilterPort,
                                uint16_t& rtcpFilterPort) const;
    virtual int32_t StartReceiving(
        const uint32_t numberOfSocketBuffers);
    virtual int32_t StopReceiving();
    virtual bool Receiving() const;
    virtual bool SendSocketsInitialized() const;
    virtual bool SourcePortsInitialized() const;
    virtual bool ReceiveSocketsInitialized() const;
    virtual int32_t SendRaw(const int8_t* data,
                            uint32_t length, int32_t isRTCP,
                            uint16_t portnr = 0,
                            const char* ip = NULL);
    virtual int32_t SendRTPPacketTo(const int8_t *data,
                                    uint32_t length,
                                    const SocketAddress& to);
    virtual int32_t SendRTCPPacketTo(const int8_t *data,
                                     uint32_t length,
                                     const SocketAddress& to);
    virtual int32_t SendRTPPacketTo(const int8_t *data,
                                    uint32_t length,
                                    uint16_t rtpPort);
    virtual int32_t SendRTCPPacketTo(const int8_t *data,
                                     uint32_t length,
                                     uint16_t rtcpPort);
    // Transport functions
    virtual int SendPacket(int channel, const void* data, int length);
    virtual int SendRTCPPacket(int channel, const void* data, int length);

    // UdpTransport functions continue.
    virtual int32_t SetSendIP(const char* ipaddr);
    virtual int32_t SetSendPorts(const uint16_t rtpPort,
                                 const uint16_t rtcpPort = 0);

    virtual ErrorCode LastError() const;

    virtual int32_t IPAddressCached(const SocketAddress& address,
                                    char* ip,
                                    uint32_t& ipSize,
                                    uint16_t& sourcePort);

    int32_t Id() const {return _id;}
protected:
    // IncomingSocketCallback signature functions for receiving callbacks from
    // UdpSocketWrapper.
    static void IncomingRTPCallback(CallbackObj obj,
                                    const int8_t* rtpPacket,
                                    int32_t rtpPacketLength,
                                    const SocketAddress* from);
    static void IncomingRTCPCallback(CallbackObj obj,
                                     const int8_t* rtcpPacket,
                                     int32_t rtcpPacketLength,
                                     const SocketAddress* from);

    void CloseSendSockets();
    void CloseReceiveSockets();

    // Update _remoteRTPAddr according to _destPort and _destIP
    void BuildRemoteRTPAddr();
    // Update _remoteRTCPAddr according to _destPortRTCP and _destIP
    void BuildRemoteRTCPAddr();

    void BuildSockaddrIn(uint16_t portnr, const char* ip,
                         SocketAddress& remoteAddr) const;

    ErrorCode BindLocalRTPSocket();
    ErrorCode BindLocalRTCPSocket();

    ErrorCode BindRTPSendSocket();
    ErrorCode BindRTCPSendSocket();

    void IncomingRTPFunction(const int8_t* rtpPacket,
                             int32_t rtpPacketLength,
                             const SocketAddress* from);
    void IncomingRTCPFunction(const int8_t* rtcpPacket,
                              int32_t rtcpPacketLength,
                              const SocketAddress* from);

    bool FilterIPAddress(const SocketAddress* fromAddress);

    bool SetSockOptUsed();

    int32_t EnableQoS(int32_t serviceType, bool audio,
                      uint32_t maxBitrate, int32_t overrideDSCP);

    int32_t DisableQoS();

private:
    void GetCachedAddress(char* ip, uint32_t& ipSize,
                          uint16_t& sourcePort);

    int32_t _id;
    SocketFactoryInterface* _socket_creator;
    // Protects the sockets from being re-configured while receiving packets.
    CriticalSectionWrapper* _crit;
    CriticalSectionWrapper* _critFilter;
    // _packetCallback's critical section.
    CriticalSectionWrapper* _critPacketCallback;
    UdpSocketManager* _mgr;
    ErrorCode _lastError;

    // Remote RTP and RTCP ports.
    uint16_t _destPort;
    uint16_t _destPortRTCP;

    // Local RTP and RTCP ports.
    uint16_t _localPort;
    uint16_t _localPortRTCP;

    // Local port number when the local port for receiving and local port number
    // for sending are not the same.
    uint16_t _srcPort;
    uint16_t _srcPortRTCP;

    // Remote port from which last received packet was sent.
    uint16_t _fromPort;
    uint16_t _fromPortRTCP;

    char _fromIP[kIpAddressVersion6Length];
    char _destIP[kIpAddressVersion6Length];
    char _localIP[kIpAddressVersion6Length];
    char _localMulticastIP[kIpAddressVersion6Length];

    UdpSocketWrapper* _ptrRtpSocket;
    UdpSocketWrapper* _ptrRtcpSocket;

    // Local port when the local port for receiving and local port for sending
    // are not the same.
    UdpSocketWrapper* _ptrSendRtpSocket;
    UdpSocketWrapper* _ptrSendRtcpSocket;

    SocketAddress _remoteRTPAddr;
    SocketAddress _remoteRTCPAddr;

    SocketAddress _localRTPAddr;
    SocketAddress _localRTCPAddr;

    int32_t _tos;
    bool _receiving;
    bool _useSetSockOpt;
    bool _qos;
    int32_t _pcp;
    bool _ipV6Enabled;
    int32_t _serviceType;
    int32_t _overrideDSCP;
    uint32_t _maxBitrate;

    // Cache used by GetCachedAddress(..).
    RWLockWrapper* _cachLock;
    SocketAddress _previousAddress;
    char _previousIP[kIpAddressVersion6Length];
    uint32_t _previousIPSize;
    uint16_t _previousSourcePort;

    SocketAddress _filterIPAddress;
    uint16_t _rtpFilterPort;
    uint16_t _rtcpFilterPort;

    UdpTransportData* _packetCallback;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_CHANNEL_TRANSPORT_UDP_TRANSPORT_IMPL_H_
