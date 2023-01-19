/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PK11GTEST_JSON_H_
#define PK11GTEST_JSON_H_

#include <functional>
#include <iostream>
#include <vector>
#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"
#include "secoidt.h"

// If we make a few assumptions about the file, parsing JSON can be easy.
// This is not a full parser, it only works on a narrow set of inputs.
class JsonReader {
 public:
  JsonReader(const std::string& n);

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
  uint64_t ReadInt();
  // No input checking, no unicode, no escaping (not even \"), just read ASCII.
  std::string ReadString();
  std::string ReadLabel();
  std::vector<uint8_t> ReadHex();
  SECOidTag ReadHash();

  bool NextItem(uint8_t h = '{', uint8_t t = '}');
  bool NextItemArray() { return NextItem('[', ']'); }
  void SkipValue();

 private:
  void TopUp();
  void SkipWhitespace();
  // This only handles lowercase.
  uint8_t Hex(uint8_t c);

  ScopedPRFileDesc f_;
  uint8_t buf_[4096];
  size_t available_;
  size_t i_;
};

// The way this is expected to work is that this reads the header, then
// passes off the content of each "testGroups" member to `group_handler`.
// That function processes any attributes in that structure, calls
// `WycheproofReadTests` to load individual cases and runs those tests.
void WycheproofHeader(const std::string& name, const std::string& algorithm,
                      const std::string& schema,
                      std::function<void(JsonReader& r)> group_handler);

template <typename T>
struct id {
  typedef T type;
};

template <typename T>
using nondeduced = typename id<T>::type;

// Read into a block of test cases, handling standard attributes on Wycheproof
// tests.
//
// `T` needs `uint64_t id` and `bool valid` fields.
// `attr_reader` is responsible for reading values into the test case struct.
// `acceptable` determines whether a test marked "acceptable" is valid by
// default. `process_flags` allows for processing the flags on an entry.
//
// Note that this gathers all tests into a vector rather than running tests as
// they arrive.  This is necessary because the testGroup JSON struct might have
// fields that haven't been read when this list is constructed (it doesn't in
// the current files, but this is not guaranteed).  Tests can only run after all
// of the group attributes have been read and processed.
template <typename T>
void WycheproofReadTests(
    JsonReader& r, std::vector<T>* tests,
    const std::function<nondeduced<void(T&, const std::string&, JsonReader&)>>&
        attr_reader,
    bool acceptable = true,
    const std::function<nondeduced<void(T&, const std::string&,
                                        const std::vector<std::string>&)>>&
        process_flags = nullptr) {
  while (r.NextItemArray()) {
    T tc;
    std::string comment;
    std::string result;
    std::vector<std::string> flags;

    while (r.NextItem()) {
      std::string n = r.ReadLabel();
      if (n == "") {
        break;
      }
      if (n == "tcId") {
        tc.id = r.ReadInt();
      } else if (n == "result") {
        result = r.ReadString();
      } else if (n == "comment") {
        comment = r.ReadString();
      } else if (n == "flags") {
        while (r.NextItemArray()) {
          flags.push_back(r.ReadString());
        }
      } else {
        ASSERT_NO_FATAL_FAILURE(attr_reader(tc, n, r));
      }
    }

    tc.valid = (result == "valid") || (acceptable && result == "acceptable");
    if (process_flags) {
      process_flags(tc, result, flags);
    }

    std::cout << "  tc " << tc.id << ": " << comment << " [" << result;
    for (auto& f : flags) {
      std::cout << ", " << f;
    }
    std::cout << "] expect " << (tc.valid ? "success" : "failure") << std::endl;

    tests->push_back(tc);
  }
}

#endif  // PK11GTEST_JSON_H_
