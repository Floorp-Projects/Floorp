/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_filter_h_
#define tls_filter_h_

#include <functional>
#include <memory>
#include <vector>

#include "test_io.h"
#include "tls_parser.h"

namespace nss_test {

// Abstract filter that operates on entire (D)TLS records.
class TlsRecordFilter : public PacketFilter {
 public:
  TlsRecordFilter() : count_(0) {}

  // External interface. Overrides PacketFilter.
  PacketFilter::Action Filter(const DataBuffer& input, DataBuffer* output);

  // Report how many packets were altered by the filter.
  size_t filtered_packets() const { return count_; }

  class Versioned {
   public:
    Versioned() : version_(0) {}
    explicit Versioned(uint16_t version) : version_(version) {}

    bool is_dtls() const { return IsDtls(version_); }
    uint16_t version() const { return version_; }

   protected:
    uint16_t version_;
  };

  class RecordHeader : public Versioned {
   public:
    RecordHeader() : Versioned(), content_type_(0), sequence_number_(0) {}
    RecordHeader(uint16_t version, uint8_t content_type,
                 uint64_t sequence_number)
        : Versioned(version),
          content_type_(content_type),
          sequence_number_(sequence_number) {}

    uint8_t content_type() const { return content_type_; }
    uint64_t sequence_number() const { return sequence_number_; }
    size_t header_length() const { return is_dtls() ? 11 : 3; }

    // Parse the header; return true if successful; body in an outparam if OK.
    bool Parse(TlsParser* parser, DataBuffer* body);
    // Write the header and body to a buffer at the given offset.
    // Return the offset of the end of the write.
    size_t Write(DataBuffer* buffer, size_t offset,
                 const DataBuffer& body) const;

   private:
    uint8_t content_type_;
    uint64_t sequence_number_;
  };

 protected:
  // There are two filter functions which can be overriden. Both are
  // called with the header and the record but the outer one is called
  // with a raw pointer to let you write into the buffer and lets you
  // do anything with this section of the stream. The inner one
  // just lets you change the record contents. By default, the
  // outer one calls the inner one, so if you override the outer
  // one, the inner one is never called unless you call it yourself.
  virtual PacketFilter::Action FilterRecord(const RecordHeader& header,
                                            const DataBuffer& record,
                                            size_t* offset, DataBuffer* output);

  // The record filter receives the record contentType, version and DTLS
  // sequence number (which is zero for TLS), plus the existing record payload.
  // It returns an action (KEEP, CHANGE, DROP).  It writes to the `changed`
  // outparam with the new record contents if it chooses to CHANGE the record.
  virtual PacketFilter::Action FilterRecord(const RecordHeader& header,
                                            const DataBuffer& data,
                                            DataBuffer* changed) {
    return KEEP;
  }

 private:
  size_t count_;
};

// Abstract filter that operates on handshake messages rather than records.
// This assumes that the handshake messages are written in a block as entire
// records and that they don't span records or anything crazy like that.
class TlsHandshakeFilter : public TlsRecordFilter {
 public:
  TlsHandshakeFilter() {}

  class HandshakeHeader : public Versioned {
   public:
    HandshakeHeader() : Versioned(), handshake_type_(0), message_seq_(0) {}

    uint8_t handshake_type() const { return handshake_type_; }
    bool Parse(TlsParser* parser, const RecordHeader& record_header,
               DataBuffer* body);
    size_t Write(DataBuffer* buffer, size_t offset,
                 const DataBuffer& body) const;

   private:
    // Reads the length from the record header.
    // This also reads the DTLS fragment information and checks it.
    bool ReadLength(TlsParser* parser, const RecordHeader& header,
                    uint32_t* length);

    uint8_t handshake_type_;
    uint16_t message_seq_;
    // fragment_offset is always zero in these tests.
  };

 protected:
  virtual PacketFilter::Action FilterRecord(const RecordHeader& header,
                                            const DataBuffer& input,
                                            DataBuffer* output);
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) = 0;

 private:
};

// Make a copy of the first instance of a handshake message.
class TlsInspectorRecordHandshakeMessage : public TlsHandshakeFilter {
 public:
  TlsInspectorRecordHandshakeMessage(uint8_t handshake_type)
      : handshake_type_(handshake_type), buffer_() {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output);

  const DataBuffer& buffer() const { return buffer_; }

 private:
  uint8_t handshake_type_;
  DataBuffer buffer_;
};

// Replace all instances of a handshake message.
class TlsInspectorReplaceHandshakeMessage : public TlsHandshakeFilter {
 public:
  TlsInspectorReplaceHandshakeMessage(uint8_t handshake_type,
                                      const DataBuffer& replacement)
      : handshake_type_(handshake_type), buffer_(replacement) {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output);

