/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Coverity model file in order to avoid false-positive
*/

// In Bug 1248897 we've seen that Coverity thinks that json-cpp allocates
// memmory for the strings that are used as indexes, this is wrong and this
// model of CZString fixes this.
namespace Json {
class Value {
 private:
  class CZString {
   private:
    char const* cstr_;

   public:
    ~CZString() {
      // Don't do anything since most of the time cstr_ only stores address of
      // str
      __coverity_escape__(static_cast<void*>(cstr_));
    }
  };
};
}  // namespace Json
