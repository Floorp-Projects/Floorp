/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

// Copyright (c) 2011, 2013 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>


// This file is derived from the following files in
// toolkit/crashreporter/google-breakpad:
//   src/common/module.cc
//   src/common/unique_string.cc

// There's no internal-only interface for LulCommon.  Hence include
// the external interface directly.
#include "LulCommonExt.h"

#include <stdlib.h>
#include <string.h>

#include <string>
#include <map>


namespace lul {

using std::string;

////////////////////////////////////////////////////////////////
// Module
//
Module::Module(const string &name, const string &os,
               const string &architecture, const string &id) :
    name_(name),
    os_(os),
    architecture_(architecture),
    id_(id) { }

Module::~Module() {
}


////////////////////////////////////////////////////////////////
// UniqueString
//
class UniqueString {
 public:
  explicit UniqueString(string str) { str_ = strdup(str.c_str()); }
  ~UniqueString() { free(reinterpret_cast<void*>(const_cast<char*>(str_))); }
  const char* str_;
};

class UniqueStringUniverse {
public:
  UniqueStringUniverse() {};
  const UniqueString* FindOrCopy(string str) {
    std::map<string, UniqueString*>::iterator it = map_.find(str);
    if (it == map_.end()) {
      UniqueString* ustr = new UniqueString(str);
      map_[str] = ustr;
      return ustr;
    } else {
      return it->second;
    }
  }
private:
  std::map<string, UniqueString*> map_;
};


const UniqueString* ToUniqueString(string str) {
  // For the initialisation of sUniverse to be threadsafe,
  // this relies on C++11's semantics.
  static UniqueStringUniverse sUniverse;
  return sUniverse.FindOrCopy(str);
}

const char* const FromUniqueString(const UniqueString* ustr)
{
  return ustr->str_;
}

}  // namespace lul
