/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - External protocol support.
//  - Extended port and address APIs.
//  - Port and address filters.
//  - Windows GQoS functions.
//  - Packet timeout notification.
//  - Dead-or-Alive connection observations.
//  - Transmission of raw RTP/RTCP packets into existing channels.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoENetwork* netw  = VoENetwork::GetInterface(voe);
//  base->Init();
//  int ch = base->CreateChannel();
//  ...
//  netw->SetPeriodicDeadOrAliveStatus(ch, true);
//  ...
//  base->DeleteChannel(ch);
//  base->Terminate();
//  base->Release();
//  netw->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_NETWORK_H
#define WEBRTC_VOICE_ENGINE_VOE_NETWORK_H

#include "common_types.h"

namespace webrtc {

class VoiceEngine;

// VoEConnectionObserver
class WEBRTC_DLLEXPORT VoEConnectionObserver
{
public:
    // This method will be called peridically and deliver dead-or-alive
    // notifications for a specified |channel| when the observer interface
    // has been installed and activated.
    virtual void OnPeriodicDeadOrAlive(const int channel, const bool alive) = 0;

protected:
    virtual ~VoEConnectionObserver() {}
};

// VoENetwork
class WEBRTC_DLLEXPORT VoENetwork
{
public:
    // Factory for the VoENetwork sub-API. Increases an internal
    // reference counter if successful. Returns NULL if the API is not
    // supported or if construction fails.
    static VoENetwork* GetInterface(VoiceEngine* voiceEngine);

    // Releases the VoENetwork sub-API and decreases an internal
    // reference counter. Returns the new reference count. This value should
    // be zero for all sub-API:s before the VoiceEngine object can be safely
    // deleted.
    virtual int Release() = 0;

    // Installs and enables a user-defined external transport protocol for a
    // specified |channel|.
    virtual int RegisterExternalTransport(
        int channel, Transport& transport) = 0;

    // Removes and disables a user-defined external transport protocol for a
    // specified |channel|.
    virtual int DeRegisterExternalTransport(int channel) = 0;

    // The packets received from the network should be passed to this
    // function when external transport is enabled. Note that the data
    // including the RTP-header must also be given to the VoiceEngine.
    virtual int ReceivedRTPPacket(
        int channel, const void* data, unsigned int length) = 0;

    // The packets received from the network should be passed to this
    // function when external transport is enabled. Note that the data
    // including the RTCP-header must also be given to the VoiceEngine.
    virtual int ReceivedRTCPPacket(
        int channel, const void* data, unsigned int length) = 0;

    // Gets the source ports and IP address of incoming packets on a
    // specific |channel|.
    virtual int GetSourceInfo(
        int channel, int& rtpPort, int& rtcpPort, char ipAddr[64]) = 0;

    // Gets the local (host) IP address.
    virtual int GetLocalIP(char ipAddr[64], bool ipv6 = false) = 0;

    // Enables IPv6 for a specified |channel|.
    virtual int EnableIPv6(int channel) = 0;

    // Gets the current IPv6 staus for a specified |channel|.
    virtual bool IPv6IsEnabled(int channel) = 0;

    // Enables a port and IP address filter for incoming packets on a
    // specific |channel|.
    virtual int SetSourceFilter(int channel,
        int rtpPort, int rtcpPort = 0, const char ipAddr[64] = 0) = 0;

    // Gets the current port and IP-address filter for a specified |channel|.
    virtual int GetSourceFilter(
        int channel, int& rtpPort, int& rtcpPort, char ipAddr[64]) = 0;

    // Sets the six-bit Differentiated Services Code Point (DSCP) in the
    // IP header of the outgoing stream for a specific |channel|.
    virtual int SetSendTOS(int channel,
        int DSCP, int priority = -1, bool useSetSockopt = false) = 0;

    // Gets the six-bit DSCP in the IP header of the outgoing stream for
    // a specific channel.
    virtual int GetSendTOS(
        int channel, int& DSCP, int& priority, bool& useSetSockopt) = 0;

    // Sets the Generic Quality of Service (GQoS) service level.
    // The Windows operating system then maps to a Differentiated Services
    // Code Point (DSCP) and to an 802.1p setting. [Windows only]
    virtual int SetSendGQoS(
        int channel, bool enable, int serviceType, int overrideDSCP = 0) = 0;

    // Gets the Generic Quality of Service (GQoS) service level.
    virtual int GetSendGQoS(
        int channel, bool& enabled, int& serviceType, int& overrideDSCP) = 0;

    // Enables or disables warnings that report if packets have not been
    // received in |timeoutSeconds| seconds for a specific |channel|.
    virtual int SetPacketTimeoutNotification(
        int channel, bool enable, int timeoutSeconds = 2) = 0;

    // Gets the current time-out notification status.
    virtual int GetPacketTimeoutNotification(
        int channel, bool& enabled, int& timeoutSeconds) = 0;

    // Installs the observer class implementation for a specified |channel|.
    virtual int RegisterDeadOrAliveObserver(
        int channel, VoEConnectionObserver& observer) = 0;

    // Removes the observer class implementation for a specified |channel|.
    virtual int DeRegisterDeadOrAliveObserver(int channel) = 0;

    // Enables or disables the periodic dead-or-alive callback functionality
    // for a specified |channel|.
    virtual int SetPeriodicDeadOrAliveStatus(
        int channel, bool enable, int sampleTimeSeconds = 2) = 0;

    // Gets the current dead-or-alive notification status.
    virtual int GetPeriodicDeadOrAliveStatus(
        int channel, bool& enabled, int& sampleTimeSeconds) = 0;

    // Handles sending a raw UDP data packet over an existing RTP or RTCP
    // socket.
    virtual int SendUDPPacket(
        int channel, const void* data, unsigned int length,
        int& transmittedBytes, bool useRtcpSocket = false) = 0;

protected:
    VoENetwork() {}
    virtual ~VoENetwork() {}
};

} // namespace webrtc

#endif  //  WEBRTC_VOICE_ENGINE_VOE_NETWORK_H
