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

#include <cassert>
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
    case SSL_LIBRARY_VERSION_TLS_1_1:
      stream << (is_dtls() ? "1.0" : "1.1");
      break;
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
    std::cerr << (isServer ? "server" : "client") << ": "
              << (sending ? "send" : "receive")
              << " cipher spec changed:  " << newSpec->epoch << " ("
              << newSpec->phase << ")" << std::endl;
  }
  if (!sending) {
    return;
  }

  uint64_t seq_no;
  if (self->agent()->variant() == ssl_variant_datagram) {
    seq_no = static_cast<uint64_t>(SSLInt_CipherSpecToEpoch(newSpec)) << 48;
  } else {
    seq_no = 0;
  }
  self->in_sequence_number_ = seq_no;
  self->out_sequence_number_ = seq_no;
  self->dropped_record_ = false;
  self->cipher_spec_.reset(new TlsCipherSpec());
  bool ret = self->cipher_spec_->Init(
      SSLInt_CipherSpecToEpoch(newSpec), SSLInt_CipherSpecToAlgorithm(newSpec),
      SSLInt_CipherSpecToKey(newSpec), SSLInt_CipherSpecToIv(newSpec));
  EXPECT_EQ(true, ret);
}

bool TlsRecordFilter::is_dtls13() const {
  if (agent()->variant() != ssl_variant_datagram) {
    return false;
  }
  if (agent()->state() == TlsAgent::STATE_CONNECTED) {
    return agent()->version() >= SSL_LIBRARY_VERSION_TLS_1_3;
  }
  SSLPreliminaryChannelInfo info;
  EXPECT_EQ(SECSuccess, SSL_GetPreliminaryChannelInfo(agent()->ssl_fd(), &info,
                                                      sizeof(info)));
  return (info.protocolVersion >= SSL_LIBRARY_VERSION_TLS_1_3) ||
         info.canSendEarlyData;
}

