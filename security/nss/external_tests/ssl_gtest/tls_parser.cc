/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_parser.h"

// Process DTLS Records
#define CHECK_LENGTH(expected)                \
  do {                                        \
    if (remaining() < expected) return false; \
  } while (0)

bool TlsParser::Read(unsigned char* val) {
  if (remaining() < 1) {
    return false;
  }
  *val = *ptr();
  consume(1);
  return true;
}

bool TlsParser::Read(unsigned char* val, size_t len) {
  if (remaining() < len) {
    return false;
  }

  if (val) {
    memcpy(val, ptr(), len);
  }
  consume(len);

  return true;
}

bool TlsRecordParser::NextRecord(uint8_t* ct,
                                 std::auto_ptr<DataBuffer>* buffer) {
  if (!remaining()) return false;

  CHECK_LENGTH(5U);
  const uint8_t* ctp = reinterpret_cast<const uint8_t*>(ptr());
  consume(3);  // ct + version

  const uint16_t* tmp = reinterpret_cast<const uint16_t*>(ptr());
  size_t length = ntohs(*tmp);
  consume(2);

  CHECK_LENGTH(length);
  DataBuffer* db = new DataBuffer(ptr(), length);
  consume(length);

  *ct = *ctp;
  buffer->reset(db);

  return true;
}
