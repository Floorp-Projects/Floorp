/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_NETWORK_H_
#define WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_NETWORK_H_

// This sub-API supports the following functionalities:
//  - Configuring send and receive addresses.
//  - External transport support.
//  - Port and address filters.
//  - Windows GQoS functions and ToS functions.
//  - Packet timeout notification.
//  - Dead‐or‐Alive connection observations.

#include "common_types.h"

namespace webrtc {

class Transport;
class VideoEngine;

// This enumerator describes VideoEngine packet timeout states.
enum ViEPacketTimeout {
  NoPacket = 0,
  PacketReceived = 1
};

// This class declares an abstract interface for a user defined observer. It is
// up to the VideoEngine user to implement a derived class which implements the
// observer class. The observer is registered using RegisterObserver() and
// deregistered using DeregisterObserver().
class WEBRTC_DLLEXPORT ViENetworkObserver {
 public:
  // This method will be called periodically delivering a dead‐or‐alive
  // decision for a specified channel.
  virtual void OnPeriodicDeadOrAlive(const int video_channel,
                                     const bool alive) = 0;

  // This method is called once if a packet timeout occurred.
  virtual void PacketTimeout(const int video_channel,
                             const ViEPacketTimeout timeout) = 0;
 protected:
  virtual ~ViENetworkObserver() {}
};

class WEBRTC_DLLEXPORT ViENetwork {
 public:
  // Default values.
  enum { KDefaultSampleTimeSeconds = 2 };

  // Factory for the ViENetwork sub‐API and increases an internal reference
  // counter if successful. Returns NULL if the API is not supported or if
  // construction fails.
  static ViENetwork* GetInterface(VideoEngine* video_engine);

  // Releases the ViENetwork sub-API and decreases an internal reference
  // counter.Returns the new reference count. This value should be zero
  // for all sub-API:s before the VideoEngine object can be safely deleted.
  virtual int Release() = 0;

  // Specifies the ports to receive RTP packets on. It is also possible to set
  // port for RTCP and local IP address.
  virtual int SetLocalReceiver(const int video_channel,
                               const unsigned short rtp_port,
                               const unsigned short rtcp_port = 0,
                               const char* ip_address = NULL) = 0;

  // Gets the local receiver ports and address for a specified channel.
  virtual int GetLocalReceiver(const int video_channel,
                               unsigned short& rtp_port,
                               unsigned short& rtcp_port, char* ip_address) = 0;

  // Specifies the destination port and IP address for a specified channel.
  virtual int SetSendDestination(const int video_channel,
                                 const char* ip_address,
                                 const unsigned short rtp_port,
                                 const unsigned short rtcp_port = 0,
                                 const unsigned short source_rtp_port = 0,
                                 const unsigned short source_rtcp_port = 0) = 0;

  // Get the destination port and address for a specified channel.
  virtual int GetSendDestination(const int video_channel,
                                 char* ip_address,
                                 unsigned short& rtp_port,
                                 unsigned short& rtcp_port,
                                 unsigned short& source_rtp_port,
                                 unsigned short& source_rtcp_port) = 0;

  // This function registers a user implementation of Transport to use for
  // sending RTP and RTCP packets on this channel.
  virtual int RegisterSendTransport(const int video_channel,
                                    Transport& transport) = 0;

  // This function deregisters a used Transport for a specified channel.
  virtual int DeregisterSendTransport(const int video_channel) = 0;

  // When using external transport for a channel, received RTP packets should
  // be passed to VideoEngine using this function. The input should contain
  // the RTP header and payload.
  virtual int ReceivedRTPPacket(const int video_channel,
                                const void* data,
                                const int length) = 0;

  // When using external transport for a channel, received RTCP packets should
  // be passed to VideoEngine using this function.
  virtual int ReceivedRTCPPacket(const int video_channel,
                                 const void* data,
                                 const int length) = 0;

  // Gets the source ports and IP address of the incoming stream for a
  // specified channel.
  virtual int GetSourceInfo(const int video_channel,
                            unsigned short& rtp_port,
                            unsigned short& rtcp_port,
                            char* ip_address,
                            unsigned int ip_address_length) = 0;

  // Gets the local IP address, in string format.
  virtual int GetLocalIP(char ip_address[64], bool ipv6 = false) = 0;

  // Enables IPv6, instead of IPv4, for a specified channel.
  virtual int EnableIPv6(int video_channel) = 0;

  // The function returns true if IPv6 is enabled, false otherwise.
  virtual bool IsIPv6Enabled(int video_channel) = 0;

  // Enables a port and IP address filtering for incoming packets on a
  // specific channel.
  virtual int SetSourceFilter(const int video_channel,
                              const unsigned short rtp_port,
                              const unsigned short rtcp_port = 0,
                              const char* ip_address = NULL) = 0;

  // Gets current port and IP address filter for a specified channel.
  virtual int GetSourceFilter(const int video_channel,
                              unsigned short& rtp_port,
                              unsigned short& rtcp_port,
                              char* ip_address) = 0;

  // This function sets the six‐bit Differentiated Services Code Point (DSCP)
  // in the IP header of the outgoing stream for a specific channel.
  // Windows and Linux only.
  virtual int SetSendToS(const int video_channel,
                         const int DSCP,
                         const bool use_set_sockOpt = false) = 0;

  // Retrieves the six‐bit Differentiated Services Code Point (DSCP) in the IP
  // header of the outgoing stream for a specific channel.
  virtual int GetSendToS(const int video_channel,
                         int& DSCP,
                         bool& use_set_sockOpt) = 0;

  // This function sets the Generic Quality of Service (GQoS) service level.
  // The Windows operating system then maps to a Differentiated Services Code
  // Point (DSCP) and to an 802.1p setting. Windows only.
  virtual int SetSendGQoS(const int video_channel, const bool enable,
                          const int service_type,
                          const int overrideDSCP = 0) = 0;

  // This function retrieves the currently set GQoS service level for a
  // specific channel.
  virtual int GetSendGQoS(const int video_channel,
                          bool& enabled,
                          int& service_type,
                          int& overrideDSCP) = 0;

  // This function sets the Maximum Transition Unit (MTU) for a channel. The
  // RTP packet will be packetized based on this MTU to optimize performance
  // over the network.
  virtual int SetMTU(int video_channel, unsigned int mtu) = 0;

  // This function enables or disables warning reports if packets have not
  // been received for a specified time interval.
  virtual int SetPacketTimeoutNotification(const int video_channel,
                                           bool enable,
                                           int timeout_seconds) = 0;

  // Registers an instance of a user implementation of the ViENetwork
  // observer.
  virtual int RegisterObserver(const int video_channel,
                               ViENetworkObserver& observer) = 0;

  // Removes a registered instance of ViENetworkObserver.
  virtual int DeregisterObserver(const int video_channel) = 0;

  // This function enables or disables the periodic dead‐or‐alive callback
  // functionality for a specified channel.
  virtual int SetPeriodicDeadOrAliveStatus(
      const int video_channel,
      const bool enable,
      const unsigned int sample_time_seconds = KDefaultSampleTimeSeconds) = 0;

  // This function handles sending a raw UDP data packet over an existing RTP
  // or RTCP socket.
  virtual int SendUDPPacket(const int video_channel,
                            const void* data,
                            const unsigned int length,
                            int& transmitted_bytes,
                            bool use_rtcp_socket = false) = 0;

 protected:
  ViENetwork() {}
  virtual ~ViENetwork() {}
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_NETWORK_H_
