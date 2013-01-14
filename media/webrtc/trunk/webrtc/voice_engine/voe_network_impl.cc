/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_network_impl.h"

#include "channel.h"
#include "critical_section_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc
{

VoENetwork* VoENetwork::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_NETWORK_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = reinterpret_cast<VoiceEngineImpl*>(voiceEngine);
    s->AddRef();
    return s;
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_NETWORK_API

VoENetworkImpl::VoENetworkImpl(voe::SharedData* shared) : _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoENetworkImpl() - ctor");
}

VoENetworkImpl::~VoENetworkImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "~VoENetworkImpl() - dtor");
}

int VoENetworkImpl::RegisterExternalTransport(int channel,
                                              Transport& transport)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetExternalTransport(channel=%d, transport=0x%x)",
                 channel, &transport);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetExternalTransport() failed to locate channel");
        return -1;
    }
    return channelPtr->RegisterExternalTransport(transport);
}

int VoENetworkImpl::DeRegisterExternalTransport(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "DeRegisterExternalTransport(channel=%d)", channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "DeRegisterExternalTransport() failed to locate channel");
        return -1;
    }
    return channelPtr->DeRegisterExternalTransport();
}

int VoENetworkImpl::ReceivedRTPPacket(int channel,
                                      const void* data,
                                      unsigned int length)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ReceivedRTPPacket(channel=%d, length=%u)", channel, length);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if ((length < 12) || (length > 807))
    {
        _shared->SetLastError(VE_INVALID_PACKET, kTraceError,
            "ReceivedRTPPacket() invalid packet length");
        return -1;
    }
    if (NULL == data)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "ReceivedRTPPacket() invalid data vector");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "ReceivedRTPPacket() failed to locate channel");
        return -1;
    }

    if (!channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_INVALID_OPERATION, kTraceError,
            "ReceivedRTPPacket() external transport is not enabled");
        return -1;
    }
    return channelPtr->ReceivedRTPPacket((const WebRtc_Word8*) data, length);
}

int VoENetworkImpl::ReceivedRTCPPacket(int channel, const void* data,
                                       unsigned int length)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ReceivedRTCPPacket(channel=%d, length=%u)", channel, length);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (length < 4)
    {
        _shared->SetLastError(VE_INVALID_PACKET, kTraceError,
            "ReceivedRTCPPacket() invalid packet length");
        return -1;
    }
    if (NULL == data)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "ReceivedRTCPPacket() invalid data vector");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "ReceivedRTCPPacket() failed to locate channel");
        return -1;
    }
    if (!channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_INVALID_OPERATION, kTraceError,
            "ReceivedRTCPPacket() external transport is not enabled");
        return -1;
    }
    return channelPtr->ReceivedRTCPPacket((const WebRtc_Word8*) data, length);
}

int VoENetworkImpl::GetSourceInfo(int channel,
                                  int& rtpPort,
                                  int& rtcpPort,
                                  char ipAddr[64])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetSourceInfo(channel=%d, rtpPort=?, rtcpPort=?, ipAddr[]=?)",
                 channel);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (NULL == ipAddr)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "GetSourceInfo() invalid IP-address buffer");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetSourceInfo() failed to locate channel");
        return -1;
    }
    if (channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "GetSourceInfo() external transport is enabled");
        return -1;
    }
    return channelPtr->GetSourceInfo(rtpPort, rtcpPort, ipAddr);
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "GetSourceInfo() VoE is built for external transport");
    return -1;
#endif
}