PacketFilter::Action TlsRecordFilter::Filter(const DataBuffer& input,
                                             DataBuffer* output) {
  // Disable during shutdown.
  if (!agent()) {
    return KEEP;
  }

  bool changed = false;
  size_t offset = 0U;

  output->Allocate(input.len());
  TlsParser parser(input);

  while (parser.remaining()) {
    TlsRecordHeader header;
    DataBuffer record;

    if (!header.Parse(is_dtls13(), in_sequence_number_, &parser, &record)) {
      ADD_FAILURE() << "not a valid record";
      return KEEP;
    }

    // Track the sequence number, which is necessary for stream mode when
    // decrypting and for TLS 1.3 datagram to recover the sequence number.
    //
    // We reset the counter when the cipher spec changes, but that notification
    // appears before a record is sent.  If multiple records are sent with
    // different cipher specs, this would fail.  This filters out cleartext
    // records, so we don't get confused by handshake messages that are sent at
    // the same time as encrypted records.  Sequence numbers are therefore
    // likely to be incorrect for cleartext records.
    //
    // This isn't perfectly robust: if there is a change from an active cipher
    // spec to another active cipher spec (KeyUpdate for instance) AND writes
    // are consolidated across that change, this code could use the wrong
    // sequence numbers when re-encrypting records with the old keys.
    if (header.content_type() == kTlsApplicationDataType) {
      in_sequence_number_ =
          (std::max)(in_sequence_number_, header.sequence_number() + 1);
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
    if (g_ssl_gtest_verbose) {
      std::cerr << "unprotect failed: " << header << ":" << record << std::endl;
    }
    return KEEP;
  }

  TlsRecordHeader real_header(header.variant(), header.version(),
                              inner_content_type, header.sequence_number());

  PacketFilter::Action action = FilterRecord(real_header, plaintext, &filtered);
  // In stream mode, even if something doesn't change we need to re-encrypt if
  // previous packets were dropped.
  if (action == KEEP) {
    if (header.is_dtls() || !dropped_record_) {
      return KEEP;
    }
    filtered = plaintext;
  }

  if (action == DROP) {
    std::cerr << "record drop: " << header << ":" << record << std::endl;
    dropped_record_ = true;
    return DROP;
  }

  EXPECT_GT(0x10000U, filtered.len());
  if (action != KEEP) {
    std::cerr << "record old: " << plaintext << std::endl;
    std::cerr << "record new: " << filtered << std::endl;
  }

  uint64_t seq_num;
  if (header.is_dtls() || !cipher_spec_ ||
      header.content_type() != kTlsApplicationDataType) {
    seq_num = header.sequence_number();
  } else {
    seq_num = out_sequence_number_++;
  }
  TlsRecordHeader out_header(header.variant(), header.version(),
                             header.content_type(), seq_num);

  DataBuffer ciphertext;
  bool rv = Protect(out_header, inner_content_type, filtered, &ciphertext);
  EXPECT_TRUE(rv);
  if (!rv) {
    return KEEP;
  }
  *offset = out_header.Write(output, *offset, ciphertext);
  return CHANGE;
}

size_t TlsRecordHeader::header_length() const {
  // If we have a header, return it's length.
  if (header_.len()) {
    return header_.len();
  }

  // Otherwise make a dummy header and return the length.
  DataBuffer buf;
  return WriteHeader(&buf, 0, 0);
}

uint64_t TlsRecordHeader::RecoverSequenceNumber(uint64_t expected,
                                                uint32_t partial,
                                                size_t partial_bits) {
  EXPECT_GE(32U, partial_bits);
  uint64_t mask = (1 << partial_bits) - 1;
  // First we determine the highest possible value.  This is half the
  // expressible range above the expected value.
  uint64_t cap = expected + (1ULL << (partial_bits - 1));
  // Add the partial piece in.  e.g., xxxx789a and 1234 becomes xxxx1234.
  uint64_t seq_no = (cap & ~mask) | partial;
  // If the partial value is higher than the same partial piece from the cap,
  // then the real value has to be lower.  e.g., xxxx1234 can't become xxxx5678.
  if (partial > (cap & mask)) {
    seq_no -= 1ULL << partial_bits;
  }
  return seq_no;
}

// Determine the full epoch and sequence number from an expected and raw value.
// The expected and output values are packed as they are in DTLS 1.2 and
// earlier: with 16 bits of epoch and 48 bits of sequence number.
uint64_t TlsRecordHeader::ParseSequenceNumber(uint64_t expected, uint32_t raw,
                                              size_t seq_no_bits,
                                              size_t epoch_bits) {
  uint64_t epoch_mask = (1ULL << epoch_bits) - 1;
  uint64_t epoch = RecoverSequenceNumber(
      expected >> 48, (raw >> seq_no_bits) & epoch_mask, epoch_bits);
  if (epoch > (expected >> 48)) {
    // If the epoch has changed, reset the expected sequence number.
    expected = 0;
  } else {
    // Otherwise, retain just the sequence number part.
    expected &= (1ULL << 48) - 1;
  }
  uint64_t seq_no_mask = (1ULL << seq_no_bits) - 1;
  uint64_t seq_no =
      RecoverSequenceNumber(expected, raw & seq_no_mask, seq_no_bits);
  return (epoch << 48) | seq_no;
}

bool TlsRecordHeader::Parse(bool is_dtls13, uint64_t seqno, TlsParser* parser,
                            DataBuffer* body) {
  auto mark = parser->consumed();

  if (!parser->Read(&content_type_)) {
    return false;
  }

  if (is_dtls13) {
    variant_ = ssl_variant_datagram;
    version_ = SSL_LIBRARY_VERSION_TLS_1_3;

#ifndef UNSAFE_FUZZER_MODE
    // Deal with the 7 octet header.
    if (content_type_ == kTlsApplicationDataType) {
      uint32_t tmp;
      if (!parser->Read(&tmp, 4)) {
        return false;
      }
      sequence_number_ = ParseSequenceNumber(seqno, tmp, 30, 2);
      if (!parser->ReadFromMark(&header_, parser->consumed() + 2 - mark,
                                mark)) {
        return false;
      }
      return parser->ReadVariable(body, 2);
    }

    // The short, 2 octet header.
    if ((content_type_ & 0xe0) == 0x20) {
      uint32_t tmp;
      if (!parser->Read(&tmp, 1)) {
        return false;
      }
      // Need to use the low 5 bits of the first octet too.
      tmp |= (content_type_ & 0x1f) << 8;
      content_type_ = kTlsApplicationDataType;
      sequence_number_ = ParseSequenceNumber(seqno, tmp, 12, 1);

      if (!parser->ReadFromMark(&header_, parser->consumed() - mark, mark)) {
        return false;
      }
      return parser->Read(body, parser->remaining());
    }

    // The full 13 octet header can only be used for a few types.
    EXPECT_TRUE(content_type_ == kTlsAlertType ||
                content_type_ == kTlsHandshakeType ||
                content_type_ == kTlsAckType);
#endif
  }

  uint32_t ver;
  if (!parser->Read(&ver, 2)) {
    return false;
  }
  if (!is_dtls13) {
    variant_ = IsDtls(ver) ? ssl_variant_datagram : ssl_variant_stream;
  }
  version_ = NormalizeTlsVersion(ver);

  if (is_dtls()) {
    // If this is DTLS, read the sequence number.
    uint32_t tmp;
    if (!parser->Read(&tmp, 4)) {
      return false;
    }
    sequence_number_ = static_cast<uint64_t>(tmp) << 32;
    if (!parser->Read(&tmp, 4)) {
      return false;
    }
    sequence_number_ |= static_cast<uint64_t>(tmp);
  } else {
    sequence_number_ = seqno;
  }
  if (!parser->ReadFromMark(&header_, parser->consumed() + 2 - mark, mark)) {
    return false;
  }
  return parser->ReadVariable(body, 2);
}

size_t TlsRecordHeader::WriteHeader(DataBuffer* buffer, size_t offset,
                                    size_t body_len) const {
  offset = buffer->Write(offset, content_type_, 1);
  if (is_dtls() && version_ >= SSL_LIBRARY_VERSION_TLS_1_3 &&
      content_type() == kTlsApplicationDataType) {
    // application_data records in TLS 1.3 have a different header format.
    // Always use the long header here for simplicity.
    uint32_t e = (sequence_number_ >> 48) & 0x3;
    uint32_t seqno = sequence_number_ & ((1ULL << 30) - 1);
    offset = buffer->Write(offset, (e << 30) | seqno, 4);
  } else {
    uint16_t v = is_dtls() ? TlsVersionToDtlsVersion(version_) : version_;
    offset = buffer->Write(offset, v, 2);
    if (is_dtls()) {
      // write epoch (2 octet), and seqnum (6 octet)
      offset = buffer->Write(offset, sequence_number_ >> 32, 4);
      offset = buffer->Write(offset, sequence_number_ & 0xffffffff, 4);
    }
  }
  offset = buffer->Write(offset, body_len, 2);
  return offset;
}

size_t TlsRecordHeader::Write(DataBuffer* buffer, size_t offset,
                              const DataBuffer& body) const {
  offset = WriteHeader(buffer, offset, body.len());
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

  if (!cipher_spec_->Unprotect(header, ciphertext, plaintext)) {
    return false;
  }

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
  if (g_ssl_gtest_verbose) {
    std::cerr << "unprotect: " << std::hex << header.sequence_number()
              << std::dec << " type=" << static_cast<int>(*inner_content_type)
              << " " << *plaintext << std::endl;
  }

  return true;
}

bool TlsRecordFilter::Protect(const TlsRecordHeader& header,
                              uint8_t inner_content_type,
                              const DataBuffer& plaintext,
                              DataBuffer* ciphertext, size_t padding) {
  if (!cipher_spec_ || header.content_type() != kTlsApplicationDataType) {
    *ciphertext = plaintext;
    return true;
  }
  if (g_ssl_gtest_verbose) {
    std::cerr << "protect: " << header.sequence_number() << std::endl;
  }
  DataBuffer padded;
  padded.Allocate(plaintext.len() + 1 + padding);
  size_t offset = padded.Write(0, plaintext.data(), plaintext.len());
  padded.Write(offset, inner_content_type, 1);
  return cipher_spec_->Protect(header, padded, ciphertext);
}

bool IsHelloRetry(const DataBuffer& body) {
  static const uint8_t ssl_hello_retry_random[] = {
      0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11, 0xBE, 0x1D, 0x8C,
      0x02, 0x1E, 0x65, 0xB8, 0x91, 0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB,
      0x8C, 0x5E, 0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C};
  return memcmp(body.data() + 2, ssl_hello_retry_random,
                sizeof(ssl_hello_retry_random)) == 0;
}

bool TlsHandshakeFilter::IsFilteredType(const HandshakeHeader& header,
                                        const DataBuffer& body) {
  if (handshake_types_.empty()) {
    return true;
  }

  uint8_t type = header.handshake_type();
  if (type == kTlsHandshakeServerHello) {
    if (IsHelloRetry(body)) {
      type = kTlsHandshakeHelloRetryRequest;
    }
  }
  return handshake_types_.count(type) > 0U;
}

PacketFilter::Action TlsHandshakeFilter::FilterRecord(
    const TlsRecordHeader& record_header, const DataBuffer& input,
    DataBuffer* output) {
  // Check that the first byte is as requested.
  if ((record_header.content_type() != kTlsHandshakeType) &&
      (record_header.content_type() != kTlsAltHandshakeType)) {
    return KEEP;
  }

  bool changed = false;
  size_t offset = 0U;
  output->Allocate(input.len());  // Preallocate a little.

  TlsParser parser(input);
  while (parser.remaining()) {
    HandshakeHeader header;
    DataBuffer handshake;
    bool complete = false;
    if (!header.Parse(&parser, record_header, preceding_fragment_, &handshake,
                      &complete)) {
      return KEEP;
    }

    if (!complete) {
      EXPECT_TRUE(record_header.is_dtls());
      // Save the fragment and drop it from this record.  Fragments are
      // coalesced with the last fragment of the handshake message.
      changed = true;
      preceding_fragment_.Assign(handshake);
      continue;
    }
    preceding_fragment_.Truncate(0);

    DataBuffer filtered;
    PacketFilter::Action action;
    if (!IsFilteredType(header, handshake)) {
      action = KEEP;
    } else {
      action = FilterHandshake(header, handshake, &filtered);
    }
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
    } else if (preceding_fragment_.len()) {
      changed = true;
    }

    offset = header.Write(output, offset, *source);
  }
  output->Truncate(offset);
  return changed ? (offset ? CHANGE : DROP) : KEEP;
}

