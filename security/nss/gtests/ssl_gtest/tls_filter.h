/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_filter_h_
#define tls_filter_h_

#include <functional>
#include <memory>
#include <set>
#include <vector>
#include "sslt.h"
#include "test_io.h"
#include "tls_agent.h"
#include "tls_parser.h"
#include "tls_protect.h"

extern "C" {
#include "libssl_internals.h"
}

namespace nss_test {

class TlsCipherSpec;

class TlsVersioned {
 public:
  TlsVersioned() : variant_(ssl_variant_stream), version_(0) {}
  TlsVersioned(SSLProtocolVariant var, uint16_t ver)
      : variant_(var), version_(ver) {}

  bool is_dtls() const { return variant_ == ssl_variant_datagram; }
  SSLProtocolVariant variant() const { return variant_; }
  uint16_t version() const { return version_; }

  void WriteStream(std::ostream& stream) const;

 protected:
  SSLProtocolVariant variant_;
  uint16_t version_;
};

class TlsRecordHeader : public TlsVersioned {
 public:
  TlsRecordHeader()
      : TlsVersioned(), content_type_(0), sequence_number_(0), header_() {}
  TlsRecordHeader(SSLProtocolVariant var, uint16_t ver, uint8_t ct,
                  uint64_t seqno)
      : TlsVersioned(var, ver),
        content_type_(ct),
        sequence_number_(seqno),
        header_() {}

  uint8_t content_type() const { return content_type_; }
  uint64_t sequence_number() const { return sequence_number_; }
  uint16_t epoch() const {
    return static_cast<uint16_t>(sequence_number_ >> 48);
  }
  size_t header_length() const;
  const DataBuffer& header() const { return header_; }

  // Parse the header; return true if successful; body in an outparam if OK.
  bool Parse(bool is_dtls13, uint64_t sequence_number, TlsParser* parser,
             DataBuffer* body);
  // Write the header and body to a buffer at the given offset.
  // Return the offset of the end of the write.
  size_t Write(DataBuffer* buffer, size_t offset, const DataBuffer& body) const;
  size_t WriteHeader(DataBuffer* buffer, size_t offset, size_t body_len) const;

 private:
  static uint64_t RecoverSequenceNumber(uint64_t expected, uint32_t partial,
                                        size_t partial_bits);
  static uint64_t ParseSequenceNumber(uint64_t expected, uint32_t raw,
                                      size_t seq_no_bits, size_t epoch_bits);

  uint8_t content_type_;
  uint64_t sequence_number_;
  DataBuffer header_;
};

struct TlsRecord {
  const TlsRecordHeader header;
  const DataBuffer buffer;
};

// Make a filter and install it on a TlsAgent.
template <class T, typename... Args>
inline std::shared_ptr<T> MakeTlsFilter(const std::shared_ptr<TlsAgent>& agent,
                                        Args&&... args) {
  auto filter = std::make_shared<T>(agent, std::forward<Args>(args)...);
  agent->SetFilter(filter);
  return filter;
}

// Abstract filter that operates on entire (D)TLS records.
class TlsRecordFilter : public PacketFilter {
 public:
  TlsRecordFilter(const std::shared_ptr<TlsAgent>& a);

  std::shared_ptr<TlsAgent> agent() const { return agent_.lock(); }

  // External interface. Overrides PacketFilter.
  PacketFilter::Action Filter(const DataBuffer& input, DataBuffer* output);

  // Report how many packets were altered by the filter.
  size_t filtered_packets() const { return count_; }

  // Enable decryption. This only works properly for TLS 1.3 and above.
  // Enabling it for lower version tests will cause undefined
  // behavior.
  void EnableDecryption();
  bool Unprotect(const TlsRecordHeader& header, const DataBuffer& cipherText,
                 uint16_t* protection_epoch, uint8_t* inner_content_type,
                 DataBuffer* plaintext);
  bool Protect(TlsCipherSpec& protection_spec, const TlsRecordHeader& header,
               uint8_t inner_content_type, const DataBuffer& plaintext,
               DataBuffer* ciphertext, size_t padding = 0);

 protected:
  // There are two filter functions which can be overriden. Both are
  // called with the header and the record but the outer one is called
  // with a raw pointer to let you write into the buffer and lets you
  // do anything with this section of the stream. The inner one
  // just lets you change the record contents. By default, the
  // outer one calls the inner one, so if you override the outer
  // one, the inner one is never called unless you call it yourself.
  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& record,
                                            size_t* offset, DataBuffer* output);

