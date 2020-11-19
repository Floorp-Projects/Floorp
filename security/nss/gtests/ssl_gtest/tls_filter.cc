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
#include "tls_parser.h"
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

TlsRecordFilter::TlsRecordFilter(const std::shared_ptr<TlsAgent>& a)
    : agent_(a) {
  cipher_specs_.emplace_back(a->variant() == ssl_variant_datagram, 0);
}

void TlsRecordFilter::EnableDecryption() {
  EXPECT_EQ(SECSuccess,
            SSL_SecretCallback(agent()->ssl_fd(), SecretCallback, this));
  decrypting_ = true;
}

void TlsRecordFilter::SecretCallback(PRFileDesc* fd, PRUint16 epoch,
                                     SSLSecretDirection dir, PK11SymKey* secret,
                                     void* arg) {
  TlsRecordFilter* self = static_cast<TlsRecordFilter*>(arg);
  if (g_ssl_gtest_verbose) {
    std::cerr << self->agent()->role_str() << ": " << dir
              << " secret changed for epoch " << epoch << std::endl;
  }

  if (dir == ssl_secret_read) {
    return;
  }

  for (auto& spec : self->cipher_specs_) {
    ASSERT_NE(spec.epoch(), epoch) << "duplicate spec for epoch " << epoch;
  }

  SSLPreliminaryChannelInfo preinfo;
  EXPECT_EQ(SECSuccess,
            SSL_GetPreliminaryChannelInfo(self->agent()->ssl_fd(), &preinfo,
                                          sizeof(preinfo)));
  EXPECT_EQ(sizeof(preinfo), preinfo.length);

  // Check the version.
  if (preinfo.valuesSet & ssl_preinfo_version) {
    EXPECT_EQ(SSL_LIBRARY_VERSION_TLS_1_3, preinfo.protocolVersion);
  } else {
    EXPECT_EQ(1U, epoch);
  }

  uint16_t suite;
  if (epoch == 1) {
    // 0-RTT
    EXPECT_TRUE(preinfo.valuesSet & ssl_preinfo_0rtt_cipher_suite);
    suite = preinfo.zeroRttCipherSuite;
  } else {
    EXPECT_TRUE(preinfo.valuesSet & ssl_preinfo_cipher_suite);
    suite = preinfo.cipherSuite;
  }

  SSLCipherSuiteInfo cipherinfo;
  EXPECT_EQ(SECSuccess,
            SSL_GetCipherSuiteInfo(suite, &cipherinfo, sizeof(cipherinfo)));
  EXPECT_EQ(sizeof(cipherinfo), cipherinfo.length);

  bool is_dtls = self->agent()->variant() == ssl_variant_datagram;
  self->cipher_specs_.emplace_back(is_dtls, epoch);
  EXPECT_TRUE(self->cipher_specs_.back().SetKeys(&cipherinfo, secret));
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

bool TlsRecordFilter::is_dtls13_ciphertext(uint8_t ct) const {
  return is_dtls13() && (ct & kCtDtlsCiphertextMask) == kCtDtlsCiphertext;
}

// Gets the cipher spec that matches the specified epoch.
TlsCipherSpec& TlsRecordFilter::spec(uint16_t write_epoch) {
  for (auto& sp : cipher_specs_) {
    if (sp.epoch() == write_epoch) {
      return sp;
    }
  }

  // If we aren't decrypting, provide a cipher spec that does nothing other than
  // count sequence numbers.
  EXPECT_FALSE(decrypting_) << "No spec available for epoch " << write_epoch;
  ;
  bool is_dtls = agent()->variant() == ssl_variant_datagram;
  cipher_specs_.emplace_back(is_dtls, write_epoch);
  return cipher_specs_.back();
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

  // This uses the current write spec for the purposes of parsing the epoch and
  // sequence number from the header.  This might be wrong because we can
  // receive records from older specs, but guessing is good enough:
  // - In DTLS, parsing the sequence number corrects any errors.
  // - In TLS, we don't use the sequence number unless decrypting, where we use
  //   trial decryption to get the right epoch.
  uint16_t write_epoch = 0;
  SECStatus rv = SSL_GetCurrentEpoch(agent()->ssl_fd(), nullptr, &write_epoch);
  if (rv != SECSuccess) {
    ADD_FAILURE() << "unable to read epoch";
    return KEEP;
  }
  uint64_t guess_seqno = static_cast<uint64_t>(write_epoch) << 48;

  while (parser.remaining()) {
    TlsRecordHeader header;
    DataBuffer record;
    if (!header.Parse(is_dtls13(), guess_seqno, &parser, &record)) {
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
  uint16_t protection_epoch = 0;
  TlsRecordHeader out_header(header);

  if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                 &plaintext, &out_header)) {
    std::cerr << agent()->role_str() << ": unprotect failed: " << header << ":"
              << record << std::endl;
    return KEEP;
  }

  auto& protection_spec = spec(protection_epoch);
  TlsRecordHeader real_header(out_header.variant(), out_header.version(),
                              inner_content_type, out_header.sequence_number());

  PacketFilter::Action action = FilterRecord(real_header, plaintext, &filtered);
  // In stream mode, even if something doesn't change we need to re-encrypt if
  // previous packets were dropped.
  if (action == KEEP) {
    if (out_header.is_dtls() || !protection_spec.record_dropped()) {
      // Count every outgoing packet.
      protection_spec.RecordProtected();
      return KEEP;
    }
    filtered = plaintext;
  }

  if (action == DROP) {
    std::cerr << "record drop: " << out_header << ":" << record << std::endl;
    protection_spec.RecordDropped();
    return DROP;
  }

  EXPECT_GT(0x10000U, filtered.len());
  if (action != KEEP) {
    std::cerr << "record old: " << plaintext << std::endl;
    std::cerr << "record new: " << filtered << std::endl;
  }

  uint64_t seq_num = protection_spec.next_out_seqno();
  if (!decrypting_ && out_header.is_dtls()) {
    // Copy over the epoch, which isn't tracked when not decrypting.
    seq_num |= out_header.sequence_number() & (0xffffULL << 48);
  }
  out_header.sequence_number(seq_num);

  DataBuffer ciphertext;
  bool rv = Protect(protection_spec, out_header, inner_content_type, filtered,
                    &ciphertext, &out_header);
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

bool TlsRecordHeader::MaskSequenceNumber() {
  return MaskSequenceNumber(sn_mask());
}

bool TlsRecordHeader::MaskSequenceNumber(const DataBuffer& mask_buf) {
  if (mask_buf.empty()) {
    return false;
  }

  DataBuffer mask;
  if (is_dtls13_ciphertext()) {
    uint64_t seqno = sequence_number();
    uint8_t len = content_type() & kCtDtlsCiphertext16bSeqno ? 2 : 1;
    uint16_t seqno_bitmask = (1 << len * 8) - 1;
    DataBuffer val;
    if (val.Write(0, seqno & seqno_bitmask, len) != len) {
      return false;
    }

#ifdef UNSAFE_FUZZER_MODE
    // Use a null mask.
    mask.Allocate(mask_buf.len());
#endif
    mask.Append(mask_buf);
    val.data()[0] ^= mask.data()[0];
    if (len == 2 && mask.len() > 1) {
      val.data()[1] ^= mask.data()[1];
    }
    uint32_t tmp;
    if (!val.Read(0, len, &tmp)) {
      return false;
    }

    seqno = (seqno & ~seqno_bitmask) | tmp;
    seqno_is_masked_ = !seqno_is_masked_;
    if (!seqno_is_masked_) {
      seqno = ParseSequenceNumber(guess_seqno_, seqno, len * 8, 2);
    }
    sequence_number_ = seqno;

    // Now update the header bytes
    if (header_.len() > 1) {
      header_.data()[1] ^= mask.data()[0];
      if ((content_type() & kCtDtlsCiphertext16bSeqno) && header().len() > 2) {
        header_.data()[2] ^= mask.data()[1];
      }
    }
  }

  sn_mask_ = mask;
  return true;
}

uint64_t TlsRecordHeader::RecoverSequenceNumber(uint64_t guess_seqno,
                                                uint32_t partial,
                                                size_t partial_bits) {
  EXPECT_GE(32U, partial_bits);
  uint64_t mask = (1ULL << partial_bits) - 1;
  // First we determine the highest possible value.  This is half the
  // expressible range above the expected value (|guess_seqno|), less 1.
  //
  // We subtract the extra 1 from the cap so that when given a choice between
  // the equidistant expected+N and expected-N we want to chose the lower.  With
  // 0-RTT, we sometimes have to recover an epoch of 1 when we expect an epoch
  // of 3 and with 2 partial bits, the alternative result of 5 is wrong.
  uint64_t cap = guess_seqno + (1ULL << (partial_bits - 1)) - 1;
  // Add the partial piece in.  e.g., xxxx789a and 1234 becomes xxxx1234.
  uint64_t seq_no = (cap & ~mask) | partial;
  // If the partial value is higher than the same partial piece from the cap,
  // then the real value has to be lower.  e.g., xxxx1234 can't become xxxx5678.
  if (partial > (cap & mask) && (seq_no >= (1ULL << partial_bits))) {
    seq_no -= 1ULL << partial_bits;
  }
  return seq_no;
}

// Determine the full epoch and sequence number from an expected and raw value.
// The expected, raw, and output values are packed as they are in DTLS 1.2 and
// earlier: with 16 bits of epoch and 48 bits of sequence number. The raw value
// is packed this way (even before recovery) so that we don't need to track a
// moving value between two calls (one to recover the epoch, and one after
// unmasking to recover the sequence number).
uint64_t TlsRecordHeader::ParseSequenceNumber(uint64_t expected, uint64_t raw,
                                              size_t seq_no_bits,
                                              size_t epoch_bits) {
  uint64_t epoch_mask = (1ULL << epoch_bits) - 1;
  uint64_t ep = RecoverSequenceNumber(expected >> 48, (raw >> 48) & epoch_mask,
                                      epoch_bits);
  if (ep > (expected >> 48)) {
    // If the epoch has changed, reset the expected sequence number.
    expected = 0;
  } else {
    // Otherwise, retain just the sequence number part.
    expected &= (1ULL << 48) - 1;
  }
  uint64_t seq_no_mask = (1ULL << seq_no_bits) - 1;
  uint64_t seq_no = (raw & seq_no_mask);
  if (!seqno_is_masked_) {
    seq_no = RecoverSequenceNumber(expected, seq_no, seq_no_bits);
  }

  return (ep << 48) | seq_no;
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
    // Deal with the DTLSCipherText header.
    if (is_dtls13_ciphertext()) {
      uint8_t seq_no_bytes =
          (content_type_ & kCtDtlsCiphertext16bSeqno) ? 2 : 1;
      uint32_t tmp;

      if (!parser->Read(&tmp, seq_no_bytes)) {
        return false;
      }

      // Store the guess if masked. If and when seqno_bytesenceNumber is called,
      // the value will be unmasked and recovered. This assumes we only call
      // Parse() on headers containing masked values.
      seqno_is_masked_ = true;
      guess_seqno_ = seqno;
      uint64_t ep = content_type_ & 0x03;
      sequence_number_ = (ep << 48) | tmp;

      // Recover the full epoch. Note the sequence number portion holds the
      // masked value until a call to Mask() reveals it (as indicated by
      // |seqno_is_masked_|).
      sequence_number_ =
          ParseSequenceNumber(seqno, sequence_number_, seq_no_bytes * 8, 2);

      uint32_t len_bytes =
          (content_type_ & kCtDtlsCiphertextLengthPresent) ? 2 : 0;
      if (len_bytes) {
        if (!parser->Read(&tmp, 2)) {
          return false;
        }
      }

      if (!parser->ReadFromMark(&header_, parser->consumed() - mark, mark)) {
        return false;
      }

      return len_bytes ? parser->Read(body, tmp)
                       : parser->Read(body, parser->remaining());
    }

    // The full DTLSPlainText header can only be used for a few types.
    EXPECT_TRUE(content_type_ == ssl_ct_alert ||
                content_type_ == ssl_ct_handshake ||
                content_type_ == ssl_ct_ack);
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
  if (is_dtls13_ciphertext()) {
    uint8_t seq_no_bytes = (content_type_ & kCtDtlsCiphertext16bSeqno) ? 2 : 1;
    // application_data records in TLS 1.3 have a different header format.
    uint32_t e = (sequence_number_ >> 48) & 0x3;
    uint32_t seqno = sequence_number_ & ((1ULL << seq_no_bytes * 8) - 1);
    uint8_t new_content_type_ = content_type_ | e;
    offset = buffer->Write(offset, new_content_type_, 1);
    offset = buffer->Write(offset, seqno, seq_no_bytes);

    if (content_type_ & kCtDtlsCiphertextLengthPresent) {
      offset = buffer->Write(offset, body_len, 2);
    }
  } else {
    offset = buffer->Write(offset, content_type_, 1);
    uint16_t v = is_dtls() ? TlsVersionToDtlsVersion(version_) : version_;
    offset = buffer->Write(offset, v, 2);
    if (is_dtls()) {
      // write epoch (2 octet), and seqnum (6 octet)
      offset = buffer->Write(offset, sequence_number_ >> 32, 4);
      offset = buffer->Write(offset, sequence_number_ & 0xffffffff, 4);
    }
    offset = buffer->Write(offset, body_len, 2);
  }

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
                                uint16_t* protection_epoch,
                                uint8_t* inner_content_type,
                                DataBuffer* plaintext,
                                TlsRecordHeader* out_header) {
  if (!decrypting_ || !header.is_protected()) {
    // Maintain the epoch and sequence number for plaintext records.
    uint16_t ep = 0;
    if (agent()->variant() == ssl_variant_datagram) {
      ep = static_cast<uint16_t>(header.sequence_number() >> 48);
    }
    spec(ep).RecordUnprotected(header.sequence_number());
    *protection_epoch = ep;
    *inner_content_type = header.content_type();
    *plaintext = ciphertext;
    return true;
  }

  uint16_t ep = 0;
  if (agent()->variant() == ssl_variant_datagram) {
    ep = static_cast<uint16_t>(header.sequence_number() >> 48);
    if (!spec(ep).Unprotect(header, ciphertext, plaintext, out_header)) {
      return false;
    }
  } else {
    // In TLS, records aren't clearly labelled with their epoch, and we
    // can't just use the newest keys because the same flight of messages can
    // contain multiple epochs. So... trial decrypt!
    for (size_t i = cipher_specs_.size() - 1; i > 0; --i) {
      if (cipher_specs_[i].Unprotect(header, ciphertext, plaintext,
                                     out_header)) {
        ep = cipher_specs_[i].epoch();
        break;
      }
    }
    if (!ep) {
      return false;
    }
  }

  size_t len = plaintext->len();
  while (len > 0 && !plaintext->data()[len - 1]) {
    --len;
  }
  if (!len) {
    // Bogus padding.
    return false;
  }

  *protection_epoch = ep;
  *inner_content_type = plaintext->data()[len - 1];
  plaintext->Truncate(len - 1);
  if (g_ssl_gtest_verbose) {
    std::cerr << agent()->role_str() << ": unprotect: epoch=" << ep
              << " seq=" << std::hex << header.sequence_number() << std::dec
              << " " << *plaintext << std::endl;
  }

  return true;
}

bool TlsRecordFilter::Protect(TlsCipherSpec& protection_spec,
                              const TlsRecordHeader& header,
                              uint8_t inner_content_type,
                              const DataBuffer& plaintext,
                              DataBuffer* ciphertext,
                              TlsRecordHeader* out_header, size_t padding) {
  if (!protection_spec.is_protected()) {
    // Not protected, just keep the sequence numbers updated.
    protection_spec.RecordProtected();
    *ciphertext = plaintext;
    return true;
  }

  DataBuffer padded;
  padded.Allocate(plaintext.len() + 1 + padding);
  size_t offset = padded.Write(0, plaintext.data(), plaintext.len());
  padded.Write(offset, inner_content_type, 1);

  bool ok = protection_spec.Protect(header, padded, ciphertext, out_header);
  if (!ok) {
    ADD_FAILURE() << "protect fail";
  } else if (g_ssl_gtest_verbose) {
    std::cerr << agent()->role_str()
              << ": protect: epoch=" << protection_spec.epoch()
              << " seq=" << std::hex << header.sequence_number() << std::dec
              << " " << *ciphertext << std::endl;
  }
  return ok;
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
  if (record_header.content_type() != ssl_ct_handshake) {
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

PacketFilter::Action TlsExtensionResizer::FilterExtension(
    uint16_t extension_type, const DataBuffer& input, DataBuffer* output) {
  if (extension_type != extension_) {
    return KEEP;
  }

  if (input.len() <= length_) {
    DataBuffer buf(length_ - input.len());
    output->Append(buf);
    return CHANGE;
  }

  output->Assign(input.data(), length_);
  return CHANGE;
}

PacketFilter::Action TlsExtensionAppender::FilterHandshake(
    const HandshakeHeader& header, const DataBuffer& input,
    DataBuffer* output) {
  TlsParser parser(input);
  if (!TlsExtensionFilter::FindExtensions(&parser, header)) {
    return KEEP;
  }
  *output = input;

  // Increase the length of the extensions block.
  if (!UpdateLength(output, parser.consumed(), 2)) {
    return KEEP;
  }

  // Extensions in Certificate are nested twice.  Increase the size of the
  // certificate list.
  if (header.handshake_type() == kTlsHandshakeCertificate) {
    TlsParser p2(input);
    if (!p2.SkipVariable(1)) {
      ADD_FAILURE();
      return KEEP;
    }
    if (!UpdateLength(output, p2.consumed(), 3)) {
      return KEEP;
    }
  }

  size_t offset = output->len();
  offset = output->Write(offset, extension_, 2);
  WriteVariable(output, offset, data_, 2);

  return CHANGE;
}

bool TlsExtensionAppender::UpdateLength(DataBuffer* output, size_t offset,
                                        size_t size) {
  uint32_t len;
  if (!output->Read(offset, size, &len)) {
    ADD_FAILURE();
    return false;
  }

  len += 4 + data_.len();
  output->Write(offset, len, size);
  return true;
}

PacketFilter::Action TlsExtensionDropper::FilterExtension(
    uint16_t extension_type, const DataBuffer& input, DataBuffer* output) {
  if (extension_type == extension_) {
    return DROP;
  }
  return KEEP;
}

PacketFilter::Action TlsExtensionDamager::FilterExtension(
    uint16_t extension_type, const DataBuffer& input, DataBuffer* output) {
  if (extension_type != extension_) {
    return KEEP;
  }

  *output = input;
  output->data()[index_] += 73;  // Increment selected for maximum damage
  return CHANGE;
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
  EXPECT_EQ(header.version(), NormalizeTlsVersion(temp));
  // Cipher suite is after version(2), random(32)
  // and [legacy_]session_id(<0..32>).
  size_t pos = 34;
  EXPECT_TRUE(input.Read(pos, 1, &temp));
  pos += 1 + temp;

  output->Write(pos, static_cast<uint32_t>(cipher_suite_), 2);
  return CHANGE;
}
}  // namespace nss_test