bool TlsHandshakeFilter::HandshakeHeader::ReadLength(
    TlsParser* parser, const TlsRecordHeader& header, uint32_t expected_offset,
    uint32_t* length, bool* last_fragment) {
  uint32_t message_length;
  if (!parser->Read(&message_length, 3)) {
    return false;  // malformed
  }

  if (!header.is_dtls()) {
    *last_fragment = true;
    *length = message_length;
    return true;  // nothing left to do
  }

  // Read and check DTLS parameters
  uint32_t message_seq_tmp;
  if (!parser->Read(&message_seq_tmp, 2)) {  // sequence number
    return false;
  }
  message_seq_ = message_seq_tmp;

  uint32_t offset = 0;
  if (!parser->Read(&offset, 3)) {
    return false;
  }
  // We only parse if the fragments are all complete and in order.
  if (offset != expected_offset) {
    EXPECT_NE(0U, header.epoch())
        << "Received out of order handshake fragment for epoch 0";
    return false;
  }

  // For DTLS, we return the length of just this fragment.
  if (!parser->Read(length, 3)) {
    return false;
  }

  // It's a fragment if the entire message is longer than what we have.
  *last_fragment = message_length == (*length + offset);
  return true;
}

bool TlsHandshakeFilter::HandshakeHeader::Parse(
    TlsParser* parser, const TlsRecordHeader& record_header,
    const DataBuffer& preceding_fragment, DataBuffer* body, bool* complete) {
  *complete = false;

  variant_ = record_header.variant();
  version_ = record_header.version();
  if (!parser->Read(&handshake_type_)) {
    return false;  // malformed
  }

  uint32_t length;
  if (!ReadLength(parser, record_header, preceding_fragment.len(), &length,
                  complete)) {
    return false;
  }

  if (!parser->Read(body, length)) {
    return false;
  }
  if (preceding_fragment.len()) {
    body->Splice(preceding_fragment, 0);
  }
  return true;
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

PacketFilter::Action TlsHandshakeRecorder::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  // Only do this once.
  if (buffer_.len()) {
    return KEEP;
  }

  buffer_ = input;
  return KEEP;
}

