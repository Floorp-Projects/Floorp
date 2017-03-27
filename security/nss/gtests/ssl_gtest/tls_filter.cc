/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_filter.h"
#include "sslproto.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include <iostream>
#include "gtest_utils.h"
#include "tls_agent.h"
#include "tls_filter.h"
#include "tls_protect.h"

namespace nss_test {

void TlsVersioned::WriteStream(std::ostream& stream) const {
  stream << (is_dtls() ? "DTLS " : "TLS ");
  switch (version()) {
    case 0:
      stream << "(no version)";
      break;
    case SSL_LIBRARY_VERSION_TLS_1_0:
      stream << "1.0";
      break;
    case SSL_LIBRARY_VERSION_DTLS_1_0_WIRE:
    case SSL_LIBRARY_VERSION_TLS_1_1:
      stream << (is_dtls() ? "1.0" : "1.1");
      break;
    case SSL_LIBRARY_VERSION_DTLS_1_2_WIRE:
    case SSL_LIBRARY_VERSION_TLS_1_2:
      stream << "1.2";
      break;
    case SSL_LIBRARY_VERSION_TLS_1_3:
      stream << "1.3";
      break;
    default:
      stream << "Invalid version: " << version();
      break;
  }
}

void TlsRecordFilter::EnableDecryption() {
  SSLInt_SetCipherSpecChangeFunc(agent()->ssl_fd(), CipherSpecChanged,
                                 (void*)this);
}

void TlsRecordFilter::CipherSpecChanged(void* arg, PRBool sending,
                                        ssl3CipherSpec* newSpec) {
  TlsRecordFilter* self = static_cast<TlsRecordFilter*>(arg);
  PRBool isServer = self->agent()->role() == TlsAgent::SERVER;

  if (g_ssl_gtest_verbose) {
    std::cerr << "Cipher spec changed. Role="
              << (isServer ? "server" : "client")
              << " direction=" << (sending ? "send" : "receive") << std::endl;
  }
  if (!sending) return;

  self->cipher_spec_.reset(new TlsCipherSpec());
  bool ret =
      self->cipher_spec_->Init(SSLInt_CipherSpecToAlgorithm(isServer, newSpec),
                               SSLInt_CipherSpecToKey(isServer, newSpec),
                               SSLInt_CipherSpecToIv(isServer, newSpec));
  EXPECT_EQ(true, ret);
}

PacketFilter::Action TlsRecordFilter::Filter(const DataBuffer& input,
                                             DataBuffer* output) {
  bool changed = false;
  size_t offset = 0U;
  output->Allocate(input.len());

  TlsParser parser(input);

  while (parser.remaining()) {
    TlsRecordHeader header;
    DataBuffer record;

    if (!header.Parse(&parser, &record)) {
      ADD_FAILURE() << "not a valid record";
      return KEEP;
    }

    if (FilterRecord(header, record, &offset, output) != KEEP) {
      changed = true;
    } else {
      offset = header.Write(output, offset, record);
    }
  }
  output->Truncate(offset);

  // Record how many packets we actually touched.
  if (changed) {
    ++count_;
    return (offset == 0) ? DROP : CHANGE;
  }

  return KEEP;
}

PacketFilter::Action TlsRecordFilter::FilterRecord(
    const TlsRecordHeader& header, const DataBuffer& record, size_t* offset,
    DataBuffer* output) {
  DataBuffer filtered;
  uint8_t inner_content_type;
  DataBuffer plaintext;

  if (!Unprotect(header, record, &inner_content_type, &plaintext)) {
    return KEEP;
  }

  TlsRecordHeader real_header = {header.version(), inner_content_type,
                                 header.sequence_number()};

  PacketFilter::Action action = FilterRecord(real_header, plaintext, &filtered);
  if (action == KEEP) {
    return KEEP;
  }

  if (action == DROP) {
    std::cerr << "record drop: " << record << std::endl;
    return DROP;
  }

  EXPECT_GT(0x10000U, filtered.len());
  std::cerr << "record old: " << plaintext << std::endl;
  std::cerr << "record new: " << filtered << std::endl;

  DataBuffer ciphertext;
  bool rv = Protect(header, inner_content_type, filtered, &ciphertext);
  EXPECT_TRUE(rv);
  if (!rv) {
    return KEEP;
  }
  *offset = header.Write(output, *offset, ciphertext);
  return CHANGE;
}

bool TlsRecordHeader::Parse(TlsParser* parser, DataBuffer* body) {
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

size_t TlsRecordHeader::Write(DataBuffer* buffer, size_t offset,
                              const DataBuffer& body) const {
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

bool TlsRecordFilter::Unprotect(const TlsRecordHeader& header,
                                const DataBuffer& ciphertext,
                                uint8_t* inner_content_type,
                                DataBuffer* plaintext) {
  if (!cipher_spec_ || header.content_type() != kTlsApplicationDataType) {
    *inner_content_type = header.content_type();
    *plaintext = ciphertext;
    return true;
  }

  if (!cipher_spec_->Unprotect(header, ciphertext, plaintext)) return false;

  size_t len = plaintext->len();
  while (len > 0 && !plaintext->data()[len - 1]) {
    --len;
  }
  if (!len) {
    // Bogus padding.
    return false;
  }

  *inner_content_type = plaintext->data()[len - 1];
  plaintext->Truncate(len - 1);

  return true;
}

bool TlsRecordFilter::Protect(const TlsRecordHeader& header,
                              uint8_t inner_content_type,
                              const DataBuffer& plaintext,
                              DataBuffer* ciphertext) {
  if (!cipher_spec_ || header.content_type() != kTlsApplicationDataType) {
    *ciphertext = plaintext;
    return true;
  }
  DataBuffer padded = plaintext;
  padded.Write(padded.len(), inner_content_type, 1);
  return cipher_spec_->Protect(header, padded, ciphertext);
}

PacketFilter::Action TlsHandshakeFilter::FilterRecord(
    const TlsRecordHeader& record_header, const DataBuffer& input,
    DataBuffer* output) {
  // Check that the first byte is as requested.
  if (record_header.content_type() != kTlsHandshakeType) {
    return KEEP;
  }

  bool changed = false;
  size_t offset = 0U;
  output->Allocate(input.len());  // Preallocate a little.

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

bool TlsHandshakeFilter::HandshakeHeader::ReadLength(
    TlsParser* parser, const TlsRecordHeader& header, uint32_t* length) {
  if (!parser->Read(length, 3)) {
    return false;  // malformed
  }

  if (!header.is_dtls()) {
    return true;  // nothing left to do
  }

  // Read and check DTLS parameters
  uint32_t message_seq_tmp;
  if (!parser->Read(&message_seq_tmp, 2)) {  // sequence number
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
    TlsParser* parser, const TlsRecordHeader& record_header, DataBuffer* body) {
  version_ = record_header.version();
  if (!parser->Read(&handshake_type_)) {
    return false;  // malformed
  }
  uint32_t length;
  if (!ReadLength(parser, record_header, &length)) {
    return false;
  }

  return parser->Read(body, length);
}

size_t TlsHandshakeFilter::HandshakeHeader::WriteFragment(
    DataBuffer* buffer, size_t offset, const DataBuffer& body,
    size_t fragment_offset, size_t fragment_length) const {
  EXPECT_TRUE(is_dtls());
  EXPECT_GE(body.len(), fragment_offset + fragment_length);
  offset = buffer->Write(offset, handshake_type(), 1);
  offset = buffer->Write(offset, body.len(), 3);
  offset = buffer->Write(offset, message_seq_, 2);
  offset = buffer->Write(offset, fragment_offset, 3);
  offset = buffer->Write(offset, fragment_length, 3);
  offset =
      buffer->Write(offset, body.data() + fragment_offset, fragment_length);
  return offset;
}

size_t TlsHandshakeFilter::HandshakeHeader::Write(
    DataBuffer* buffer, size_t offset, const DataBuffer& body) const {
  if (is_dtls()) {
    return WriteFragment(buffer, offset, body, 0U, body.len());
  }
  offset = buffer->Write(offset, handshake_type(), 1);
  offset = buffer->Write(offset, body.len(), 3);
  offset = buffer->Write(offset, body);
  return offset;
}

PacketFilter::Action TlsInspectorRecordHandshakeMessage::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
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
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  if (header.handshake_type() == handshake_type_) {
    *output = buffer_;
    return CHANGE;
  }

  return KEEP;
}

PacketFilter::Action TlsConversationRecorder::FilterRecord(
    const TlsRecordHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  buffer_.Append(input);
  return KEEP;
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

PacketFilter::Action TlsExtensionFilter::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  if (header.handshake_type() == kTlsHandshakeClientHello) {
    TlsParser parser(input);
    if (!FindClientHelloExtensions(&parser, header)) {
      return KEEP;
    }
    return FilterExtensions(&parser, input, output);
  }
  if (header.handshake_type() == kTlsHandshakeServerHello) {
    TlsParser parser(input);
    if (!FindServerHelloExtensions(&parser)) {
      return KEEP;
    }
    return FilterExtensions(&parser, input, output);
  }
  return KEEP;
}

bool TlsExtensionFilter::FindClientHelloExtensions(TlsParser* parser,
                                                   const TlsVersioned& header) {
  if (!parser->Skip(2 + 32)) {  // version + random
    return false;
  }
  if (!parser->SkipVariable(1)) {  // session ID
    return false;
  }
  if (header.is_dtls() && !parser->SkipVariable(1)) {  // DTLS cookie
    return false;
  }
  if (!parser->SkipVariable(2)) {  // cipher suites
    return false;
  }
  if (!parser->SkipVariable(1)) {  // compression methods
    return false;
  }
  return true;
}

bool TlsExtensionFilter::FindServerHelloExtensions(TlsParser* parser) {
  uint32_t vtmp;
  if (!parser->Read(&vtmp, 2)) {
    return false;
  }
  uint16_t version = static_cast<uint16_t>(vtmp);
  if (!parser->Skip(32)) {  // random
    return false;
  }
  if (NormalizeTlsVersion(version) <= SSL_LIBRARY_VERSION_TLS_1_2) {
    if (!parser->SkipVariable(1)) {  // session ID
      return false;
    }
  }
  if (!parser->Skip(2)) {  // cipher suite
    return false;
  }
  if (NormalizeTlsVersion(version) <= SSL_LIBRARY_VERSION_TLS_1_2) {
    if (!parser->Skip(1)) {  // compression method
      return false;
    }
  }
  return true;
}

PacketFilter::Action TlsExtensionFilter::FilterExtensions(
    TlsParser* parser, const DataBuffer& input, DataBuffer* output) {
  size_t length_offset = parser->consumed();
  uint32_t all_extensions;
  if (!parser->Read(&all_extensions, 2)) {
    return KEEP;  // no extensions, odd but OK
  }
  if (all_extensions != parser->remaining()) {
    return KEEP;  // malformed
  }

  bool changed = false;

  // Write out the start of the message.
  output->Allocate(input.len());
  size_t offset = output->Write(0, input.data(), parser->consumed());

  while (parser->remaining()) {
    uint32_t extension_type;
    if (!parser->Read(&extension_type, 2)) {
      return KEEP;  // malformed
    }

    DataBuffer extension;
    if (!parser->ReadVariable(&extension, 2)) {
      return KEEP;  // malformed
    }

    DataBuffer filtered;
    PacketFilter::Action action =
        FilterExtension(extension_type, extension, &filtered);
    if (action == DROP) {
      changed = true;
      std::cerr << "extension drop: " << extension << std::endl;
      continue;
    }

    const DataBuffer* source = &extension;
    if (action == CHANGE) {
      EXPECT_GT(0x10000U, filtered.len());
      changed = true;
      std::cerr << "extension old: " << extension << std::endl;
      std::cerr << "extension new: " << filtered << std::endl;
      source = &filtered;
    }

    // Write out extension.
    offset = output->Write(offset, extension_type, 2);
    offset = output->Write(offset, source->len(), 2);
    if (source->len() > 0) {
      offset = output->Write(offset, *source);
    }
  }
  output->Truncate(offset);

  if (changed) {
    size_t newlen = output->len() - length_offset - 2;
    EXPECT_GT(0x10000U, newlen);
    if (newlen >= 0x10000) {
      return KEEP;  // bad: size increased too much
    }
    output->Write(length_offset, newlen, 2);
    return CHANGE;
  }
  return KEEP;
}

PacketFilter::Action TlsExtensionCapture::FilterExtension(
    uint16_t extension_type, const DataBuffer& input, DataBuffer* output) {
  if (extension_type == extension_ && (last_ || !captured_)) {
    data_.Assign(input);
    captured_ = true;
  }
  return KEEP;
}

PacketFilter::Action TlsExtensionReplacer::FilterExtension(
    uint16_t extension_type, const DataBuffer& input, DataBuffer* output) {
  if (extension_type != extension_) {
    return KEEP;
  }

  *output = data_;
  return CHANGE;
}

PacketFilter::Action TlsExtensionDropper::FilterExtension(
    uint16_t extension_type, const DataBuffer& input, DataBuffer* output) {
  if (extension_type == extension_) {
    return DROP;
  }
  return KEEP;
}

PacketFilter::Action AfterRecordN::FilterRecord(const TlsRecordHeader& header,
                                                const DataBuffer& body,
                                                DataBuffer* out) {
  if (counter_++ == record_) {
    DataBuffer buf;
    header.Write(&buf, 0, body);
    src_.lock()->SendDirect(buf);
    dest_.lock()->Handshake();
    func_();
    return DROP;
  }

  return KEEP;
}

PacketFilter::Action TlsInspectorClientHelloVersionChanger::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  if (header.handshake_type() == kTlsHandshakeClientKeyExchange) {
    EXPECT_EQ(SECSuccess,
              SSLInt_IncrementClientHandshakeVersion(server_.lock()->ssl_fd()));
  }
  return KEEP;
}

PacketFilter::Action SelectiveDropFilter::Filter(const DataBuffer& input,
                                                 DataBuffer* output) {
  if (counter_ >= 32) {
    return KEEP;
  }
  return ((1 << counter_++) & pattern_) ? DROP : KEEP;
}

PacketFilter::Action TlsInspectorClientHelloVersionSetter::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  if (header.handshake_type() == kTlsHandshakeClientHello) {
    *output = input;
    output->Write(0, version_, 2);
    return CHANGE;
  }
  return KEEP;
}

}  // namespace nss_test