  // The record filter receives the record contentType, version and DTLS
  // sequence number (which is zero for TLS), plus the existing record payload.
  // It returns an action (KEEP, CHANGE, DROP).  It writes to the `changed`
  // outparam with the new record contents if it chooses to CHANGE the record.
  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& data,
                                            DataBuffer* changed) {
    return KEEP;
  }

  bool is_dtls13() const;
  TlsCipherSpec& spec(uint16_t epoch);

 private:
  static void SecretCallback(PRFileDesc* fd, PRUint16 epoch,
                             SSLSecretDirection dir, PK11SymKey* secret,
                             void* arg);

  std::weak_ptr<TlsAgent> agent_;
  size_t count_ = 0;
  std::vector<TlsCipherSpec> cipher_specs_;
  bool decrypting_ = false;
};

inline std::ostream& operator<<(std::ostream& stream, const TlsVersioned& v) {
  v.WriteStream(stream);
  return stream;
}

inline std::ostream& operator<<(std::ostream& stream,
                                const TlsRecordHeader& hdr) {
  hdr.WriteStream(stream);
  stream << ' ';
  switch (hdr.content_type()) {
    case ssl_ct_change_cipher_spec:
      stream << "CCS";
      break;
    case ssl_ct_alert:
      stream << "Alert";
      break;
    case ssl_ct_handshake:
      stream << "Handshake";
      break;
    case ssl_ct_application_data:
      stream << "Data";
      break;
    case ssl_ct_ack:
      stream << "ACK";
      break;
    default:
      stream << '<' << static_cast<int>(hdr.content_type()) << '>';
      break;
  }
  return stream << ' ' << std::hex << hdr.sequence_number() << std::dec;
}

// Abstract filter that operates on handshake messages rather than records.
// This assumes that the handshake messages are written in a block as entire
// records and that they don't span records or anything crazy like that.
class TlsHandshakeFilter : public TlsRecordFilter {
 public:
  TlsHandshakeFilter(const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a), handshake_types_(), preceding_fragment_() {}
  TlsHandshakeFilter(const std::shared_ptr<TlsAgent>& a,
                     const std::set<uint8_t>& types)
      : TlsRecordFilter(a), handshake_types_(types), preceding_fragment_() {}

  // This filter can be set to be selective based on handshake message type.  If
  // this function isn't used (or the set is empty), then all handshake messages
  // will be filtered.
  void SetHandshakeTypes(const std::set<uint8_t>& types) {
    handshake_types_ = types;
  }

  class HandshakeHeader : public TlsVersioned {
   public:
    HandshakeHeader() : TlsVersioned(), handshake_type_(0), message_seq_(0) {}

    uint8_t handshake_type() const { return handshake_type_; }
    bool Parse(TlsParser* parser, const TlsRecordHeader& record_header,
               const DataBuffer& preceding_fragment, DataBuffer* body,
               bool* complete);
    size_t Write(DataBuffer* buffer, size_t offset,
                 const DataBuffer& body) const;
    size_t WriteFragment(DataBuffer* buffer, size_t offset,
                         const DataBuffer& body, size_t fragment_offset,
                         size_t fragment_length) const;

   private:
    // Reads the length from the record header.
    // This also reads the DTLS fragment information and checks it.
    bool ReadLength(TlsParser* parser, const TlsRecordHeader& header,
                    uint32_t expected_offset, uint32_t* length,
                    bool* last_fragment);

    uint8_t handshake_type_;
    uint16_t message_seq_;
    // fragment_offset is always zero in these tests.
  };

 protected:
  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& input,
                                            DataBuffer* output);
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) = 0;

 private:
  bool IsFilteredType(const HandshakeHeader& header,
                      const DataBuffer& handshake);

  std::set<uint8_t> handshake_types_;
  DataBuffer preceding_fragment_;
};

// Make a copy of the first instance of a handshake message.
class TlsHandshakeRecorder : public TlsHandshakeFilter {
 public:
  TlsHandshakeRecorder(const std::shared_ptr<TlsAgent>& a,
                       uint8_t handshake_type)
      : TlsHandshakeFilter(a, {handshake_type}), buffer_() {}
  TlsHandshakeRecorder(const std::shared_ptr<TlsAgent>& a,
                       const std::set<uint8_t>& handshake_types)
      : TlsHandshakeFilter(a, handshake_types), buffer_() {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output);

