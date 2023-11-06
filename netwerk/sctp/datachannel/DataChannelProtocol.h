/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NETWERK_SCTP_DATACHANNEL_DATACHANNELPROTOCOL_H_
#define NETWERK_SCTP_DATACHANNEL_DATACHANNELPROTOCOL_H_

#if defined(__GNUC__)
#  define SCTP_PACKED __attribute__((packed))
#elif defined(_MSC_VER)
#  pragma pack(push, 1)
#  define SCTP_PACKED
#else
#  error "Unsupported compiler"
#endif

#define WEBRTC_DATACHANNEL_STREAMS_DEFAULT 256
// Do not change this value!
#define WEBRTC_DATACHANNEL_PORT_DEFAULT 5000
// TODO: Bug 1381146, change once we resolve the nsCString limitation
#define WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_LOCAL 1073741823
#define WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_REMOTE_DEFAULT 65536
// TODO: Bug 1382779, once resolved, can be increased to min(Uint8ArrayMaxSize,
// UINT32_MAX)
// TODO: Bug 1381146, once resolved, can be increased to whatever we support
//       then (hopefully SIZE_MAX) or be removed
#define WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_REMOTE 2147483637

#define DATA_CHANNEL_PPID_CONTROL 50
#define DATA_CHANNEL_PPID_BINARY_PARTIAL 52
#define DATA_CHANNEL_PPID_BINARY 53
#define DATA_CHANNEL_PPID_DOMSTRING_PARTIAL 54
#define DATA_CHANNEL_PPID_DOMSTRING 51
#define DATA_CHANNEL_PPID_DOMSTRING_EMPTY 56
#define DATA_CHANNEL_PPID_BINARY_EMPTY 57

#define DATA_CHANNEL_MAX_BINARY_FRAGMENT 0x4000

#define DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_TOO_LARGE 0x01
#define DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_BUFFERED 0x02
#define DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_COMPLETE 0x04

#define INVALID_STREAM (0xFFFF)
// max is 0xFFFF: Streams 0 to 0xFFFE = 0xFFFF streams
#define MAX_NUM_STREAMS (2048)

struct rtcweb_datachannel_open_request {
  uint8_t msg_type;  // DATA_CHANNEL_OPEN
  uint8_t channel_type;
  int16_t priority;
  uint32_t reliability_param;
  uint16_t label_length;
  uint16_t protocol_length;
  char label[1];  // (and protocol) keep VC++ happy...
} SCTP_PACKED;

struct rtcweb_datachannel_ack {
  uint8_t msg_type;  // DATA_CHANNEL_ACK
} SCTP_PACKED;

/* msg_type values: */
/* 0-1 were used in an early version of the protocol with 3-way handshakes */
#define DATA_CHANNEL_ACK 2
#define DATA_CHANNEL_OPEN_REQUEST 3

/* channel_type values: */
#define DATA_CHANNEL_RELIABLE 0x00
#define DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT 0x01
#define DATA_CHANNEL_PARTIAL_RELIABLE_TIMED 0x02

#define DATA_CHANNEL_RELIABLE_UNORDERED 0x80
#define DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT_UNORDERED 0x81
#define DATA_CHANNEL_PARTIAL_RELIABLE_TIMED_UNORDERED 0x82

#define ERR_DATA_CHANNEL_ALREADY_OPEN 1
#define ERR_DATA_CHANNEL_NONE_AVAILABLE 2

#if defined(_MSC_VER)
#  pragma pack(pop)
#  undef SCTP_PACKED
#endif

#endif
