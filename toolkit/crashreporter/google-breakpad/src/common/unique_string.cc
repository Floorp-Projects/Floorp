// Copyright (c) 2013 Google Inc.
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

#include <string>
#include <map>

#include <stdlib.h>
#include <string.h>

#include "common/unique_string.h"

namespace google_breakpad {

///////////////////////////////////////////////////////////////////
// UniqueString
//
class UniqueString {
 public:
  UniqueString(string str) { str_ = strdup(str.c_str()); }
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

//
///////////////////////////////////////////////////////////////////


static UniqueStringUniverse* sUSU = NULL;


// This isn't threadsafe.
const UniqueString* ToUniqueString(string str) {
  if (!sUSU) {
    sUSU = new UniqueStringUniverse();
  }
  return sUSU->FindOrCopy(str);
}

// This isn't threadsafe.
const UniqueString* ToUniqueString_n(const char* str, size_t n) {
  if (!sUSU) {
    sUSU = new UniqueStringUniverse();
  }
  string key(str, n);
  return sUSU->FindOrCopy(key);
}

const char Index(const UniqueString* us, int ix)
{
  return us->str_[ix];
}

const char* const FromUniqueString(const UniqueString* ustr)
{
  return ustr->str_;
}

int StrcmpUniqueString(const UniqueString* us1, const UniqueString* us2) {
  return strcmp(us1->str_, us2->str_);
}

bool LessThan_UniqueString(const UniqueString* us1, const UniqueString* us2) {
  int r = StrcmpUniqueString(us1, us2);
  return r < 0;
}

}  // namespace google_breakpad
