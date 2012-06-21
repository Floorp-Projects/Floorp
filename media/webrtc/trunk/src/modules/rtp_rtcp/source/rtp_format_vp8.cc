/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format_vp8.h"

#include <string.h>  // memcpy

#include <cassert>   // assert
#include <vector>

#include "modules/rtp_rtcp/source/vp8_partition_aggregator.h"

namespace webrtc {

// Define how the VP8PacketizerModes are implemented.
// Modes are: kStrict, kAggregate, kEqualSize.
const RtpFormatVp8::AggregationMode RtpFormatVp8::aggr_modes_[kNumModes] =
    { kAggrNone, kAggrPartitions, kAggrFragments };
const bool RtpFormatVp8::balance_modes_[kNumModes] =
    { true, true, true };
const bool RtpFormatVp8::separate_first_modes_[kNumModes] =
    { true, false, false };

RtpFormatVp8::RtpFormatVp8(const WebRtc_UWord8* payload_data,
                           WebRtc_UWord32 payload_size,
                           const RTPVideoHeaderVP8& hdr_info,
                           int max_payload_len,
                           const RTPFragmentationHeader& fragmentation,
                           VP8PacketizerMode mode)
    : payload_data_(payload_data),
      payload_size_(static_cast<int>(payload_size)),
      vp8_fixed_payload_descriptor_bytes_(1),
      aggr_mode_(aggr_modes_[mode]),
      balance_(balance_modes_[mode]),
      separate_first_(separate_first_modes_[mode]),
      hdr_info_(hdr_info),
      num_partitions_(fragmentation.fragmentationVectorSize),
      max_payload_len_(max_payload_len),
      packets_calculated_(false) {
  part_info_ = fragmentation;
}

RtpFormatVp8::RtpFormatVp8(const WebRtc_UWord8* payload_data,
                           WebRtc_UWord32 payload_size,
                           const RTPVideoHeaderVP8& hdr_info,
                           int max_payload_len)
    : payload_data_(payload_data),
      payload_size_(static_cast<int>(payload_size)),
      part_info_(),
      vp8_fixed_payload_descriptor_bytes_(1),
      aggr_mode_(aggr_modes_[kEqualSize]),
      balance_(balance_modes_[kEqualSize]),
      separate_first_(separate_first_modes_[kEqualSize]),
      hdr_info_(hdr_info),
      num_partitions_(1),
      max_payload_len_(max_payload_len),
      packets_calculated_(false) {
    part_info_.VerifyAndAllocateFragmentationHeader(1);
    part_info_.fragmentationLength[0] = payload_size;
    part_info_.fragmentationOffset[0] = 0;
}

int RtpFormatVp8::NextPacket(WebRtc_UWord8* buffer,
                             int* bytes_to_send,
                             bool* last_packet) {
  if (!packets_calculated_) {
    int ret = 0;
    if (aggr_mode_ == kAggrPartitions && balance_) {
      ret = GeneratePacketsBalancedAggregates();
    } else {
      ret = GeneratePackets();
    }
    if (ret < 0) {
      return ret;
    }
  }
  if (packets_.empty()) {
    return -1;
  }
  InfoStruct packet_info = packets_.front();
  packets_.pop();

  *bytes_to_send = WriteHeaderAndPayload(packet_info, buffer, max_payload_len_);
  if (*bytes_to_send < 0) {
    return -1;
  }

  *last_packet = packets_.empty();
  return packet_info.first_partition_ix;
}

int RtpFormatVp8::CalcNextSize(int max_payload_len, int remaining_bytes,
                               bool split_payload) const {
  if (max_payload_len == 0 || remaining_bytes == 0) {
    return 0;
  }
  if (!split_payload) {
    return max_payload_len >= remaining_bytes ? remaining_bytes : 0;
  }

  if (balance_) {
    // Balance payload sizes to produce (almost) equal size
    // fragments.
    // Number of fragments for remaining_bytes:
    int num_frags = remaining_bytes / max_payload_len + 1;
    // Number of bytes in this fragment:
    return static_cast<int>(static_cast<double>(remaining_bytes)
                            / num_frags + 0.5);
  } else {
    return max_payload_len >= remaining_bytes ? remaining_bytes
        : max_payload_len;
  }
}

int RtpFormatVp8::GeneratePackets() {
  if (max_payload_len_ < vp8_fixed_payload_descriptor_bytes_
      + PayloadDescriptorExtraLength() + 1) {
    // The provided payload length is not long enough for the payload
    // descriptor and one payload byte. Return an error.
    return -1;
  }
  int total_bytes_processed = 0;
  bool start_on_new_fragment = true;
  bool beginning = true;
  int part_ix = 0;
  while (total_bytes_processed < payload_size_) {
    int packet_bytes = 0;  // How much data to send in this packet.
    bool split_payload = true;  // Splitting of partitions is initially allowed.
    int remaining_in_partition = part_info_.fragmentationOffset[part_ix] -
        total_bytes_processed + part_info_.fragmentationLength[part_ix];
    int rem_payload_len = max_payload_len_ -
        (vp8_fixed_payload_descriptor_bytes_ + PayloadDescriptorExtraLength());
    int first_partition_in_packet = part_ix;

    while (int next_size = CalcNextSize(rem_payload_len, remaining_in_partition,
                                        split_payload)) {
      packet_bytes += next_size;
      rem_payload_len -= next_size;
      remaining_in_partition -= next_size;

      if (remaining_in_partition == 0 && !(beginning && separate_first_)) {
        // Advance to next partition?
        // Check that there are more partitions; verify that we are either
        // allowed to aggregate fragments, or that we are allowed to
        // aggregate intact partitions and that we started this packet
        // with an intact partition (indicated by first_fragment_ == true).
        if (part_ix + 1 < num_partitions_ &&
            ((aggr_mode_ == kAggrFragments) ||
                (aggr_mode_ == kAggrPartitions && start_on_new_fragment))) {
          assert(part_ix < num_partitions_);
          remaining_in_partition = part_info_.fragmentationLength[++part_ix];
          // Disallow splitting unless kAggrFragments. In kAggrPartitions,
          // we can only aggregate intact partitions.
          split_payload = (aggr_mode_ == kAggrFragments);
        }
      } else if (balance_ && remaining_in_partition > 0) {
        break;
      }
    }
    if (remaining_in_partition == 0) {
      ++part_ix;  // Advance to next partition.
    }
    assert(packet_bytes > 0);

    QueuePacket(total_bytes_processed, packet_bytes, first_partition_in_packet,
                start_on_new_fragment);
    total_bytes_processed += packet_bytes;
    start_on_new_fragment = (remaining_in_partition == 0);
    beginning = false;  // Next packet cannot be first packet in frame.
  }
  packets_calculated_ = true;
  assert(total_bytes_processed == payload_size_);
  return 0;
}

int RtpFormatVp8::GeneratePacketsBalancedAggregates() {
  if (max_payload_len_ < vp8_fixed_payload_descriptor_bytes_
      + PayloadDescriptorExtraLength() + 1) {
    // The provided payload length is not long enough for the payload
    // descriptor and one payload byte. Return an error.
    return -1;
  }
  std::vector<int> partition_decision;
  const int overhead = vp8_fixed_payload_descriptor_bytes_ +
      PayloadDescriptorExtraLength();
  const uint32_t max_payload_len = max_payload_len_ - overhead;
  int min_size, max_size;
  AggregateSmallPartitions(&partition_decision, &min_size, &max_size);

  int total_bytes_processed = 0;
  int part_ix = 0;
  while (part_ix < num_partitions_) {
    if (partition_decision[part_ix] == -1) {
      // Split large partitions.
      int remaining_partition = part_info_.fragmentationLength[part_ix];
      int num_fragments = Vp8PartitionAggregator::CalcNumberOfFragments(
          remaining_partition, max_payload_len, overhead, min_size, max_size);
      const int packet_bytes =
          (remaining_partition + num_fragments - 1) / num_fragments;
      for (int n = 0; n < num_fragments; ++n) {
        const int this_packet_bytes = packet_bytes < remaining_partition ?
            packet_bytes : remaining_partition;
        QueuePacket(total_bytes_processed, this_packet_bytes, part_ix,
                    (n == 0));
        remaining_partition -= this_packet_bytes;
        total_bytes_processed += this_packet_bytes;
        if (this_packet_bytes < min_size) {
          min_size = this_packet_bytes;
        }
        if (this_packet_bytes > max_size) {
          max_size = this_packet_bytes;
        }
      }
      assert(remaining_partition == 0);
      ++part_ix;
    } else {
      int this_packet_bytes = 0;
      const int first_partition_in_packet = part_ix;
      const int aggregation_index = partition_decision[part_ix];
      while (static_cast<size_t>(part_ix) < partition_decision.size() &&
          partition_decision[part_ix] == aggregation_index) {
        // Collect all partitions that were aggregated into the same packet.
        this_packet_bytes += part_info_.fragmentationLength[part_ix];
        ++part_ix;
      }
      QueuePacket(total_bytes_processed, this_packet_bytes,
                  first_partition_in_packet, true);
      total_bytes_processed += this_packet_bytes;
    }
  }
  packets_calculated_ = true;
  return 0;
}

void RtpFormatVp8::AggregateSmallPartitions(std::vector<int>* partition_vec,
                                            int* min_size,
                                            int* max_size) {
  assert(min_size && max_size);
  *min_size = -1;
  *max_size = -1;
  assert(partition_vec);
  partition_vec->assign(num_partitions_, -1);
  const int overhead = vp8_fixed_payload_descriptor_bytes_ +
      PayloadDescriptorExtraLength();
  const uint32_t max_payload_len = max_payload_len_ - overhead;
  int first_in_set = 0;
  int last_in_set = 0;
  int num_aggregate_packets = 0;
  // Find sets of partitions smaller than max_payload_len_.
  while (first_in_set < num_partitions_) {
    if (part_info_.fragmentationLength[first_in_set] < max_payload_len) {
      // Found start of a set.
      last_in_set = first_in_set;
      while (last_in_set + 1 < num_partitions_ &&
          part_info_.fragmentationLength[last_in_set + 1] < max_payload_len) {
        ++last_in_set;
      }
      // Found end of a set. Run optimized aggregator. It is ok if start == end.
      Vp8PartitionAggregator aggregator(part_info_, first_in_set,
                                        last_in_set);
      if (*min_size >= 0 && *max_size >= 0) {
        aggregator.SetPriorMinMax(*min_size, *max_size);
      }
      Vp8PartitionAggregator::ConfigVec optimal_config =
          aggregator.FindOptimalConfiguration(max_payload_len, overhead);
      aggregator.CalcMinMax(optimal_config, min_size, max_size);
      for (int i = first_in_set, j = 0; i <= last_in_set; ++i, ++j) {
        // Transfer configuration for this set of partitions to the joint
        // partition vector representing all partitions in the frame.
        (*partition_vec)[i] = num_aggregate_packets + optimal_config[j];
      }
      num_aggregate_packets += optimal_config.back() + 1;
      first_in_set = last_in_set;
    }
    ++first_in_set;
  }
}

void RtpFormatVp8::QueuePacket(int start_pos,
                               int packet_size,
                               int first_partition_in_packet,
                               bool start_on_new_fragment) {
  // Write info to packet info struct and store in packet info queue.
  InfoStruct packet_info;
  packet_info.payload_start_pos = start_pos;
  packet_info.size = packet_size;
  packet_info.first_partition_ix = first_partition_in_packet;
  packet_info.first_fragment = start_on_new_fragment;
  packets_.push(packet_info);
}

int RtpFormatVp8::WriteHeaderAndPayload(const InfoStruct& packet_info,
                                        WebRtc_UWord8* buffer,
                                        int buffer_length) const {
  // Write the VP8 payload descriptor.
  //       0
  //       0 1 2 3 4 5 6 7 8
  //      +-+-+-+-+-+-+-+-+-+
  //      |X| |N|S| PART_ID |
  //      +-+-+-+-+-+-+-+-+-+
  // X:   |I|L|T|K|         | (mandatory if any of the below are used)
  //      +-+-+-+-+-+-+-+-+-+
  // I:   |PictureID (8/16b)| (optional)
  //      +-+-+-+-+-+-+-+-+-+
  // L:   |   TL0PIC_IDX    | (optional)
  //      +-+-+-+-+-+-+-+-+-+
  // T/K: |TID:Y|  KEYIDX   | (optional)
  //      +-+-+-+-+-+-+-+-+-+

  assert(packet_info.size > 0);
  buffer[0] = 0;
  if (XFieldPresent())            buffer[0] |= kXBit;
  if (hdr_info_.nonReference)     buffer[0] |= kNBit;
  if (packet_info.first_fragment) buffer[0] |= kSBit;
  buffer[0] |= (packet_info.first_partition_ix & kPartIdField);

  const int extension_length = WriteExtensionFields(buffer, buffer_length);

  memcpy(&buffer[vp8_fixed_payload_descriptor_bytes_ + extension_length],
         &payload_data_[packet_info.payload_start_pos], packet_info.size);

  // Return total length of written data.
  return packet_info.size + vp8_fixed_payload_descriptor_bytes_
      + extension_length;
}

int RtpFormatVp8::WriteExtensionFields(WebRtc_UWord8* buffer,
                                       int buffer_length) const {
  int extension_length = 0;
  if (XFieldPresent()) {
    WebRtc_UWord8* x_field = buffer + vp8_fixed_payload_descriptor_bytes_;
    *x_field = 0;
    extension_length = 1;  // One octet for the X field.
    if (PictureIdPresent()) {
      if (WritePictureIDFields(x_field, buffer, buffer_length,
                               &extension_length) < 0) {
        return -1;
      }
    }
    if (TL0PicIdxFieldPresent()) {
      if (WriteTl0PicIdxFields(x_field, buffer, buffer_length,
                               &extension_length) < 0) {
        return -1;
      }
    }
    if (TIDFieldPresent() || KeyIdxFieldPresent()) {
      if (WriteTIDAndKeyIdxFields(x_field, buffer, buffer_length,
                                  &extension_length) < 0) {
        return -1;
      }
    }
    assert(extension_length == PayloadDescriptorExtraLength());
  }
  return extension_length;
}

int RtpFormatVp8::WritePictureIDFields(WebRtc_UWord8* x_field,
                                       WebRtc_UWord8* buffer,
                                       int buffer_length,
                                       int* extension_length) const {
  *x_field |= kIBit;
  const int pic_id_length = WritePictureID(
      buffer + vp8_fixed_payload_descriptor_bytes_ + *extension_length,
      buffer_length - vp8_fixed_payload_descriptor_bytes_
      - *extension_length);
  if (pic_id_length < 0) return -1;
  *extension_length += pic_id_length;
  return 0;
}

int RtpFormatVp8::WritePictureID(WebRtc_UWord8* buffer,
                                 int buffer_length) const {
  const WebRtc_UWord16 pic_id =
      static_cast<WebRtc_UWord16> (hdr_info_.pictureId);
  int picture_id_len = PictureIdLength();
  if (picture_id_len > buffer_length) return -1;
  if (picture_id_len == 2) {
    buffer[0] = 0x80 | ((pic_id >> 8) & 0x7F);
    buffer[1] = pic_id & 0xFF;
  } else if (picture_id_len == 1) {
    buffer[0] = pic_id & 0x7F;
  }
  return picture_id_len;
}

int RtpFormatVp8::WriteTl0PicIdxFields(WebRtc_UWord8* x_field,
                                       WebRtc_UWord8* buffer,
                                       int buffer_length,
                                       int* extension_length) const {
  if (buffer_length < vp8_fixed_payload_descriptor_bytes_ + *extension_length
      + 1) {
    return -1;
  }
  *x_field |= kLBit;
  buffer[vp8_fixed_payload_descriptor_bytes_
         + *extension_length] = hdr_info_.tl0PicIdx;
  ++*extension_length;
  return 0;
}

int RtpFormatVp8::WriteTIDAndKeyIdxFields(WebRtc_UWord8* x_field,
                                          WebRtc_UWord8* buffer,
                                          int buffer_length,
                                          int* extension_length) const {
  if (buffer_length < vp8_fixed_payload_descriptor_bytes_ + *extension_length
      + 1) {
    return -1;
  }
  WebRtc_UWord8* data_field =
      &buffer[vp8_fixed_payload_descriptor_bytes_ + *extension_length];
  *data_field = 0;
  if (TIDFieldPresent()) {
    *x_field |= kTBit;
    assert(hdr_info_.temporalIdx >= 0 && hdr_info_.temporalIdx <= 3);
    *data_field |= hdr_info_.temporalIdx << 6;
    *data_field |= hdr_info_.layerSync ? kYBit : 0;
  }
  if (KeyIdxFieldPresent()) {
    *x_field |= kKBit;
    *data_field |= (hdr_info_.keyIdx & kKeyIdxField);
  }
  ++*extension_length;
  return 0;
}

int RtpFormatVp8::PayloadDescriptorExtraLength() const {
  int length_bytes = PictureIdLength();
  if (TL0PicIdxFieldPresent()) ++length_bytes;
  if (TIDFieldPresent() || KeyIdxFieldPresent()) ++length_bytes;
  if (length_bytes > 0) ++length_bytes;  // Include the extension field.
  return length_bytes;
}

int RtpFormatVp8::PictureIdLength() const {
  if (hdr_info_.pictureId == kNoPictureId) {
    return 0;
  }
  if (hdr_info_.pictureId <= 0x7F) {
    return 1;
  }
  return 2;
}

bool RtpFormatVp8::XFieldPresent() const {
  return (TIDFieldPresent() || TL0PicIdxFieldPresent() || PictureIdPresent()
      || KeyIdxFieldPresent());
}

bool RtpFormatVp8::TIDFieldPresent() const {
  assert((hdr_info_.layerSync == false) ||
         (hdr_info_.temporalIdx != kNoTemporalIdx));
  return (hdr_info_.temporalIdx != kNoTemporalIdx);
}

bool RtpFormatVp8::KeyIdxFieldPresent() const {
  return (hdr_info_.keyIdx != kNoKeyIdx);
}

bool RtpFormatVp8::TL0PicIdxFieldPresent() const {
  return (hdr_info_.tl0PicIdx != kNoTl0PicIdx);
}
}  // namespace webrtc
