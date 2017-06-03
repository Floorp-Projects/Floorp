/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NETWERK_SCTP_DATACHANNEL_DATACHANNELPROTOCOL_H_
#define NETWERK_SCTP_DATACHANNEL_DATACHANNELPROTOCOL_H_

#if defined(__GNUC__)
#define SCTP_PACKED __attribute__((packed))
#elif defined(_MSC_VER)
#pragma pack (push, 1)
#define SCTP_PACKED
#else
#error "Unsupported compiler"
#endif

#define WEBRTC_DATACHANNEL_STREAMS_DEFAULT          256
#define WEBRTC_DATACHANNEL_PORT_DEFAULT             5000
#define WEBRTC_DATACHANELL_MAX_MESSAGE_SIZE_DEFAULT 65536

#define DATA_CHANNEL_PPID_CONTROL        50
#define DATA_CHANNEL_PPID_BINARY         52
#define DATA_CHANNEL_PPID_BINARY_LAST    53
#define DATA_CHANNEL_PPID_DOMSTRING      54
#define DATA_CHANNEL_PPID_DOMSTRING_LAST 51

#define DATA_CHANNEL_MAX_BINARY_FRAGMENT 0x4000

#define DATA_CHANNEL_FLAGS_SEND_REQ             0x00000001
#define DATA_CHANNEL_FLAGS_SEND_RSP             0x00000002
#define DATA_CHANNEL_FLAGS_SEND_ACK             0x00000004
#define DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED 0x00000008
#define DATA_CHANNEL_FLAGS_SEND_DATA            0x00000010
#define DATA_CHANNEL_FLAGS_FINISH_OPEN          0x00000020
#define DATA_CHANNEL_FLAGS_FINISH_RSP           0x00000040
#define DATA_CHANNEL_FLAGS_EXTERNAL_NEGOTIATED  0x00000080
#define DATA_CHANNEL_FLAGS_WAITING_ACK          0x00000100

#define INVALID_STREAM (0xFFFF)
// max is 0xFFFF: Streams 0 to 0xFFFE = 0xFFFF streams
#define MAX_NUM_STREAMS (2048)

struct rtcweb_datachannel_open_request {
  uint8_t  msg_type; // DATA_CHANNEL_OPEN
  uint8_t  channel_type;  
  int16_t  priority;
  uint32_t reliability_param;
  uint16_t label_length;
  uint16_t protocol_length;
  char     label[1]; // (and protocol) keep VC++ happy...
} SCTP_PACKED;

struct rtcweb_datachannel_ack {
  uint8_t  msg_type; // DATA_CHANNEL_ACK
} SCTP_PACKED;

/* msg_type values: */
/* 0-1 were used in an early version of the protocol with 3-way handshakes */
#define DATA_CHANNEL_ACK                      2
#define DATA_CHANNEL_OPEN_REQUEST             3

/* channel_type values: */
#define DATA_CHANNEL_RELIABLE                 0x00
#define DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT  0x01
#define DATA_CHANNEL_PARTIAL_RELIABLE_TIMED   0x02

#define DATA_CHANNEL_RELIABLE_UNORDERED                0x80
#define DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT_UNORDERED 0x81
#define DATA_CHANNEL_PARTIAL_RELIABLE_TIMED_UNORDERED  0x82

#define ERR_DATA_CHANNEL_ALREADY_OPEN   1
#define ERR_DATA_CHANNEL_NONE_AVAILABLE 2

#if defined(_MSC_VER)
#pragma pack (pop)
#undef SCTP_PACKED
#endif

#endif
