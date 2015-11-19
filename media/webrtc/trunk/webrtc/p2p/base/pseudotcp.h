/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_PSEUDOTCP_H_
#define WEBRTC_P2P_BASE_PSEUDOTCP_H_

#include <list>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/stream.h"

namespace cricket {

//////////////////////////////////////////////////////////////////////
// IPseudoTcpNotify
//////////////////////////////////////////////////////////////////////

class PseudoTcp;

class IPseudoTcpNotify {
 public:
  // Notification of tcp events
  virtual void OnTcpOpen(PseudoTcp* tcp) = 0;
  virtual void OnTcpReadable(PseudoTcp* tcp) = 0;
  virtual void OnTcpWriteable(PseudoTcp* tcp) = 0;
  virtual void OnTcpClosed(PseudoTcp* tcp, uint32 error) = 0;

  // Write the packet onto the network
  enum WriteResult { WR_SUCCESS, WR_TOO_LARGE, WR_FAIL };
  virtual WriteResult TcpWritePacket(PseudoTcp* tcp,
                                     const char* buffer, size_t len) = 0;

 protected:
  virtual ~IPseudoTcpNotify() {}
};

//////////////////////////////////////////////////////////////////////
// PseudoTcp
//////////////////////////////////////////////////////////////////////

class PseudoTcp {
 public:
  static uint32 Now();

  PseudoTcp(IPseudoTcpNotify* notify, uint32 conv);
  virtual ~PseudoTcp();

  int Connect();
  int Recv(char* buffer, size_t len);
  int Send(const char* buffer, size_t len);
  void Close(bool force);
  int GetError();

  enum TcpState {
    TCP_LISTEN, TCP_SYN_SENT, TCP_SYN_RECEIVED, TCP_ESTABLISHED, TCP_CLOSED
  };
  TcpState State() const { return m_state; }

  // Call this when the PMTU changes.
  void NotifyMTU(uint16 mtu);

  // Call this based on timeout value returned from GetNextClock.
  // It's ok to call this too frequently.
  void NotifyClock(uint32 now);

  // Call this whenever a packet arrives.
  // Returns true if the packet was processed successfully.
  bool NotifyPacket(const char * buffer, size_t len);

  // Call this to determine the next time NotifyClock should be called.
  // Returns false if the socket is ready to be destroyed.
  bool GetNextClock(uint32 now, long& timeout);

  // Call these to get/set option values to tailor this PseudoTcp
  // instance's behaviour for the kind of data it will carry.
  // If an unrecognized option is set or got, an assertion will fire.
  //
  // Setting options for OPT_RCVBUF or OPT_SNDBUF after Connect() is called
  // will result in an assertion.
  enum Option {
    OPT_NODELAY,      // Whether to enable Nagle's algorithm (0 == off)
    OPT_ACKDELAY,     // The Delayed ACK timeout (0 == off).
    OPT_RCVBUF,       // Set the receive buffer size, in bytes.
    OPT_SNDBUF,       // Set the send buffer size, in bytes.
  };
  void GetOption(Option opt, int* value);
  void SetOption(Option opt, int value);

  // Returns current congestion window in bytes.
  uint32 GetCongestionWindow() const;

  // Returns amount of data in bytes that has been sent, but haven't
  // been acknowledged.
  uint32 GetBytesInFlight() const;

  // Returns number of bytes that were written in buffer and haven't
  // been sent.
  uint32 GetBytesBufferedNotSent() const;

  // Returns current round-trip time estimate in milliseconds.
  uint32 GetRoundTripTimeEstimateMs() const;

 protected:
  enum SendFlags { sfNone, sfDelayedAck, sfImmediateAck };

  struct Segment {
    uint32 conv, seq, ack;
    uint8 flags;
    uint16 wnd;
    const char * data;
    uint32 len;
    uint32 tsval, tsecr;
  };

