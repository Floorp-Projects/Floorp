/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_parser.h"

namespace nss_test {

bool TlsParser::Read(uint8_t* val) {
  if (remaining() < 1) {
    return false;
  }
  *val = *ptr();
  consume(1);
  return true;
}

bool TlsParser::Read(uint32_t* val, size_t size) {
  if (size > sizeof(uint32_t)) {
    return false;
  }

  uint32_t v = 0;
  for (size_t i = 0; i < size; ++i) {
    uint8_t tmp;
    if (!Read(&tmp)) {
      return false;
    }

    v = (v << 8) | tmp;
  }

  *val = v;
  return true;
}

bool TlsParser::Read(DataBuffer* val, size_t len) {
  if (remaining() < len) {
    return false;
  }

  val->Assign(ptr(), len);
  consume(len);
  return true;
}

bool TlsParser::ReadVariable(DataBuffer* val, size_t len_size) {
  uint32_t len;
  if (!Read(&len, len_size)) {
    return false;
  }
  return Read(val, len);
}

bool TlsParser::Skip(size_t len) {
  if (len > remaining()) {
    return false;
  }
  consume(len);
  return true;
}

bool TlsParser::SkipVariable(size_t len_size) {
  uint32_t len;
  if (!Read(&len, len_size)) {
    return false;
  }
  return Skip(len);
}

}  // namespace nss_test
