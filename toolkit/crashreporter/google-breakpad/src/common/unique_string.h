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

#ifndef COMMON_UNIQUE_STRING_H_
#define COMMON_UNIQUE_STRING_H_

#include <map>
#include <string>
#include "common/using_std_string.h"

namespace google_breakpad {

// Abstract type
class UniqueString;

// Unique-ify a string.  |ToUniqueString| can never return NULL.
const UniqueString* ToUniqueString(string);

// ditto, starting instead from the first n characters of a C string
const UniqueString* ToUniqueString_n(const char* str, size_t n);

// Pull chars out of the string.  No range checking.
const char Index(const UniqueString*, int);

// Get the contained C string (debugging only)
const char* const FromUniqueString(const UniqueString*);

// Do a strcmp-style comparison on the contained C string
int StrcmpUniqueString(const UniqueString*, const UniqueString*);

// Less-than comparison of two UniqueStrings, usable for std::sort.
bool LessThan_UniqueString(const UniqueString*, const UniqueString*);

// Some handy pre-uniqified strings.  Z is an escape character:
//   ZS        '$'
//   ZD        '.'
//   Zeq       '='
//   Zplus     '+'
//   Zstar     '*'
//   Zslash    '/'
//   Zpercent  '%'
//   Zat       '@'
//   Zcaret    '^'

// Note that ustr__empty and (UniqueString*)NULL are considered
// to be different.
//
// Unfortunately these have to be written as functions so as to
// make them safe to use in static initialisers.

// ""
inline static const UniqueString* ustr__empty() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("");
  return us;
}

// "$eip"
inline static const UniqueString* ustr__ZSeip() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("$eip");
  return us;
}

// "$ebp"
inline static const UniqueString* ustr__ZSebp() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("$ebp");
  return us;
}

// "$esp"
inline static const UniqueString* ustr__ZSesp() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("$esp");
  return us;
}

// "$ebx"
inline static const UniqueString* ustr__ZSebx() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("$ebx");
  return us;
}

// "$esi"
inline static const UniqueString* ustr__ZSesi() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("$esi");
  return us;
}

// "$edi"
inline static const UniqueString* ustr__ZSedi() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("$edi");
  return us;
}

// ".cbCalleeParams"
inline static const UniqueString* ustr__ZDcbCalleeParams() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString(".cbCalleeParams");
  return us;
}

// ".cbSavedRegs"
inline static const UniqueString* ustr__ZDcbSavedRegs() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString(".cbSavedRegs");
  return us;
}

// ".cbLocals"
inline static const UniqueString* ustr__ZDcbLocals() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString(".cbLocals");
  return us;
}

// ".raSearchStart"
inline static const UniqueString* ustr__ZDraSearchStart() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString(".raSearchStart");
  return us;
}

// ".raSearch"
inline static const UniqueString* ustr__ZDraSearch() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString(".raSearch");
  return us;
}

// ".cbParams"
inline static const UniqueString* ustr__ZDcbParams() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString(".cbParams");
  return us;
}

// "+"
inline static const UniqueString* ustr__Zplus() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("+");
  return us;
}

// "-"
inline static const UniqueString* ustr__Zminus() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("-");
  return us;
}

// "*"
inline static const UniqueString* ustr__Zstar() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("*");
  return us;
}

// "/"
inline static const UniqueString* ustr__Zslash() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("/");
  return us;
}

// "%"
inline static const UniqueString* ustr__Zpercent() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("%");
  return us;
}

// "@"
inline static const UniqueString* ustr__Zat() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("@");
  return us;
}

// "^"
inline static const UniqueString* ustr__Zcaret() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("^");
  return us;
}

// "="
inline static const UniqueString* ustr__Zeq() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("=");
  return us;
}

// ".cfa"
inline static const UniqueString* ustr__ZDcfa() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString(".cfa");
  return us;
}

// ".ra"
inline static const UniqueString* ustr__ZDra() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString(".ra");
  return us;
}

// "pc"
inline static const UniqueString* ustr__pc() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("pc");
  return us;
}

// "lr"
inline static const UniqueString* ustr__lr() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("lr");
  return us;
}

// "sp"
inline static const UniqueString* ustr__sp() {
  static const UniqueString* us = NULL;
  if (!us) us = ToUniqueString("sp");
  return us;
}

template <typename ValueType>
class UniqueStringMap
{
 private:
  static const int N_FIXED = 10;

 public:
  UniqueStringMap() : n_fixed_(0), n_sets_(0), n_gets_(0), n_clears_(0) {};
  ~UniqueStringMap() {};

  // Empty out the map.
  void clear() {
    ++n_clears_;
    map_.clear();
    n_fixed_ = 0;
  }

  // Do "map[ix] = v".
  void set(const UniqueString* ix, ValueType v) {
    ++n_sets_;
    int i;
    for (i = 0; i < n_fixed_; ++i) {
      if (fixed_keys_[i] == ix) {
        fixed_vals_[i] = v;
        return;
      }
    }
    if (n_fixed_ < N_FIXED) {
      i = n_fixed_;
      fixed_keys_[i] = ix;
      fixed_vals_[i] = v;
      ++n_fixed_;
    } else {
      map_[ix] = v;
    }
  }

  // Lookup 'ix' in the map, and also return a success/fail boolean.
  ValueType get(/*OUT*/bool* have, const UniqueString* ix) const {
    ++n_gets_;
    int i;
    for (i = 0; i < n_fixed_; ++i) {
      if (fixed_keys_[i] == ix) {
        *have = true;
        return fixed_vals_[i];
      }
    }
    typename std::map<const UniqueString*, ValueType>::const_iterator it
        = map_.find(ix);
    if (it == map_.end()) {
      *have = false;
      return ValueType();
    } else {
      *have = true;
      return it->second;
    }
  };

  // Lookup 'ix' in the map, and return zero if it is not present.
  ValueType get(const UniqueString* ix) const {
    ++n_gets_;
    bool found;
    ValueType v = get(&found, ix);
    return found ? v : ValueType();
  }

  // Find out whether 'ix' is in the map.
  bool have(const UniqueString* ix) const {
    ++n_gets_;
    bool found;
    (void)get(&found, ix);
    return found;
  }

  // Copy the contents to a std::map, generally for testing.
  void copy_to_map(std::map<const UniqueString*, ValueType>* m) const {
    m->clear();
    int i;
    for (i = 0; i < n_fixed_; ++i) {
      (*m)[fixed_keys_[i]] = fixed_vals_[i];
    }
    m->insert(map_.begin(), map_.end());
  }

  // Note that users of this class rely on having also a sane
  // assignment operator.  The default one is OK, though.
  // AFAICT there are no uses of the copy constructor, but if
  // there were, the default one would also suffice.

 private:
  // Quick (hopefully) cache
  const UniqueString* fixed_keys_[N_FIXED];
  ValueType           fixed_vals_[N_FIXED];
  int                 n_fixed_;  // 0 .. N_FIXED inclusive
  // Fallback storage when the cache is filled
  std::map<const UniqueString*, ValueType> map_;

  // For tracking usage stats.
  mutable int n_sets_, n_gets_, n_clears_;
};

}  // namespace google_breakpad

#endif  // COMMON_UNIQUE_STRING_H_