  void Reset() { buffer_.Truncate(0); }

  const DataBuffer& buffer() const { return buffer_; }

 private:
  DataBuffer buffer_;
};

// Replace all instances of a handshake message.
class TlsInspectorReplaceHandshakeMessage : public TlsHandshakeFilter {
 public:
  TlsInspectorReplaceHandshakeMessage(const std::shared_ptr<TlsAgent>& a,
                                      uint8_t handshake_type,
                                      const DataBuffer& replacement)
      : TlsHandshakeFilter(a, {handshake_type}), buffer_(replacement) {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output);

 private:
  DataBuffer buffer_;
};

// Make a copy of each record of a given type.
class TlsRecordRecorder : public TlsRecordFilter {
 public:
  TlsRecordRecorder(const std::shared_ptr<TlsAgent>& a, uint8_t ct)
      : TlsRecordFilter(a), filter_(true), ct_(ct), records_() {}
  TlsRecordRecorder(const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a),
        filter_(false),
        ct_(ssl_ct_handshake),  // dummy (<optional> is C++14)
        records_() {}
  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& input,
                                            DataBuffer* output);

  size_t count() const { return records_.size(); }
  void Clear() { records_.clear(); }

  const TlsRecord& record(size_t i) const { return records_[i]; }

 private:
  bool filter_;
  uint8_t ct_;
  std::vector<TlsRecord> records_;
};

// Make a copy of the complete conversation.
class TlsConversationRecorder : public TlsRecordFilter {
 public:
  TlsConversationRecorder(const std::shared_ptr<TlsAgent>& a,
                          DataBuffer& buffer)
      : TlsRecordFilter(a), buffer_(buffer) {}

  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& input,
                                            DataBuffer* output);

 private:
  DataBuffer buffer_;
};

// Make a copy of the records
class TlsHeaderRecorder : public TlsRecordFilter {
 public:
  TlsHeaderRecorder(const std::shared_ptr<TlsAgent>& a) : TlsRecordFilter(a) {}
  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& input,
                                            DataBuffer* output);
  const TlsRecordHeader* header(size_t index);

 private:
  std::vector<TlsRecordHeader> headers_;
};

typedef std::initializer_list<std::shared_ptr<PacketFilter>>
    ChainedPacketFilterInit;

// Runs multiple packet filters in series.
class ChainedPacketFilter : public PacketFilter {
 public:
  ChainedPacketFilter() {}
  ChainedPacketFilter(const std::vector<std::shared_ptr<PacketFilter>> filters)
      : filters_(filters.begin(), filters.end()) {}
  ChainedPacketFilter(ChainedPacketFilterInit il) : filters_(il) {}
  virtual ~ChainedPacketFilter() {}

  virtual PacketFilter::Action Filter(const DataBuffer& input,
                                      DataBuffer* output);

  // Takes ownership of the filter.
  void Add(std::shared_ptr<PacketFilter> filter) { filters_.push_back(filter); }

 private:
  std::vector<std::shared_ptr<PacketFilter>> filters_;
};

typedef std::function<bool(TlsParser* parser, const TlsVersioned& header)>
    TlsExtensionFinder;

class TlsExtensionFilter : public TlsHandshakeFilter {
 public:
  TlsExtensionFilter(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a,
                           {kTlsHandshakeClientHello, kTlsHandshakeServerHello,
                            kTlsHandshakeHelloRetryRequest,
                            kTlsHandshakeEncryptedExtensions}) {}

  TlsExtensionFilter(const std::shared_ptr<TlsAgent>& a,
                     const std::set<uint8_t>& types)
      : TlsHandshakeFilter(a, types) {}

  static bool FindExtensions(TlsParser* parser, const HandshakeHeader& header);

 protected:
  PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                       const DataBuffer& input,
                                       DataBuffer* output) override;

  virtual PacketFilter::Action FilterExtension(uint16_t extension_type,
                                               const DataBuffer& input,
                                               DataBuffer* output) = 0;

 private:
  PacketFilter::Action FilterExtensions(TlsParser* parser,
                                        const DataBuffer& input,
                                        DataBuffer* output);
};

