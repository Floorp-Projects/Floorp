/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/test/pcap_file_reader.h"

#ifdef WIN32
#include <windows.h>
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/modules/video_coding/main/test/rtp_player.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {
namespace rtpplayer {

enum {
  kResultFail = -1,
  kResultSuccess = 0,
  kResultSkip = 1,

  kPcapVersionMajor = 2,
  kPcapVersionMinor = 4,
  kLinktypeNull = 0,
  kLinktypeEthernet = 1,
  kBsdNullLoopback1 = 0x00000002,
  kBsdNullLoopback2 = 0x02000000,
  kEthernetIIHeaderMacSkip = 12,
  kEthertypeIp = 0x0800,
  kIpVersion4 = 4,
  kMinIpHeaderLength = 20,
  kFragmentOffsetClear = 0x0000,
  kFragmentOffsetDoNotFragment = 0x4000,
  kProtocolTcp = 0x06,
  kProtocolUdp = 0x11,
  kUdpHeaderLength = 8,
  kMaxReadBufferSize = 4096
};

const uint32_t kPcapBOMSwapOrder = 0xd4c3b2a1UL;
const uint32_t kPcapBOMNoSwapOrder = 0xa1b2c3d4UL;

#if 1
# define DEBUG_LOG(text)
# define DEBUG_LOG1(text, arg)
#else
# define DEBUG_LOG(text) (printf(text "\n"))
# define DEBUG_LOG1(text, arg) (printf(text "\n", arg))
#endif

#define TRY(expr)                                       \
  do {                                                  \
    int r = (expr);                                     \
    if (r == kResultFail) {                             \
      DEBUG_LOG1("FAIL at " __FILE__ ":%d", __LINE__);  \
      return kResultFail;                               \
    } else if (r == kResultSkip) {                      \
      return kResultSkip;                               \
    }                                                   \
  } while (0)

// Read RTP packets from file in tcpdump/libpcap format, as documented at:
// http://wiki.wireshark.org/Development/LibpcapFileFormat
class PcapFileReaderImpl : public RtpPacketSourceInterface {
 public:
  PcapFileReaderImpl()
    : file_(NULL),
      swap_pcap_byte_order_(false),
      swap_network_byte_order_(false),
      read_buffer_(),
      packets_by_ssrc_(),
      packets_(),
      next_packet_it_() {
    int16_t test = 0x7f00;
    if (test != htons(test)) {
      swap_network_byte_order_ = true;
    }
  }

  virtual ~PcapFileReaderImpl() {
    if (file_ != NULL) {
      fclose(file_);
      file_ = NULL;
    }
  }

  int Initialize(const std::string& filename) {
    file_ = fopen(filename.c_str(), "rb");
    if (file_ == NULL) {
      printf("ERROR: Can't open file: %s\n", filename.c_str());
      return kResultFail;
    }

    if (ReadGlobalHeader() < 0) {
      return kResultFail;
    }

    int total_packet_count = 0;
    uint32_t stream_start_ms = 0;
    int32_t next_packet_pos = ftell(file_);
    for (;;) {
      ++total_packet_count;
      TRY(fseek(file_, next_packet_pos, SEEK_SET));
      int result = ReadPacket(&next_packet_pos, stream_start_ms,
                              total_packet_count);
      if (result == kResultFail) {
        break;
      } else if (result == kResultSuccess && packets_.size() == 1) {
        assert(stream_start_ms == 0);
        PacketIterator it = packets_.begin();
        stream_start_ms = it->time_offset_ms;
        it->time_offset_ms = 0;
      }
    }

    if (feof(file_) == 0) {
      printf("Failed reading file!\n");
      return kResultFail;
    }

    printf("Total packets in file: %d\n", total_packet_count);
    printf("Total RTP/RTCP packets: %d\n", static_cast<int>(packets_.size()));

    for (SsrcMapIterator mit = packets_by_ssrc_.begin();
        mit != packets_by_ssrc_.end(); ++mit) {
      uint32_t ssrc = mit->first;
      const std::vector<uint32_t>& packet_numbers = mit->second;
      printf("SSRC: %08x, %d packets\n", ssrc,
             static_cast<int>(packet_numbers.size()));
    }

    // TODO(solenberg): Better validation of identified SSRC streams.
    //
    // Since we're dealing with raw network data here, we will wrongly identify
    // some packets as RTP. When these packets are consumed by RtpPlayer, they
    // are unlikely to cause issues as they will ultimately be filtered out by
    // the RtpRtcp module. However, we should really do better filtering here,
    // which we can accomplish in a number of ways, e.g.:
    //
    // - Verify that the time stamps and sequence numbers for RTP packets are
    //   both increasing/decreasing. If they move in different directions, the
    //   SSRC is likely bogus and can be dropped. (Normally they should be inc-
    //   reasing but we must allow packet reordering).
    // - If RTP sequence number is not changing, drop the stream.
    // - Can also use srcip:port->dstip:port pairs, assuming few SSRC collisions
    //   for up/down streams.

    next_packet_it_ = packets_.begin();
    return kResultSuccess;
  }

  virtual int NextPacket(uint8_t* data, uint32_t* length, uint32_t* time_ms) {
    assert(data);
    assert(length);
    assert(time_ms);

    if (next_packet_it_ == packets_.end()) {
      return -1;
    }
    if (*length < next_packet_it_->payload_length) {
      return -1;
    }
    TRY(fseek(file_, next_packet_it_->pos_in_file, SEEK_SET));
    TRY(Read(data, next_packet_it_->payload_length));
    *length = next_packet_it_->payload_length;
    *time_ms = next_packet_it_->time_offset_ms;
    next_packet_it_++;

    return 0;
  }

 private:
  // A marker of an RTP packet within the file.
  struct RtpPacketMarker {
    uint32_t packet_number;
    uint32_t time_offset_ms;
    uint32_t source_ip;
    uint32_t dest_ip;
    uint16_t source_port;
    uint16_t dest_port;
    WebRtcRTPHeader rtp_header;
    int32_t pos_in_file;
    uint32_t payload_length;
  };

  typedef std::vector<RtpPacketMarker>::iterator PacketIterator;
  typedef std::map<uint32_t, std::vector<uint32_t> > SsrcMap;
  typedef std::map<uint32_t, std::vector<uint32_t> >::iterator SsrcMapIterator;

  int ReadGlobalHeader() {
    uint32_t magic;
    TRY(Read(&magic, false));
    if (magic == kPcapBOMSwapOrder) {
      swap_pcap_byte_order_ = true;
    } else if (magic == kPcapBOMNoSwapOrder) {
      swap_pcap_byte_order_ = false;
    } else {
      return kResultFail;
    }

    uint16_t version_major;
    uint16_t version_minor;
    TRY(Read(&version_major, false));
    TRY(Read(&version_minor, false));
    if (version_major != kPcapVersionMajor ||
        version_minor != kPcapVersionMinor) {
      return kResultFail;
    }

    int32_t this_zone;  // GMT to local correction.
    uint32_t sigfigs;   // Accuracy of timestamps.
    uint32_t snaplen;   // Max length of captured packets, in octets.
    uint32_t network;   // Data link type.
    TRY(Read(&this_zone, false));
    TRY(Read(&sigfigs, false));
    TRY(Read(&snaplen, false));
    TRY(Read(&network, false));

    // Accept only LINKTYPE_NULL and LINKTYPE_ETHERNET.
    // See: http://www.tcpdump.org/linktypes.html
    if (network != kLinktypeNull && network != kLinktypeEthernet) {
      return kResultFail;
    }

    return kResultSuccess;
  }

  int ReadPacket(int32_t* next_packet_pos, uint32_t stream_start_ms,
                 uint32_t number) {
    assert(next_packet_pos);

    uint32_t ts_sec;    // Timestamp seconds.
    uint32_t ts_usec;   // Timestamp microseconds.
    uint32_t incl_len;  // Number of octets of packet saved in file.
    uint32_t orig_len;  // Actual length of packet.
    TRY(Read(&ts_sec, false));
    TRY(Read(&ts_usec, false));
    TRY(Read(&incl_len, false));
    TRY(Read(&orig_len, false));

    *next_packet_pos = ftell(file_) + incl_len;

    RtpPacketMarker marker = {0};
    marker.packet_number = number;
    marker.time_offset_ms = CalcTimeDelta(ts_sec, ts_usec, stream_start_ms);
    TRY(ReadPacketHeader(&marker));
    marker.pos_in_file = ftell(file_);

    if (marker.payload_length > sizeof(read_buffer_)) {
      printf("Packet too large!\n");
      return kResultFail;
    }
    TRY(Read(read_buffer_, marker.payload_length));

    ModuleRTPUtility::RTPHeaderParser rtp_parser(read_buffer_,
                                                 marker.payload_length);
    if (rtp_parser.RTCP()) {
      packets_.push_back(marker);
    } else {
      if (!rtp_parser.Parse(marker.rtp_header, NULL)) {
        DEBUG_LOG("Not recognized as RTP/RTCP");
        return kResultSkip;
      }

      uint32_t ssrc = marker.rtp_header.header.ssrc;
      packets_by_ssrc_[ssrc].push_back(marker.packet_number);
      packets_.push_back(marker);
    }

    return kResultSuccess;
  }

  int ReadPacketHeader(RtpPacketMarker* marker) {
    int32_t file_pos = ftell(file_);

    // Check for BSD null/loopback frame header. The header is just 4 bytes in
    // native byte order, so we check for both versions as we don't care about
    // the header as such and will likely fail reading the IP header if this is
    // something else than null/loopback.
    uint32_t protocol;
    TRY(Read(&protocol, true));
    if (protocol == kBsdNullLoopback1 || protocol == kBsdNullLoopback2) {
      int result = ReadXxpIpHeader(marker);
      DEBUG_LOG("Recognized loopback frame");
      if (result != kResultSkip) {
        return result;
      }
    }

    TRY(fseek(file_, file_pos, SEEK_SET));

    // Check for Ethernet II, IP frame header.
    uint16_t type;
    TRY(Skip(kEthernetIIHeaderMacSkip));  // Source+destination MAC.
    TRY(Read(&type, true));
    if (type == kEthertypeIp) {
      int result = ReadXxpIpHeader(marker);
      DEBUG_LOG("Recognized ethernet 2 frame");
      if (result != kResultSkip) {
        return result;
      }
    }

    return kResultSkip;
  }

  uint32_t CalcTimeDelta(uint32_t ts_sec, uint32_t ts_usec, uint32_t start_ms) {
    // Round to nearest ms.
    uint64_t t2_ms = ((static_cast<uint64_t>(ts_sec) * 1000000) + ts_usec +
        500) / 1000;
    uint64_t t1_ms = static_cast<uint64_t>(start_ms);
    if (t2_ms < t1_ms) {
      return 0;
    } else {
      return t2_ms - t1_ms;
    }
  }

  int ReadXxpIpHeader(RtpPacketMarker* marker) {
    assert(marker);

    uint16_t version;
    uint16_t length;
    uint16_t id;
    uint16_t fragment;
    uint16_t protocol;
    uint16_t checksum;
    TRY(Read(&version, true));
    TRY(Read(&length, true));
    TRY(Read(&id, true));
    TRY(Read(&fragment, true));
    TRY(Read(&protocol, true));
    TRY(Read(&checksum, true));
    TRY(Read(&marker->source_ip, true));
    TRY(Read(&marker->dest_ip, true));

    if (((version >> 12) & 0x000f) != kIpVersion4) {
      DEBUG_LOG("IP header is not IPv4");
      return kResultSkip;
    }

    if (fragment != kFragmentOffsetClear &&
        fragment != kFragmentOffsetDoNotFragment) {
      DEBUG_LOG("IP fragments cannot be handled");
      return kResultSkip;
    }

    // Skip remaining fields of IP header.
    uint16_t header_length = (version & 0x0f00) >> (8 - 2);
    assert(header_length >= kMinIpHeaderLength);
    TRY(Skip(header_length - kMinIpHeaderLength));

    protocol = protocol & 0x00ff;
    if (protocol == kProtocolTcp) {
      DEBUG_LOG("TCP packets are not handled");
      return kResultSkip;
    } else if (protocol == kProtocolUdp) {
      uint16_t length;
      uint16_t checksum;
      TRY(Read(&marker->source_port, true));
      TRY(Read(&marker->dest_port, true));
      TRY(Read(&length, true));
      TRY(Read(&checksum, true));
      marker->payload_length = length - kUdpHeaderLength;
    } else {
      DEBUG_LOG("Unknown transport (expected UDP or TCP)");
      return kResultSkip;
    }

    return kResultSuccess;
  }

  int Read(uint32_t* out, bool expect_network_order) {
    uint32_t tmp = 0;
    if (fread(&tmp, 1, sizeof(uint32_t), file_) != sizeof(uint32_t)) {
      return kResultFail;
    }
    if ((!expect_network_order && swap_pcap_byte_order_) ||
        (expect_network_order && swap_network_byte_order_)) {
      tmp = ((tmp >> 24) & 0x000000ff) | (tmp << 24) |
          ((tmp >> 8) & 0x0000ff00) | ((tmp << 8) & 0x00ff0000);
    }
    *out = tmp;
    return kResultSuccess;
  }

  int Read(uint16_t* out, bool expect_network_order) {
    uint16_t tmp = 0;
    if (fread(&tmp, 1, sizeof(uint16_t), file_) != sizeof(uint16_t)) {
      return kResultFail;
    }
    if ((!expect_network_order && swap_pcap_byte_order_) ||
        (expect_network_order && swap_network_byte_order_)) {
      tmp = ((tmp >> 8) & 0x00ff) | (tmp << 8);
    }
    *out = tmp;
    return kResultSuccess;
  }

  int Read(uint8_t* out, uint32_t count) {
    if (fread(out, 1, count, file_) != count) {
      return kResultFail;
    }
    return kResultSuccess;
  }

  int Read(int32_t* out, bool expect_network_order) {
    int32_t tmp = 0;
    if (fread(&tmp, 1, sizeof(uint32_t), file_) != sizeof(uint32_t)) {
      return kResultFail;
    }
    if ((!expect_network_order && swap_pcap_byte_order_) ||
        (expect_network_order && swap_network_byte_order_)) {
      tmp = ((tmp >> 24) & 0x000000ff) | (tmp << 24) |
          ((tmp >> 8) & 0x0000ff00) | ((tmp << 8) & 0x00ff0000);
    }
    *out = tmp;
    return kResultSuccess;
  }

  int Skip(uint32_t length) {
    if (fseek(file_, length, SEEK_CUR) != 0) {
      return kResultFail;
    }
    return kResultSuccess;
  }

  FILE* file_;
  bool swap_pcap_byte_order_;
  bool swap_network_byte_order_;
  uint8_t read_buffer_[kMaxReadBufferSize];

  SsrcMap packets_by_ssrc_;
  std::vector<RtpPacketMarker> packets_;
  PacketIterator next_packet_it_;

  DISALLOW_COPY_AND_ASSIGN(PcapFileReaderImpl);
};

RtpPacketSourceInterface* CreatePcapFileReader(const std::string& filename) {
  scoped_ptr<PcapFileReaderImpl> impl(new PcapFileReaderImpl());
  if (impl->Initialize(filename) != 0) {
    return NULL;
  }
  return impl.release();
}
}  // namespace rtpplayer
}  // namespace webrtc
