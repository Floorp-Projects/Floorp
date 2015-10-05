/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_filter_h_
#define tls_filter_h_

#include <memory>
#include <vector>

#include "test_io.h"
#include "tls_parser.h"

namespace nss_test {

// Abstract filter that operates on entire (D)TLS records.
class TlsRecordFilter : public PacketFilter {
 public:
  TlsRecordFilter() : count_(0) {}

  virtual bool Filter(const DataBuffer& input, DataBuffer* output);

  // Report how many packets were altered by the filter.
  size_t filtered_packets() const { return count_; }

 protected:
  virtual bool FilterRecord(uint8_t content_type, uint16_t version,
                            const DataBuffer& data, DataBuffer* changed) = 0;
 private:
  size_t ApplyFilter(uint8_t content_type, uint16_t version,
                     const DataBuffer& record, DataBuffer* output,
                     size_t offset, bool* changed);

  size_t count_;
};

// Abstract filter that operates on handshake messages rather than records.
// This assumes that the handshake messages are written in a block as entire
// records and that they don't span records or anything crazy like that.
class TlsHandshakeFilter : public TlsRecordFilter {
 public:
  TlsHandshakeFilter() {}

  // Reads the length from the record header.
  // This also reads the DTLS fragment information and checks it.
  static bool ReadLength(TlsParser* parser, uint16_t version, uint32_t *length);

 protected:
  virtual bool FilterRecord(uint8_t content_type, uint16_t version,
                            const DataBuffer& input, DataBuffer* output);
  virtual bool FilterHandshake(uint16_t version, uint8_t handshake_type,
                               const DataBuffer& input, DataBuffer* output) = 0;

 private:
  size_t ApplyFilter(uint16_t version, uint8_t handshake_type,
                     const DataBuffer& record, DataBuffer* output,
                     size_t length_offset, size_t value_offset, bool* changed);
};

// Make a copy of the first instance of a handshake message.
class TlsInspectorRecordHandshakeMessage : public TlsHandshakeFilter {
 public:
  TlsInspectorRecordHandshakeMessage(uint8_t handshake_type)
      : handshake_type_(handshake_type), buffer_() {}

  virtual bool FilterHandshake(uint16_t version, uint8_t handshake_type,
                               const DataBuffer& input, DataBuffer* output);

  const DataBuffer& buffer() const { return buffer_; }

 private:
  uint8_t handshake_type_;
  DataBuffer buffer_;
};

// Records an alert.  If an alert has already been recorded, it won't save the
// new alert unless the old alert is a warning and the new one is fatal.
class TlsAlertRecorder : public TlsRecordFilter {
 public:
  TlsAlertRecorder() : level_(255), description_(255) {}

  virtual bool FilterRecord(uint8_t content_type, uint16_t version,
                            const DataBuffer& input, DataBuffer* output);

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

  virtual bool Filter(const DataBuffer& input, DataBuffer* output);

  // Takes ownership of the filter.
  void Add(PacketFilter* filter) {
    filters_.push_back(filter);
  }

 private:
  std::vector<PacketFilter*> filters_;
};

}  // namespace nss_test

#endif