class TlsExtensionCapture : public TlsExtensionFilter {
 public:
  TlsExtensionCapture(const std::shared_ptr<TlsAgent>& a, uint16_t ext,
                      bool last = false)
      : TlsExtensionFilter(a),
        extension_(ext),
        captured_(false),
        last_(last),
        data_() {}

  const DataBuffer& extension() const { return data_; }
  bool captured() const { return captured_; }

 protected:
  PacketFilter::Action FilterExtension(uint16_t extension_type,
                                       const DataBuffer& input,
                                       DataBuffer* output) override;

 private:
  const uint16_t extension_;
  bool captured_;
  bool last_;
  DataBuffer data_;
};

class TlsExtensionReplacer : public TlsExtensionFilter {
 public:
  TlsExtensionReplacer(const std::shared_ptr<TlsAgent>& a, uint16_t extension,
                       const DataBuffer& data)
      : TlsExtensionFilter(a), extension_(extension), data_(data) {}
  PacketFilter::Action FilterExtension(uint16_t extension_type,
                                       const DataBuffer& input,
                                       DataBuffer* output) override;

 private:
  const uint16_t extension_;
  const DataBuffer data_;
};

class TlsExtensionDropper : public TlsExtensionFilter {
 public:
  TlsExtensionDropper(const std::shared_ptr<TlsAgent>& a, uint16_t extension)
      : TlsExtensionFilter(a), extension_(extension) {}
  PacketFilter::Action FilterExtension(uint16_t extension_type,
                                       const DataBuffer&, DataBuffer*) override;

 private:
  uint16_t extension_;
};

class TlsHandshakeDropper : public TlsHandshakeFilter {
 public:
  TlsHandshakeDropper(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a) {}

 protected:
  PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                       const DataBuffer& input,
                                       DataBuffer* output) override {
    return DROP;
  }
};

class TlsEncryptedHandshakeMessageReplacer : public TlsRecordFilter {
 public:
  TlsEncryptedHandshakeMessageReplacer(const std::shared_ptr<TlsAgent>& a,
                                       uint8_t old_ct, uint8_t new_ct)
      : TlsRecordFilter(a), old_ct_(old_ct), new_ct_(new_ct) {}

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    if (header.content_type() != ssl_ct_application_data) {
      return KEEP;
    }

    uint16_t protection_epoch = 0;
    uint8_t inner_content_type;
    DataBuffer plaintext;
    if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                   &plaintext) ||
        !plaintext.len()) {
      return KEEP;
    }

    if (inner_content_type != ssl_ct_handshake) {
      return KEEP;
    }

    size_t off = 0;
    uint32_t msg_len = 0;
    uint32_t msg_type = 255;  // Not a real message
    do {
      if (!plaintext.Read(off, 1, &msg_type) || msg_type == old_ct_) {
        break;
      }

      // Increment and check next messages
      if (!plaintext.Read(++off, 3, &msg_len)) {
        break;
      }
      off += 3 + msg_len;
    } while (msg_type != old_ct_);

    if (msg_type == old_ct_) {
      plaintext.Write(off, new_ct_, 1);
    }

    DataBuffer ciphertext;
    bool ok = Protect(spec(protection_epoch), header, inner_content_type,
                      plaintext, &ciphertext, 0);
    if (!ok) {
      return KEEP;
    }
    *offset = header.Write(output, *offset, ciphertext);
    return CHANGE;
  }

 private:
  uint8_t old_ct_;
  uint8_t new_ct_;
};

class TlsExtensionInjector : public TlsHandshakeFilter {
 public:
  TlsExtensionInjector(const std::shared_ptr<TlsAgent>& a, uint16_t ext,
                       const DataBuffer& data)
      : TlsHandshakeFilter(a), extension_(ext), data_(data) {}

 protected:
  PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                       const DataBuffer& input,
                                       DataBuffer* output) override;

 private:
  const uint16_t extension_;
  const DataBuffer data_;
};

class TlsExtensionDamager : public TlsExtensionFilter {
 public:
  TlsExtensionDamager(const std::shared_ptr<TlsAgent>& a, uint16_t extension,
                      size_t index)
      : TlsExtensionFilter(a), extension_(extension), index_(index) {}
  virtual PacketFilter::Action FilterExtension(uint16_t extension_type,
                                               const DataBuffer& input,
                                               DataBuffer* output);

 private:
  uint16_t extension_;
  size_t index_;
};

typedef std::function<void(void)> VoidFunction;