int VoENetworkImpl::GetLocalIP(char ipAddr[64], bool ipv6)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetLocalIP(ipAddr[]=?, ipv6=%d)", ipv6);
    IPHONE_NOT_SUPPORTED(_shared->statistics());
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (NULL == ipAddr)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "GetLocalIP() invalid IP-address buffer");
        return -1;
    }

    // Create a temporary socket module to ensure that this method can be
    // called also when no channels are created.
    WebRtc_UWord8 numSockThreads(1);
    UdpTransport* socketPtr =
        UdpTransport::Create(
            -1,
            numSockThreads);
    if (NULL == socketPtr)
    {
        _shared->SetLastError(VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceError,
            "GetLocalIP() failed to create socket module");
        return -1;
    }

    // Use a buffer big enough for IPv6 addresses and initialize it with zeros.
    char localIPAddr[256] = {0};

    if (ipv6)
    {
        char localIP[16];
        if (socketPtr->LocalHostAddressIPV6(localIP) != 0)
        {
            _shared->SetLastError(VE_INVALID_IP_ADDRESS, kTraceError,
                "GetLocalIP() failed to retrieve local IP - 1");
            UdpTransport::Destroy(socketPtr);
            return -1;
        }
        // Convert 128-bit address to character string (a:b:c:d:e:f:g:h)
        sprintf(localIPAddr,
                "%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x"
                "%.2x:%.2x%.2x",
                localIP[0], localIP[1], localIP[2], localIP[3], localIP[4],
                localIP[5], localIP[6], localIP[7], localIP[8], localIP[9],
                localIP[10], localIP[11], localIP[12], localIP[13],
                localIP[14], localIP[15]);
    }
    else
    {
        WebRtc_UWord32 localIP(0);
        // Read local IP (as 32-bit address) from the socket module
        if (socketPtr->LocalHostAddress(localIP) != 0)
        {
            _shared->SetLastError(VE_INVALID_IP_ADDRESS, kTraceError,
                "GetLocalIP() failed to retrieve local IP - 2");
            UdpTransport::Destroy(socketPtr);
            return -1;
        }
        // Convert 32-bit address to character string (x.y.z.w)
        sprintf(localIPAddr, "%d.%d.%d.%d", (int) ((localIP >> 24) & 0x0ff),
                (int) ((localIP >> 16) & 0x0ff),
                (int) ((localIP >> 8) & 0x0ff),
                (int) (localIP & 0x0ff));
    }

    strcpy(ipAddr, localIPAddr);

    UdpTransport::Destroy(socketPtr);

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetLocalIP() => ipAddr=%s", ipAddr);
    return 0;
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "GetLocalIP() VoE is built for external transport");
    return -1;
#endif
}

int VoENetworkImpl::EnableIPv6(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "EnableIPv6(channel=%d)", channel);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED(_shared->statistics());
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "EnableIPv6() failed to locate channel");
        return -1;
    }
    if (channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "EnableIPv6() external transport is enabled");
        return -1;
    }
    return channelPtr->EnableIPv6();
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "EnableIPv6() VoE is built for external transport");
    return -1;
#endif
}

bool VoENetworkImpl::IPv6IsEnabled(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "IPv6IsEnabled(channel=%d)", channel);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return false;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "IPv6IsEnabled() failed to locate channel");
        return false;
    }
    if (channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "IPv6IsEnabled() external transport is enabled");
        return false;
    }
    return channelPtr->IPv6IsEnabled();
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "IPv6IsEnabled() VoE is built for external transport");
    return false;
#endif
}

int VoENetworkImpl::SetSourceFilter(int channel,
                                    int rtpPort,
                                    int rtcpPort,
                                    const char ipAddr[64])
{
    (ipAddr == NULL) ? WEBRTC_TRACE(kTraceApiCall, kTraceVoice,
                                    VoEId(_shared->instance_id(), -1),
                                    "SetSourceFilter(channel=%d, rtpPort=%d,"
                                    " rtcpPort=%d)",
                                    channel, rtpPort, rtcpPort)
                     : WEBRTC_TRACE(kTraceApiCall, kTraceVoice,
                                    VoEId(_shared->instance_id(), -1),
                                    "SetSourceFilter(channel=%d, rtpPort=%d,"
                                    " rtcpPort=%d, ipAddr=%s)",
                                    channel, rtpPort, rtcpPort, ipAddr);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if ((rtpPort < 0) || (rtpPort > 65535))
    {
        _shared->SetLastError(VE_INVALID_PORT_NMBR, kTraceError,
            "SetSourceFilter() invalid RTP port");
        return -1;
    }
    if ((rtcpPort < 0) || (rtcpPort > 65535))
    {
        _shared->SetLastError(VE_INVALID_PORT_NMBR, kTraceError,
            "SetSourceFilter() invalid RTCP port");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetSourceFilter() failed to locate channel");
        return -1;
    }
    if (channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "SetSourceFilter() external transport is enabled");
        return -1;
    }
    return channelPtr->SetSourceFilter(rtpPort, rtcpPort, ipAddr);
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "SetSourceFilter() VoE is built for external transport");
    return -1;
#endif
}

