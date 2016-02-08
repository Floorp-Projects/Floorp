/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_filter.h"

#include <iostream>
#include "gtest_utils.h"

namespace nss_test {

PacketFilter::Action TlsRecordFilter::Filter(const DataBuffer& input, DataBuffer* output) {
  bool changed = false;
  size_t offset = 0U;
  output->Allocate(input.len());

  TlsParser parser(input);
  while (parser.remaining()) {
    RecordHeader header;
    DataBuffer record;
    if (!header.Parse(&parser, &record)) {
      return KEEP;
    }

    DataBuffer filtered;
    PacketFilter::Action action = FilterRecord(header, record, &filtered);
    if (action == DROP) {
      changed = true;
      std::cerr << "record drop: " << record << std::endl;
      continue; // don't copy this one
    }

    const DataBuffer* source = &record;
    if (action == CHANGE) {
      EXPECT_GT(0x10000U, filtered.len());
      changed = true;
      std::cerr << "record old: " << record << std::endl;
      std::cerr << "record new: " << filtered << std::endl;
      source = &filtered;
    }

    offset = header.Write(output, offset, *source);
  }
  output->Truncate(offset);

  // Record how many packets we actually touched.
  if (changed) {
    ++count_;
    return (offset == 0) ? DROP : CHANGE;
  }

  return KEEP;
}

bool TlsRecordFilter::RecordHeader::Parse(TlsParser* parser, DataBuffer* body) {
  if (!parser->Read(&content_type_)) {
    return false;
  }

  uint32_t version;
  if (!parser->Read(&version, 2)) {
    return false;
  }
  version_ = version;

  sequence_number_ = 0;
  if (IsDtls(version)) {
    uint32_t tmp;
    if (!parser->Read(&tmp, 4)) {
      return false;
    }
    sequence_number_ = static_cast<uint64_t>(tmp) << 32;
    if (!parser->Read(&tmp, 4)) {
      return false;
    }
    sequence_number_ |= static_cast<uint64_t>(tmp);
  }
  return parser->ReadVariable(body, 2);
}

size_t TlsRecordFilter::RecordHeader::Write(
    DataBuffer* buffer, size_t offset, const DataBuffer& body) const {
  offset = buffer->Write(offset, content_type_, 1);
  offset = buffer->Write(offset, version_, 2);
  if (is_dtls()) {
    // write epoch (2 octet), and seqnum (6 octet)
    offset = buffer->Write(offset, sequence_number_ >> 32, 4);
    offset = buffer->Write(offset, sequence_number_ & 0xffffffff, 4);
  }
  offset = buffer->Write(offset, body.len(), 2);
  offset = buffer->Write(offset, body);
  return offset;
}

PacketFilter::Action TlsHandshakeFilter::FilterRecord(
    const RecordHeader& record_header, const DataBuffer& input,
    DataBuffer* output) {
  // Check that the first byte is as requested.
  if (record_header.content_type() != kTlsHandshakeType) {
    return KEEP;
  }

  bool changed = false;
  size_t offset = 0U;
  output->Allocate(input.len()); // Preallocate a little.

  TlsParser parser(input);
  while (parser.remaining()) {
    HandshakeHeader header;
    DataBuffer handshake;
    if (!header.Parse(&parser, record_header, &handshake)) {
      return KEEP;
    }

    DataBuffer filtered;
    PacketFilter::Action action = FilterHandshake(header, handshake, &filtered);
    if (action == DROP) {
      changed = true;
      std::cerr << "handshake drop: " << handshake << std::endl;
      continue;
    }

    const DataBuffer* source = &handshake;
    if (action == CHANGE) {
      EXPECT_GT(0x1000000U, filtered.len());
      changed = true;
      std::cerr << "handshake old: " << handshake << std::endl;
      std::cerr << "handshake new: " << filtered << std::endl;
      source = &filtered;
    }

    offset = header.Write(output, offset, *source);
  }
  output->Truncate(offset);
  return changed ? (offset ? CHANGE : DROP) : KEEP;
}

bool TlsHandshakeFilter::HandshakeHeader::ReadLength(TlsParser* parser,
                                                     const RecordHeader& header,
                                                     uint32_t *length) {
  if (!parser->Read(length, 3)) {
    return false; // malformed
  }

  if (!header.is_dtls()) {
    return true; // nothing left to do
  }

  // Read and check DTLS parameters
  uint32_t message_seq_tmp;
  if (!parser->Read(&message_seq_tmp, 2)) { // sequence number
    return false;
  }
  message_seq_ = message_seq_tmp;

  uint32_t fragment_offset;
  if (!parser->Read(&fragment_offset, 3)) {
    return false;
  }

  uint32_t fragment_length;
  if (!parser->Read(&fragment_length, 3)) {
    return false;
  }

  // All current tests where we are using this code don't fragment.
  return (fragment_offset == 0 && fragment_length == *length);
}

bool TlsHandshakeFilter::HandshakeHeader::Parse(
    TlsParser* parser, const RecordHeader& record_header,
    DataBuffer* body) {

  version_ = record_header.version();
  if (!parser->Read(&handshake_type_)) {
    return false; // malformed
  }
  uint32_t length;
  if (!ReadLength(parser, record_header, &length)) {
    return false;
  }

  return parser->Read(body, length);
}

size_t TlsHandshakeFilter::HandshakeHeader::Write(
    DataBuffer* buffer, size_t offset, const DataBuffer& body) const {
    offset = buffer->Write(offset, handshake_type(), 1);
    offset = buffer->Write(offset, body.len(), 3);
    if (is_dtls()) {
      offset = buffer->Write(offset, message_seq_, 2);
      offset = buffer->Write(offset, 0U, 3); // fragment_offset
      offset = buffer->Write(offset, body.len(), 3);
    }
    offset = buffer->Write(offset, body);
    return offset;
}

PacketFilter::Action TlsInspectorRecordHandshakeMessage::FilterHandshake(
    const HandshakeHeader& header,
    const DataBuffer& input, DataBuffer* output) {
  // Only do this once.
  if (buffer_.len()) {
    return KEEP;
  }

  if (header.handshake_type() == handshake_type_) {
    buffer_ = input;
  }
  return KEEP;
}


PacketFilter::Action TlsInspectorReplaceHandshakeMessage::FilterHandshake(
    const HandshakeHeader& header,
    const DataBuffer& input, DataBuffer* output) {
  if (header.handshake_type() == handshake_type_) {
    *output = buffer_;
    return CHANGE;
  }

  return KEEP;
}

PacketFilter::Action TlsAlertRecorder::FilterRecord(
    const RecordHeader& header, const DataBuffer& input, DataBuffer* output) {
  if (level_ == kTlsAlertFatal) { // already fatal
    return KEEP;
  }
  if (header.content_type() != kTlsAlertType) {
    return KEEP;
  }

  std::cerr << "Alert: " << input << std::endl;

  TlsParser parser(input);
  uint8_t lvl;
  if (!parser.Read(&lvl)) {
    return KEEP;
  }
  if (lvl == kTlsAlertWarning) { // not strong enough
    return KEEP;
  }
  level_ = lvl;
  (void)parser.Read(&description_);
  return KEEP;
}

ChainedPacketFilter::~ChainedPacketFilter() {
  for (auto it = filters_.begin(); it != filters_.end(); ++it) {
    delete *it;
  }
}

PacketFilter::Action ChainedPacketFilter::Filter(const DataBuffer& input,
                                                 DataBuffer* output) {
  DataBuffer in(input);
  bool changed = false;
  for (auto it = filters_.begin(); it != filters_.end(); ++it) {
    PacketFilter::Action action = (*it)->Filter(in, output);
    if (action == DROP) {
      return DROP;
    }
    if (action == CHANGE) {
      in = *output;
      changed = true;
    }
  }
  return changed ? CHANGE : KEEP;
}

}  // namespace nss_test
