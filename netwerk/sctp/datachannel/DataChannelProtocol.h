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

#define DATA_CHANNEL_PPID_CONTROL   50
#define DATA_CHANNEL_PPID_DOMSTRING 51
#define DATA_CHANNEL_PPID_BINARY    52
#define DATA_CHANNEL_PPID_BINARY_LAST 53

#define DATA_CHANNEL_MAX_BINARY_FRAGMENT 0x4000

#define DATA_CHANNEL_FLAGS_SEND_REQ             0x00000001
#define DATA_CHANNEL_FLAGS_SEND_RSP             0x00000002
#define DATA_CHANNEL_FLAGS_SEND_ACK             0x00000004
#define DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED 0x00000008
#define DATA_CHANNEL_FLAGS_SEND_DATA            0x00000010
#define DATA_CHANNEL_FLAGS_FINISH_OPEN          0x00000020
#define DATA_CHANNEL_FLAGS_FINISH_RSP           0x00000040

#define INVALID_STREAM (0xFFFF)
// max is 0xFFFF: Streams 0 to 0xFFFE = 0xFFFF streams
#define MAX_NUM_STREAMS (2048)

struct rtcweb_datachannel_open_request {
  uint8_t  msg_type; // DATA_CHANNEL_OPEN
  uint8_t  channel_type;  
  uint16_t flags;
  uint16_t reliability_params;
  int16_t  priority;
  char     label[1]; // keep VC++ happy...  UTF8 null-terminated string
} SCTP_PACKED;

struct rtcweb_datachannel_open_response {
  uint8_t  msg_type; // DATA_CHANNEL_OPEN_RESPONSE
  uint8_t  error;    // 0 == no error
  uint16_t flags;
  uint16_t reverse_stream;
} SCTP_PACKED;

struct rtcweb_datachannel_ack {
  uint8_t  msg_type; // DATA_CHANNEL_ACK
} SCTP_PACKED;

/* msg_type values: */
#define DATA_CHANNEL_OPEN_REQUEST             0
#define DATA_CHANNEL_OPEN_RESPONSE            1
#define DATA_CHANNEL_ACK                      2

/* channel_type values: */
#define DATA_CHANNEL_RELIABLE                 0
#define DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT  1
#define DATA_CHANNEL_PARTIAL_RELIABLE_TIMED   2

/* flags values: */
#define DATA_CHANNEL_FLAG_OUT_OF_ORDER_ALLOWED 0x0001
/* all other bits reserved and should be set to 0 */


#define ERR_DATA_CHANNEL_ALREADY_OPEN   1
#define ERR_DATA_CHANNEL_NONE_AVAILABLE 2

#if defined(_MSC_VER)
#pragma pack (pop)
#undef SCTP_PACKED
#endif

#endif
