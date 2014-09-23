/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_parser_h_
#define tls_parser_h_

#include <memory>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include "databuffer.h"

const uint8_t kTlsChangeCipherSpecType = 0x14;
const uint8_t kTlsHandshakeType = 0x16;

const uint8_t kTlsHandshakeCertificate = 0x0b;
const uint8_t kTlsHandshakeServerKeyExchange = 0x0c;

const uint8_t kTlsFakeChangeCipherSpec[] = {
    kTlsChangeCipherSpecType,        // Type
    0xfe,                     0xff,  // Version
    0x00,                     0x00, 0x00, 0x00,
    0x00,                     0x00, 0x00, 0x10,  // Fictitious sequence #
    0x00,                     0x01,              // Length
    0x01                                         // Value
};

class TlsParser {
 public:
  TlsParser(const unsigned char *data, size_t len)
      : buffer_(data, len), offset_(0) {}

  bool Read(unsigned char *val);

  // Read an integral type of specified width.
  bool Read(uint32_t *val, size_t len) {
    if (len > sizeof(uint32_t)) return false;

    *val = 0;

    for (size_t i = 0; i < len; ++i) {
      unsigned char tmp;

      (*val) <<= 8;
      if (!Read(&tmp)) return false;

      *val += tmp;
    }

    return true;
  }

  bool Read(unsigned char *val, size_t len);
  size_t remaining() const { return buffer_.len() - offset_; }

 private:
  void consume(size_t len) { offset_ += len; }
  const uint8_t *ptr() const { return buffer_.data() + offset_; }

  DataBuffer buffer_;
  size_t offset_;
};

class TlsRecordParser {
 public:
  TlsRecordParser(const unsigned char *data, size_t len)
      : buffer_(data, len), offset_(0) {}

  bool NextRecord(uint8_t *ct, std::auto_ptr<DataBuffer> *buffer);

 private:
  size_t remaining() const { return buffer_.len() - offset_; }
  const uint8_t *ptr() const { return buffer_.data() + offset_; }
  void consume(size_t len) { offset_ += len; }

  DataBuffer buffer_;
  size_t offset_;
};

#endif
