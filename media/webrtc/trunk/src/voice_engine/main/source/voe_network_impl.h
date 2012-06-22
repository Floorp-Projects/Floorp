/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_NETWORK_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_NETWORK_IMPL_H

#include "voe_network.h"

#include "ref_count.h"
#include "shared_data.h"


namespace webrtc
{

class VoENetworkImpl: public VoENetwork,
                      public voe::RefCount
{
public:
    virtual int Release();

    virtual int RegisterExternalTransport(int channel, Transport& transport);

    virtual int DeRegisterExternalTransport(int channel);

    virtual int ReceivedRTPPacket(int channel,
                                  const void* data,
                                  unsigned int length);

    virtual int ReceivedRTCPPacket(int channel,
                                   const void* data,
                                   unsigned int length);

    virtual int GetSourceInfo(int channel,
                              int& rtpPort,
                              int& rtcpPort,
                              char ipAddr[64]);

    virtual int GetLocalIP(char ipAddr[64], bool ipv6 = false);

    virtual int EnableIPv6(int channel);

    virtual bool IPv6IsEnabled(int channel);

    virtual int SetSourceFilter(int channel,
                                int rtpPort,
                                int rtcpPort,
                                const char ipAddr[64] = 0);

    virtual int GetSourceFilter(int channel,
                                int& rtpPort,
                                int& rtcpPort,
                                char ipAddr[64]);

    virtual int SetSendTOS(int channel,
                           int DSCP,
                           int priority = -1,
                           bool useSetSockopt = false);

    virtual int GetSendTOS(int channel,
                           int& DSCP,
                           int& priority,
                           bool& useSetSockopt);

    virtual int SetSendGQoS(int channel,
                            bool enable,
                            int serviceType,
                            int overrideDSCP);

    virtual int GetSendGQoS(int channel,
                            bool& enabled,
                            int& serviceType,
                            int& overrideDSCP);

    virtual int SetPacketTimeoutNotification(int channel,
                                             bool enable,
                                             int timeoutSeconds = 2);

    virtual int GetPacketTimeoutNotification(int channel,
                                             bool& enabled,
                                             int& timeoutSeconds);

    virtual int RegisterDeadOrAliveObserver(int channel,
                                            VoEConnectionObserver& observer);

    virtual int DeRegisterDeadOrAliveObserver(int channel);

    virtual int SetPeriodicDeadOrAliveStatus(int channel,
                                             bool enable,
                                             int sampleTimeSeconds = 2);

    virtual int GetPeriodicDeadOrAliveStatus(int channel,
                                             bool& enabled,
                                             int& sampleTimeSeconds);

    virtual int SendUDPPacket(int channel,
                              const void* data,
                              unsigned int length,
                              int& transmittedBytes,
                              bool useRtcpSocket = false);

protected:
    VoENetworkImpl(voe::SharedData* shared);
    virtual ~VoENetworkImpl();
private:
    voe::SharedData* _shared;
};

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_NETWORK_IMPL_H
