/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_FAKENETWORKINTERFACE_H_
#define MEDIA_BASE_FAKENETWORKINTERFACE_H_

#include <map>
#include <set>
#include <vector>

#include "media/base/mediachannel.h"
#include "media/base/rtputils.h"
#include "rtc_base/byteorder.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/dscp.h"
#include "rtc_base/messagehandler.h"
#include "rtc_base/messagequeue.h"
#include "rtc_base/thread.h"

namespace cricket {

// Fake NetworkInterface that sends/receives RTP/RTCP packets.
class FakeNetworkInterface : public MediaChannel::NetworkInterface,
                             public rtc::MessageHandler {
 public:
  FakeNetworkInterface()
      : thread_(rtc::Thread::Current()),
        dest_(NULL),
        conf_(false),
        sendbuf_size_(-1),
        recvbuf_size_(-1),
        dscp_(rtc::DSCP_NO_CHANGE) {
  }

  void SetDestination(MediaChannel* dest) { dest_ = dest; }

  // Conference mode is a mode where instead of simply forwarding the packets,
  // the transport will send multiple copies of the packet with the specified
  // SSRCs. This allows us to simulate receiving media from multiple sources.
  void SetConferenceMode(bool conf, const std::vector<uint32_t>& ssrcs) {
    rtc::CritScope cs(&crit_);
    conf_ = conf;
    conf_sent_ssrcs_ = ssrcs;
  }

  int NumRtpBytes() {
    rtc::CritScope cs(&crit_);
    int bytes = 0;
    for (size_t i = 0; i < rtp_packets_.size(); ++i) {
      bytes += static_cast<int>(rtp_packets_[i].size());
    }
    return bytes;
  }

  int NumRtpBytes(uint32_t ssrc) {
    rtc::CritScope cs(&crit_);
    int bytes = 0;
    GetNumRtpBytesAndPackets(ssrc, &bytes, NULL);
    return bytes;
  }

  int NumRtpPackets() {
    rtc::CritScope cs(&crit_);
    return static_cast<int>(rtp_packets_.size());
  }

  int NumRtpPackets(uint32_t ssrc) {
    rtc::CritScope cs(&crit_);
    int packets = 0;
    GetNumRtpBytesAndPackets(ssrc, NULL, &packets);
    return packets;
  }

  int NumSentSsrcs() {
    rtc::CritScope cs(&crit_);
    return static_cast<int>(sent_ssrcs_.size());
  }

  // Note: callers are responsible for deleting the returned buffer.
  const rtc::CopyOnWriteBuffer* GetRtpPacket(int index) {
    rtc::CritScope cs(&crit_);
    if (index >= NumRtpPackets()) {
      return NULL;
    }
    return new rtc::CopyOnWriteBuffer(rtp_packets_[index]);
  }

  int NumRtcpPackets() {
    rtc::CritScope cs(&crit_);
    return static_cast<int>(rtcp_packets_.size());
  }

  // Note: callers are responsible for deleting the returned buffer.
  const rtc::CopyOnWriteBuffer* GetRtcpPacket(int index) {
    rtc::CritScope cs(&crit_);
    if (index >= NumRtcpPackets()) {
      return NULL;
    }
    return new rtc::CopyOnWriteBuffer(rtcp_packets_[index]);
  }

  int sendbuf_size() const { return sendbuf_size_; }
  int recvbuf_size() const { return recvbuf_size_; }
  rtc::DiffServCodePoint dscp() const { return dscp_; }

 protected:
  virtual bool SendPacket(rtc::CopyOnWriteBuffer* packet,
                          const rtc::PacketOptions& options) {
    rtc::CritScope cs(&crit_);

    uint32_t cur_ssrc = 0;
    if (!GetRtpSsrc(packet->data(), packet->size(), &cur_ssrc)) {
      return false;
    }
    sent_ssrcs_[cur_ssrc]++;

    rtp_packets_.push_back(*packet);
    if (conf_) {
      for (size_t i = 0; i < conf_sent_ssrcs_.size(); ++i) {
        if (!SetRtpSsrc(packet->data(), packet->size(),
                        conf_sent_ssrcs_[i])) {
          return false;
        }
        PostMessage(ST_RTP, *packet);
      }
    } else {
      PostMessage(ST_RTP, *packet);
    }
    return true;
  }

  virtual bool SendRtcp(rtc::CopyOnWriteBuffer* packet,
                        const rtc::PacketOptions& options) {
    rtc::CritScope cs(&crit_);
    rtcp_packets_.push_back(*packet);
    if (!conf_) {
      // don't worry about RTCP in conf mode for now
      PostMessage(ST_RTCP, *packet);
    }
    return true;
  }

  virtual int SetOption(SocketType type, rtc::Socket::Option opt,
                        int option) {
    if (opt == rtc::Socket::OPT_SNDBUF) {
      sendbuf_size_ = option;
    } else if (opt == rtc::Socket::OPT_RCVBUF) {
      recvbuf_size_ = option;
    } else if (opt == rtc::Socket::OPT_DSCP) {
      dscp_ = static_cast<rtc::DiffServCodePoint>(option);
    }
    return 0;
  }

  void PostMessage(int id, const rtc::CopyOnWriteBuffer& packet) {
    thread_->Post(RTC_FROM_HERE, this, id, rtc::WrapMessageData(packet));
  }

  virtual void OnMessage(rtc::Message* msg) {
    rtc::TypedMessageData<rtc::CopyOnWriteBuffer>* msg_data =
        static_cast<rtc::TypedMessageData<rtc::CopyOnWriteBuffer>*>(
            msg->pdata);
    if (dest_) {
      if (msg->message_id == ST_RTP) {
        dest_->OnPacketReceived(&msg_data->data(),
                                rtc::CreatePacketTime(0));
      } else {
        dest_->OnRtcpReceived(&msg_data->data(),
                              rtc::CreatePacketTime(0));
      }
    }
    delete msg_data;
  }

 private:
  void GetNumRtpBytesAndPackets(uint32_t ssrc, int* bytes, int* packets) {
    if (bytes) {
      *bytes = 0;
    }
    if (packets) {
      *packets = 0;
    }
    uint32_t cur_ssrc = 0;
    for (size_t i = 0; i < rtp_packets_.size(); ++i) {
      if (!GetRtpSsrc(rtp_packets_[i].data(), rtp_packets_[i].size(),
                      &cur_ssrc)) {
        return;
      }
      if (ssrc == cur_ssrc) {
        if (bytes) {
          *bytes += static_cast<int>(rtp_packets_[i].size());
        }
        if (packets) {
          ++(*packets);
        }
      }
    }
  }

  rtc::Thread* thread_;
  MediaChannel* dest_;
  bool conf_;
  // The ssrcs used in sending out packets in conference mode.
  std::vector<uint32_t> conf_sent_ssrcs_;
  // Map to track counts of packets that have been sent per ssrc.
  // This includes packets that are dropped.
  std::map<uint32_t, uint32_t> sent_ssrcs_;
  // Map to track packet-number that needs to be dropped per ssrc.
  std::map<uint32_t, std::set<uint32_t> > drop_map_;
  rtc::CriticalSection crit_;
  std::vector<rtc::CopyOnWriteBuffer> rtp_packets_;
  std::vector<rtc::CopyOnWriteBuffer> rtcp_packets_;
  int sendbuf_size_;
  int recvbuf_size_;
  rtc::DiffServCodePoint dscp_;
};

}  // namespace cricket

#endif  // MEDIA_BASE_FAKENETWORKINTERFACE_H_
