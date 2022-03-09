/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define __json_h__
#include <vector>
#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"
#include "pk11pub.h"

// If we make a few assumptions about the file, parsing JSON can be easy.
// This is not a full parser, it only works on a narrow set of inputs.
class JsonReader {
 public:
  JsonReader(const std::string &n) : buf_(), available_(0), i_(0) {
    f_.reset(PR_Open(n.c_str(), PR_RDONLY, 00600));
    EXPECT_TRUE(f_) << "error opening vectors from: " << n;
    buf_[0] = 0;
  }

  void next() { i_++; }
  uint8_t peek() {
    TopUp();
    return buf_[i_];
  }
  uint8_t take() {
    uint8_t v = peek();
    next();
    return v;
  }

  // No input checking, overflow protection, or any safety.
  // Returns 0 if there isn't a number here rather than aborting.
  uint64_t ReadInt() {
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
  std::string ReadString() {
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

  std::string ReadLabel() {
    std::string s = ReadString();
    SkipWhitespace();
    EXPECT_EQ(take(), ':');
    return s;
  }

  std::vector<uint8_t> ReadHex() {
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

  bool NextItem(uint8_t h = '{', uint8_t t = '}') {
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

  bool NextItemArray() { return NextItem('[', ']'); }

  void SkipValue() {
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
    } else {
      ADD_FAILURE() << "No idea how to skip'" << c << "'";
    }
  }

 private:
  void TopUp() {
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

  void SkipWhitespace() {
    uint8_t c = peek();
    while (c && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
      next();
      c = peek();
    }
  }

  // This only handles lowercase.
  uint8_t Hex(uint8_t c) {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    EXPECT_TRUE(c >= 'a' && c <= 'f');
    return c - 'a' + 10;
  }

  ScopedPRFileDesc f_;
  uint8_t buf_[4096];
  size_t available_;
  size_t i_;
};