  struct SSegment {
    SSegment(uint32 s, uint32 l, bool c)
        : seq(s), len(l), /*tstamp(0),*/ xmit(0), bCtrl(c) {
    }
    uint32 seq, len;
    //uint32 tstamp;
    uint8 xmit;
    bool bCtrl;
  };
  typedef std::list<SSegment> SList;

  struct RSegment {
    uint32 seq, len;
  };

  uint32 queue(const char* data, uint32 len, bool bCtrl);

  // Creates a packet and submits it to the network. This method can either
  // send payload or just an ACK packet.
  //
  // |seq| is the sequence number of this packet.
  // |flags| is the flags for sending this packet.
  // |offset| is the offset to read from |m_sbuf|.
  // |len| is the number of bytes to read from |m_sbuf| as payload. If this
  // value is 0 then this is an ACK packet, otherwise this packet has payload.
  IPseudoTcpNotify::WriteResult packet(uint32 seq, uint8 flags,
                                       uint32 offset, uint32 len);
  bool parse(const uint8* buffer, uint32 size);

  void attemptSend(SendFlags sflags = sfNone);

  void closedown(uint32 err = 0);

  bool clock_check(uint32 now, long& nTimeout);

  bool process(Segment& seg);
  bool transmit(const SList::iterator& seg, uint32 now);

  void adjustMTU();

 protected:
  // This method is used in test only to query receive buffer state.
  bool isReceiveBufferFull() const;

  // This method is only used in tests, to disable window scaling
  // support for testing backward compatibility.
  void disableWindowScale();

 private:
  // Queue the connect message with TCP options.
  void queueConnectMessage();

  // Parse TCP options in the header.
  void parseOptions(const char* data, uint32 len);

  // Apply a TCP option that has been read from the header.
  void applyOption(char kind, const char* data, uint32 len);

  // Apply window scale option.
  void applyWindowScaleOption(uint8 scale_factor);

  // Resize the send buffer with |new_size| in bytes.
  void resizeSendBuffer(uint32 new_size);

  // Resize the receive buffer with |new_size| in bytes. This call adjusts
  // window scale factor |m_swnd_scale| accordingly.
  void resizeReceiveBuffer(uint32 new_size);

  IPseudoTcpNotify* m_notify;
  enum Shutdown { SD_NONE, SD_GRACEFUL, SD_FORCEFUL } m_shutdown;
  int m_error;

  // TCB data
  TcpState m_state;
  uint32 m_conv;
  bool m_bReadEnable, m_bWriteEnable, m_bOutgoing;
  uint32 m_lasttraffic;

  // Incoming data
  typedef std::list<RSegment> RList;
  RList m_rlist;
  uint32 m_rbuf_len, m_rcv_nxt, m_rcv_wnd, m_lastrecv;
  uint8 m_rwnd_scale;  // Window scale factor.
  rtc::FifoBuffer m_rbuf;

  // Outgoing data
  SList m_slist;
  uint32 m_sbuf_len, m_snd_nxt, m_snd_wnd, m_lastsend, m_snd_una;
  uint8 m_swnd_scale;  // Window scale factor.
  rtc::FifoBuffer m_sbuf;

  // Maximum segment size, estimated protocol level, largest segment sent
  uint32 m_mss, m_msslevel, m_largest, m_mtu_advise;
  // Retransmit timer
  uint32 m_rto_base;

  // Timestamp tracking
  uint32 m_ts_recent, m_ts_lastack;

  // Round-trip calculation
  uint32 m_rx_rttvar, m_rx_srtt, m_rx_rto;

  // Congestion avoidance, Fast retransmit/recovery, Delayed ACKs
  uint32 m_ssthresh, m_cwnd;
  uint8 m_dup_acks;
  uint32 m_recover;
  uint32 m_t_ack;

  // Configuration options
  bool m_use_nagling;
  uint32 m_ack_delay;

  // This is used by unit tests to test backward compatibility of
  // PseudoTcp implementations that don't support window scaling.
  bool m_support_wnd_scale;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_PSEUDOTCP_H_