class AfterRecordN : public TlsRecordFilter {
 public:
  AfterRecordN(const std::shared_ptr<TlsAgent>& src,
               const std::shared_ptr<TlsAgent>& dest, unsigned int record,
               VoidFunction func)
      : TlsRecordFilter(src),
        dest_(dest),
        record_(record),
        func_(func),
        counter_(0) {}

  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& body,
                                            DataBuffer* out) override;

 private:
  std::weak_ptr<TlsAgent> dest_;
  unsigned int record_;
  VoidFunction func_;
  unsigned int counter_;
};

// When we see the ClientKeyExchange from |client|, increment the
// ClientHelloVersion on |server|.
class TlsClientHelloVersionChanger : public TlsHandshakeFilter {
 public:
  TlsClientHelloVersionChanger(const std::shared_ptr<TlsAgent>& client,
                               const std::shared_ptr<TlsAgent>& server)
      : TlsHandshakeFilter(client, {kTlsHandshakeClientKeyExchange}),
        server_(server) {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output);

 private:
  std::weak_ptr<TlsAgent> server_;
};

// Damage a record.
class TlsRecordLastByteDamager : public TlsRecordFilter {
 public:
  TlsRecordLastByteDamager(const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a) {}

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& data,
                                    DataBuffer* changed) override {
    *changed = data;
    changed->data()[changed->len() - 1]++;
    return CHANGE;
  }
};

// This class selectively drops complete writes.  This relies on the fact that
// writes in libssl are on record boundaries.
class SelectiveDropFilter : public PacketFilter {
 public:
  SelectiveDropFilter(uint32_t pattern) : pattern_(pattern), counter_(0) {}

 protected:
  virtual PacketFilter::Action Filter(const DataBuffer& input,
                                      DataBuffer* output) override;

 private:
  const uint32_t pattern_;
  uint8_t counter_;
};

// This class selectively drops complete records. The difference from
// SelectiveDropFilter is that if multiple DTLS records are in the same
// datagram, we just drop one.
class SelectiveRecordDropFilter : public TlsRecordFilter {
 public:
  SelectiveRecordDropFilter(const std::shared_ptr<TlsAgent>& a,
                            uint32_t pattern, bool on = true)
      : TlsRecordFilter(a), pattern_(pattern), counter_(0) {
    if (!on) {
      Disable();
    }
  }
  SelectiveRecordDropFilter(const std::shared_ptr<TlsAgent>& a,
                            std::initializer_list<size_t> records)
      : SelectiveRecordDropFilter(a, ToPattern(records), true) {}

  void Reset(uint32_t pattern) {
    counter_ = 0;
    PacketFilter::Enable();
    pattern_ = pattern;
  }

  void Reset(std::initializer_list<size_t> records) {
    Reset(ToPattern(records));
  }

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& data,
                                    DataBuffer* changed) override;

 private:
  static uint32_t ToPattern(std::initializer_list<size_t> records);

  uint32_t pattern_;
  uint8_t counter_;
};

// Set the version number in the ClientHello.
class TlsClientHelloVersionSetter : public TlsHandshakeFilter {
 public:
  TlsClientHelloVersionSetter(const std::shared_ptr<TlsAgent>& a,
                              uint16_t version)
      : TlsHandshakeFilter(a, {kTlsHandshakeClientHello}), version_(version) {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output);

 private:
  uint16_t version_;
};

// Damages the last byte of a handshake message.
class TlsLastByteDamager : public TlsHandshakeFilter {
 public:
  TlsLastByteDamager(const std::shared_ptr<TlsAgent>& a, uint8_t type)
      : TlsHandshakeFilter(a), type_(type) {}
  PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) override {
    if (header.handshake_type() != type_) {
      return KEEP;
    }

    *output = input;

    output->data()[output->len() - 1]++;
    return CHANGE;
  }

 private:
  uint8_t type_;
};

class SelectedCipherSuiteReplacer : public TlsHandshakeFilter {
 public:
  SelectedCipherSuiteReplacer(const std::shared_ptr<TlsAgent>& a,
                              uint16_t suite)
      : TlsHandshakeFilter(a, {kTlsHandshakeServerHello}),
        cipher_suite_(suite) {}

 protected:
  PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                       const DataBuffer& input,
                                       DataBuffer* output) override;

 private:
  uint16_t cipher_suite_;
};

}  // namespace nss_test

#endif