int VoENetworkImpl::GetSourceFilter(int channel,
                                    int& rtpPort,
                                    int& rtcpPort,
                                    char ipAddr[64])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetSourceFilter(channel=%d, rtpPort=?, rtcpPort=?, "
                 "ipAddr[]=?)",
                 channel);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (NULL == ipAddr)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "GetSourceFilter() invalid IP-address buffer");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetSourceFilter() failed to locate channel");
        return -1;
    }
    if (channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "GetSourceFilter() external transport is enabled");
        return -1;
    }
    return channelPtr->GetSourceFilter(rtpPort, rtcpPort, ipAddr);
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "GetSourceFilter() VoE is built for external transport");
    return -1;
#endif
}

int VoENetworkImpl::SetSendTOS(int channel,
                               int DSCP,
                               int priority,
                               bool useSetSockopt)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetSendTOS(channel=%d, DSCP=%d, useSetSockopt=%d)",
                 channel, DSCP, useSetSockopt);

#if !defined(_WIN32) && !defined(WEBRTC_LINUX) && !defined(WEBRTC_MAC)
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceWarning,
        "SetSendTOS() is not supported on this platform");
    return -1;
#endif

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if ((DSCP < 0) || (DSCP > 63))
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetSendTOS() Invalid DSCP value");
        return -1;
    }
#if defined(_WIN32) || defined(WEBRTC_LINUX)
    if ((priority < -1) || (priority > 7))
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetSendTOS() Invalid priority value");
        return -1;
    }
#else
    if (-1 != priority)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetSendTOS() priority not supported");
        return -1;
    }
#endif
#if defined(_WIN32)
    if ((priority >= 0) && useSetSockopt)
    {
        // On Windows, priority and useSetSockopt cannot be combined
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetSendTOS() priority and useSetSockopt conflict");
        return -1;
    }
#endif
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetSendTOS() failed to locate channel");
        return -1;
    }
    if (channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "SetSendTOS() external transport is enabled");
        return -1;
    }
#if defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
    useSetSockopt = true;
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "   force useSetSockopt=true since there is no alternative"
                 " implementation");
#endif

    return channelPtr->SetSendTOS(DSCP, priority, useSetSockopt);
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "SetSendTOS() VoE is built for external transport");
    return -1;
#endif
}

int VoENetworkImpl::GetSendTOS(int channel,
                               int& DSCP,
                               int& priority,
                               bool& useSetSockopt)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetSendTOS(channel=%d)", channel);

#if !defined(_WIN32) && !defined(WEBRTC_LINUX) && !defined(WEBRTC_MAC)
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceWarning,
        "GetSendTOS() is not supported on this platform");
    return -1;
#endif
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetSendTOS() failed to locate channel");
        return -1;
    }
    if (channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "GetSendTOS() external transport is enabled");
        return -1;
    }
    return channelPtr->GetSendTOS(DSCP, priority, useSetSockopt);
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "GetSendTOS() VoE is built for external transport");
    return -1;
#endif
}

int VoENetworkImpl::SetSendGQoS(int channel,
                                bool enable,
                                int serviceType,
                                int overrideDSCP)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetSendGQOS(channel=%d, enable=%d, serviceType=%d,"
                 " overrideDSCP=%d)",
                 channel, (int) enable, serviceType, overrideDSCP);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED(_shared->statistics());
#if !defined(_WIN32)
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceWarning,
        "SetSendGQOS() is not supported on this platform");
    return -1;
#elif !defined(WEBRTC_EXTERNAL_TRANSPORT)
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetSendGQOS() failed to locate channel");
        return -1;
    }
    if (channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "SetSendGQOS() external transport is enabled");
        return -1;
    }
    return channelPtr->SetSendGQoS(enable, serviceType, overrideDSCP);
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "SetSendGQOS() VoE is built for external transport");
    return -1;
#endif
}

int VoENetworkImpl::GetSendGQoS(int channel,
                                bool& enabled,
                                int& serviceType,
                                int& overrideDSCP)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetSendGQOS(channel=%d)", channel);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED(_shared->statistics());
#if !defined(_WIN32)
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceWarning,
        "GetSendGQOS() is not supported on this platform");
    return -1;