PacketFilter::Action TlsInspectorReplaceHandshakeMessage::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  *output = buffer_;
  return CHANGE;
}

PacketFilter::Action TlsRecordRecorder::FilterRecord(
    const TlsRecordHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  if (!filter_ || (header.content_type() == ct_)) {
    records_.push_back({header, input});
  }
  return KEEP;
}

PacketFilter::Action TlsConversationRecorder::FilterRecord(
    const TlsRecordHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  buffer_.Append(input);
  return KEEP;
}

PacketFilter::Action TlsHeaderRecorder::FilterRecord(const TlsRecordHeader& hdr,
                                                     const DataBuffer& input,
                                                     DataBuffer* output) {
  headers_.push_back(hdr);
  return KEEP;
}

const TlsRecordHeader* TlsHeaderRecorder::header(size_t index) {
  if (index > headers_.size() + 1) {
    return nullptr;
  }
  return &headers_[index];
}

PacketFilter::Action ChainedPacketFilter::Filter(const DataBuffer& input,
                                                 DataBuffer* output) {
  DataBuffer in(input);
  bool changed = false;
  for (auto it = filters_.begin(); it != filters_.end(); ++it) {
    PacketFilter::Action action = (*it)->Process(in, output);
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

bool FindClientHelloExtensions(TlsParser* parser, const TlsVersioned& header) {
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

bool FindServerHelloExtensions(TlsParser* parser, const TlsVersioned& header) {
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

bool FindEncryptedExtensions(TlsParser* parser, const TlsVersioned& header) {
  return true;
}

static bool FindCertReqExtensions(TlsParser* parser,
                                  const TlsVersioned& header) {
  if (!parser->SkipVariable(1)) {  // request context
    return false;
  }
  return true;
}

// Only look at the EE cert for this one.
static bool FindCertificateExtensions(TlsParser* parser,
                                      const TlsVersioned& header) {
  if (!parser->SkipVariable(1)) {  // request context
    return false;
  }
  if (!parser->Skip(3)) {  // length of certificate list
    return false;
  }
  if (!parser->SkipVariable(3)) {  // ASN1Cert
    return false;
  }
  return true;
}

static bool FindNewSessionTicketExtensions(TlsParser* parser,
                                           const TlsVersioned& header) {
  if (!parser->Skip(8)) {  // lifetime, age add
    return false;
  }
  if (!parser->SkipVariable(1)) {  // ticket_nonce
    return false;
  }
  if (!parser->SkipVariable(2)) {  // ticket
    return false;
  }
  return true;
}

static const std::map<uint16_t, TlsExtensionFinder> kExtensionFinders = {
    {kTlsHandshakeClientHello, FindClientHelloExtensions},
    {kTlsHandshakeServerHello, FindServerHelloExtensions},
    {kTlsHandshakeEncryptedExtensions, FindEncryptedExtensions},
    {kTlsHandshakeCertificateRequest, FindCertReqExtensions},
    {kTlsHandshakeCertificate, FindCertificateExtensions},
    {kTlsHandshakeNewSessionTicket, FindNewSessionTicketExtensions}};

bool TlsExtensionFilter::FindExtensions(TlsParser* parser,
                                        const HandshakeHeader& header) {
  auto it = kExtensionFinders.find(header.handshake_type());
  if (it == kExtensionFinders.end()) {
    return false;
  }
  return (it->second)(parser, header);
}

PacketFilter::Action TlsExtensionFilter::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  TlsParser parser(input);
  if (!FindExtensions(&parser, header)) {
    return KEEP;
  }
  return FilterExtensions(&parser, input, output);
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

PacketFilter::Action TlsExtensionInjector::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  TlsParser parser(input);
  if (!TlsExtensionFilter::FindExtensions(&parser, header)) {
    return KEEP;
  }
  size_t offset = parser.consumed();

  *output = input;

  // Increase the size of the extensions.
  uint16_t ext_len;
  memcpy(&ext_len, output->data() + offset, sizeof(ext_len));
  ext_len = htons(ntohs(ext_len) + data_.len() + 4);
  memcpy(output->data() + offset, &ext_len, sizeof(ext_len));

  // Insert the extension type and length.
  DataBuffer type_length;
  type_length.Allocate(4);
  type_length.Write(0, extension_, 2);
  type_length.Write(2, data_.len(), 2);
  output->Splice(type_length, offset + 2);

  // Insert the payload.
  if (data_.len() > 0) {
    output->Splice(data_, offset + 6);
  }

  return CHANGE;
}

PacketFilter::Action AfterRecordN::FilterRecord(const TlsRecordHeader& header,
                                                const DataBuffer& body,
                                                DataBuffer* out) {
  if (counter_++ == record_) {
    DataBuffer buf;
    header.Write(&buf, 0, body);
    agent()->SendDirect(buf);
    dest_.lock()->Handshake();
    func_();
    return DROP;
  }

  return KEEP;
}

PacketFilter::Action TlsClientHelloVersionChanger::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  EXPECT_EQ(SECSuccess,
            SSLInt_IncrementClientHandshakeVersion(server_.lock()->ssl_fd()));
  return KEEP;
}

PacketFilter::Action SelectiveDropFilter::Filter(const DataBuffer& input,
                                                 DataBuffer* output) {
  if (counter_ >= 32) {
    return KEEP;
  }
  return ((1 << counter_++) & pattern_) ? DROP : KEEP;
}

PacketFilter::Action SelectiveRecordDropFilter::FilterRecord(
    const TlsRecordHeader& header, const DataBuffer& data,
    DataBuffer* changed) {
  if (counter_ >= 32) {
    return KEEP;
  }
  return ((1 << counter_++) & pattern_) ? DROP : KEEP;
}

/* static */ uint32_t SelectiveRecordDropFilter::ToPattern(
    std::initializer_list<size_t> records) {
  uint32_t pattern = 0;
  for (auto it = records.begin(); it != records.end(); ++it) {
    EXPECT_GT(32U, *it);
    assert(*it < 32U);
    pattern |= 1 << *it;
  }
  return pattern;
}

PacketFilter::Action TlsClientHelloVersionSetter::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  *output = input;
  output->Write(0, version_, 2);
  return CHANGE;
}

PacketFilter::Action SelectedCipherSuiteReplacer::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  *output = input;
  uint32_t temp = 0;
  EXPECT_TRUE(input.Read(0, 2, &temp));
  // Cipher suite is after version(2) and random(32).
  size_t pos = 34;
  if (temp < SSL_LIBRARY_VERSION_TLS_1_3) {
    // In old versions, we have to skip a session_id too.
    EXPECT_TRUE(input.Read(pos, 1, &temp));
    pos += 1 + temp;
  }
  output->Write(pos, static_cast<uint32_t>(cipher_suite_), 2);
  return CHANGE;
}

}  // namespace nss_test
