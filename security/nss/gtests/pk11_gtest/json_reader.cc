/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "json_reader.h"

#include "pk11pub.h"

JsonReader::JsonReader(const std::string& n) : buf_(), available_(0), i_(0) {
  f_.reset(PR_Open(n.c_str(), PR_RDONLY, 00600));
  EXPECT_TRUE(f_) << "error opening vectors from: " << n;
  buf_[0] = 0;
}

uint64_t JsonReader::ReadInt() {
  SkipWhitespace();
  uint8_t c = peek();
  uint64_t v = 0;
  while (c >= '0' && c <= '9') {
    v = v * 10 + c - '0';
    next();
    c = peek();
  }
  return v;
}

// No input checking, no unicode, no escaping (not even \"), just read ASCII.
std::string JsonReader::ReadString() {
  SkipWhitespace();
  if (peek() != '"') {
    return "";
  }
  next();

  std::string s;
  uint8_t c = take();
  while (c != '"') {
    s.push_back(c);
    c = take();
  }
  return s;
}

std::string JsonReader::ReadLabel() {
  std::string s = ReadString();
  SkipWhitespace();
  EXPECT_EQ(take(), ':');
  return s;
}

std::vector<uint8_t> JsonReader::ReadHex() {
  SkipWhitespace();
  uint8_t c = take();
  EXPECT_EQ(c, '"');
  std::vector<uint8_t> v;
  c = take();
  while (c != '"') {
    v.push_back(JsonReader::Hex(c) << 4 | JsonReader::Hex(take()));
    c = take();
  }
  return v;
}

SECOidTag JsonReader::ReadHash() {
  std::string s = ReadString();
  if (s == "SHA-1") {
    return SEC_OID_SHA1;
  }
  if (s == "SHA-224") {
    return SEC_OID_SHA224;
  }
  if (s == "SHA-256") {
    return SEC_OID_SHA256;
  }
  if (s == "SHA-384") {
    return SEC_OID_SHA384;
  }
  if (s == "SHA-512") {
    return SEC_OID_SHA512;
  }
  ADD_FAILURE() << "unsupported hash";
  return SEC_OID_UNKNOWN;
}

SECStatus JsonReader::ReadSECStatus() {
  std::string s = ReadString();
  if (s == "SECSuccess") {
    return SECSuccess;
  } else if (s == "SECFailure") {
    return SECFailure;
  } else if (s == "SECWouldBlock") {
    return SECWouldBlock;
  }
  ADD_FAILURE() << "unknown SECStatus";
  return SECFailure;
}

bool JsonReader::ReadBool() {
  std::string s = ReadString();
  if (s == "true") {
    return true;
  } else if (s == "false") {
    return false;
  }
  ADD_FAILURE() << "not a bool";
  return false;
}

bool JsonReader::NextItem(uint8_t h, uint8_t t) {
  SkipWhitespace();
  switch (uint8_t c = take()) {
    case ',':
      return true;
    case '{':
    case '[':
      EXPECT_EQ(c, h);
      SkipWhitespace();
      if (peek() == t) {
        next();
        return false;
      }
      return true;
    case '}':
    case ']':
      EXPECT_EQ(c, t);
      return false;
    default:
      ADD_FAILURE() << "Unexpected '" << c << "'";
  }
  return false;
}

void JsonReader::SkipValue() {
  SkipWhitespace();

  uint8_t c = take();
  if (c == '"') {
    do {
      c = take();
    } while (c != '"');

  } else if (c >= '0' && c <= '9') {
    c = peek();
    while (c >= '0' && c <= '9') {
      next();
      c = peek();
    }

  } else if (c == '[') {
    do {
      SkipWhitespace();
      if (peek() != ']') {
        SkipValue();
      }
    } while (NextItemArray());

  } else if (c == '{') {
    do {
      SkipWhitespace();
      if (peek() == '}') {
        continue;
      }

      std::string n = ReadLabel();
      if (n == "") {
        break;
      }
      SkipValue();
    } while (NextItem());

  } else {
    ADD_FAILURE() << "No idea how to skip '" << c << "'";
  }
}

void JsonReader::TopUp() {
  if (available_ > i_) {
    return;
  }
  i_ = 0;
  if (!f_) {
    return;
  }
  PRInt32 res = PR_Read(f_.get(), buf_, sizeof(buf_));
  if (res > 0) {
    available_ = static_cast<size_t>(res);
  } else {
    available_ = 1;
    f_.reset(nullptr);
    buf_[0] = 0;
  }
}

void JsonReader::SkipWhitespace() {
  uint8_t c = peek();
  while (c && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
    next();
    c = peek();
  }
}

// This only handles lowercase.
uint8_t JsonReader::Hex(uint8_t c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  EXPECT_TRUE(c >= 'a' && c <= 'f');
  return c - 'a' + 10;
}

extern std::string g_source_dir;

void WycheproofHeader(const std::string& name, const std::string& algorithm,
                      const std::string& schema,
                      std::function<void(JsonReader& r)> group_handler) {
  std::string basename = name + "_test.json";
  std::string dir = ::g_source_dir + "/../common/wycheproof/source_vectors/";
  std::cout << "Reading tests from: " << basename << std::endl;

  JsonReader r(dir + basename);
  while (r.NextItem()) {
    std::string n = r.ReadLabel();
    if (n == "") {
      break;
    }
    if (n == "algorithm") {
      ASSERT_EQ(algorithm, r.ReadString());
    } else if (n == "generatorVersion" || n == "numberOfTests") {
      r.SkipValue();
    } else if (n == "header") {
      while (r.NextItemArray()) {
        std::cout << "  " << r.ReadString() << std::endl;
      }
    } else if (n == "notes") {
      while (r.NextItem()) {
        std::string note = r.ReadLabel();
        if (note == "") {
          break;
        }
        std::cout << "    " << note << ": " << r.ReadString() << std::endl;
      }
    } else if (n == "schema") {
      ASSERT_EQ(schema, r.ReadString());
    } else if (n == "testGroups") {
      while (r.NextItemArray()) {
        group_handler(r);
      }
    } else {
      FAIL() << "unknown value in header";
    }
  }
}