#elif !defined(WEBRTC_EXTERNAL_TRANSPORT)
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetSendGQOS() failed to locate channel");
        return -1;
    }
    if (channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "GetSendGQOS() external transport is enabled");
        return -1;
    }
    return channelPtr->GetSendGQoS(enabled, serviceType, overrideDSCP);
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "GetSendGQOS() VoE is built for external transport");
    return -1;
#endif
}

int VoENetworkImpl::SetPacketTimeoutNotification(int channel,
                                                 bool enable,
                                                 int timeoutSeconds)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetPacketTimeoutNotification(channel=%d, enable=%d, "
                 "timeoutSeconds=%d)",
                 channel, (int) enable, timeoutSeconds);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (enable &&
        ((timeoutSeconds < kVoiceEngineMinPacketTimeoutSec) ||
        (timeoutSeconds > kVoiceEngineMaxPacketTimeoutSec)))
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetPacketTimeoutNotification() invalid timeout size");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetPacketTimeoutNotification() failed to locate channel");
        return -1;
    }
    return channelPtr->SetPacketTimeoutNotification(enable, timeoutSeconds);
}

int VoENetworkImpl::GetPacketTimeoutNotification(int channel,
                                                 bool& enabled,
                                                 int& timeoutSeconds)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetPacketTimeoutNotification(channel=%d, enabled=?,"
                 " timeoutSeconds=?)", channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetPacketTimeoutNotification() failed to locate channel");
        return -1;
    }
    return channelPtr->GetPacketTimeoutNotification(enabled, timeoutSeconds);
}

int VoENetworkImpl::RegisterDeadOrAliveObserver(int channel,
                                                VoEConnectionObserver&
                                                observer)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "RegisterDeadOrAliveObserver(channel=%d, observer=0x%x)",
                 channel, &observer);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "RegisterDeadOrAliveObserver() failed to locate channel");
        return -1;
    }
    return channelPtr->RegisterDeadOrAliveObserver(observer);
}

int VoENetworkImpl::DeRegisterDeadOrAliveObserver(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "DeRegisterDeadOrAliveObserver(channel=%d)", channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "DeRegisterDeadOrAliveObserver() failed to locate channel");
        return -1;
    }
    return channelPtr->DeRegisterDeadOrAliveObserver();
}

int VoENetworkImpl::SetPeriodicDeadOrAliveStatus(int channel, bool enable,
                                                 int sampleTimeSeconds)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetPeriodicDeadOrAliveStatus(channel=%d, enable=%d,"
                 " sampleTimeSeconds=%d)",
                 channel, enable, sampleTimeSeconds);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (enable &&
        ((sampleTimeSeconds < kVoiceEngineMinSampleTimeSec) ||
        (sampleTimeSeconds > kVoiceEngineMaxSampleTimeSec)))
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetPeriodicDeadOrAliveStatus() invalid sample time");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetPeriodicDeadOrAliveStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->SetPeriodicDeadOrAliveStatus(enable, sampleTimeSeconds);
}

int VoENetworkImpl::GetPeriodicDeadOrAliveStatus(int channel,
                                                 bool& enabled,
                                                 int& sampleTimeSeconds)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetPeriodicDeadOrAliveStatus(channel=%d, enabled=?,"
                 " sampleTimeSeconds=?)", channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetPeriodicDeadOrAliveStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->GetPeriodicDeadOrAliveStatus(enabled,
                                                    sampleTimeSeconds);
}

int VoENetworkImpl::SendUDPPacket(int channel,
                                  const void* data,
                                  unsigned int length,
                                  int& transmittedBytes,
                                  bool useRtcpSocket)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SendUDPPacket(channel=%d, data=0x%x, length=%u, useRTCP=%d)",
                 channel, data, length, useRtcpSocket);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (NULL == data)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SendUDPPacket() invalid data buffer");
        return -1;
    }
    if (0 == length)
    {
        _shared->SetLastError(VE_INVALID_PACKET, kTraceError,
            "SendUDPPacket() invalid packet size");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SendUDPPacket() failed to locate channel");
        return -1;
    }
    return channelPtr->SendUDPPacket(data,
                                     length,
                                     transmittedBytes,
                                     useRtcpSocket);
#else
    _shared->SetLastError(VE_EXTERNAL_TRANSPORT_ENABLED, kTraceWarning,
        "SendUDPPacket() VoE is built for external transport");
    return -1;
#endif
}

#endif  // WEBRTC_VOICE_ENGINE_NETWORK_API

} // namespace webrtc