 private:
  uint8_t handshake_type_;
  DataBuffer buffer_;
};

// Make a copy of the complete conversation.
class TlsConversationRecorder : public TlsRecordFilter {
 public:
  TlsConversationRecorder(DataBuffer& buffer) : buffer_(buffer) {}

  virtual PacketFilter::Action FilterRecord(const RecordHeader& header,
                                            const DataBuffer& input,
                                            DataBuffer* output);

 private:
  DataBuffer& buffer_;
};

// Records an alert.  If an alert has already been recorded, it won't save the
// new alert unless the old alert is a warning and the new one is fatal.
class TlsAlertRecorder : public TlsRecordFilter {
 public:
  TlsAlertRecorder() : level_(255), description_(255) {}

  virtual PacketFilter::Action FilterRecord(const RecordHeader& header,
                                            const DataBuffer& input,
                                            DataBuffer* output);

  uint8_t level() const { return level_; }
  uint8_t description() const { return description_; }

 private:
  uint8_t level_;
  uint8_t description_;
};

// Runs multiple packet filters in series.
class ChainedPacketFilter : public PacketFilter {
 public:
  ChainedPacketFilter() {}
  ChainedPacketFilter(const std::vector<PacketFilter*> filters)
      : filters_(filters.begin(), filters.end()) {}
  virtual ~ChainedPacketFilter();

  virtual PacketFilter::Action Filter(const DataBuffer& input,
                                      DataBuffer* output);

  // Takes ownership of the filter.
  void Add(PacketFilter* filter) { filters_.push_back(filter); }

 private:
  std::vector<PacketFilter*> filters_;
};

class TlsExtensionFilter : public TlsHandshakeFilter {
 protected:
  PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                       const DataBuffer& input,
                                       DataBuffer* output) override;

  virtual PacketFilter::Action FilterExtension(uint16_t extension_type,
                                               const DataBuffer& input,
                                               DataBuffer* output) = 0;

 public:
  static bool FindClientHelloExtensions(TlsParser* parser,
                                        const Versioned& header);
  static bool FindServerHelloExtensions(TlsParser* parser);

 private:
  PacketFilter::Action FilterExtensions(TlsParser* parser,
                                        const DataBuffer& input,
                                        DataBuffer* output);
};

class TlsExtensionCapture : public TlsExtensionFilter {
 public:
  TlsExtensionCapture(uint16_t ext, bool last = false)
      : extension_(ext), captured_(false), last_(last), data_() {}

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
  TlsExtensionReplacer(uint16_t extension, const DataBuffer& data)
      : extension_(extension), data_(data) {}
  PacketFilter::Action FilterExtension(uint16_t extension_type,
                                       const DataBuffer& input,
                                       DataBuffer* output) override;

 private:
  const uint16_t extension_;
  const DataBuffer data_;
};

class TlsExtensionDropper : public TlsExtensionFilter {
 public:
  TlsExtensionDropper(uint16_t extension) : extension_(extension) {}
  PacketFilter::Action FilterExtension(uint16_t extension_type,
                                       const DataBuffer&, DataBuffer*) override;

 private:
  uint16_t extension_;
};

class TlsAgent;
typedef std::function<void(void)> VoidFunction;

class AfterRecordN : public TlsRecordFilter {
 public:
  AfterRecordN(TlsAgent* src, TlsAgent* dest, unsigned int record,
               VoidFunction func)
      : src_(src), dest_(dest), record_(record), func_(func), counter_(0) {}

  virtual PacketFilter::Action FilterRecord(const RecordHeader& header,
                                            const DataBuffer& body,
                                            DataBuffer* out) override;

 private:
  TlsAgent* src_;
  TlsAgent* dest_;
  unsigned int record_;
  VoidFunction func_;
  unsigned int counter_;
};

// When we see the ClientKeyExchange from |client|, increment the
// ClientHelloVersion on |server|.
class TlsInspectorClientHelloVersionChanger : public TlsHandshakeFilter {
 public:
  TlsInspectorClientHelloVersionChanger(TlsAgent* server) : server_(server) {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output);

 private:
  TlsAgent* server_;
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

// Set the version number in the ClientHello.
class TlsInspectorClientHelloVersionSetter : public TlsHandshakeFilter {
 public:
  TlsInspectorClientHelloVersionSetter(uint16_t version) : version_(version) {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output);

 private:
  uint16_t version_;
};

}  // namespace nss_test

#endif
