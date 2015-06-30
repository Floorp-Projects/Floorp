/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

// Copyright (c) 2006, 2010, 2012, 2013 Google Inc.
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

// module.h: Define google_breakpad::Module. A Module holds debugging
// information, and can write that information out as a Breakpad
// symbol file.


//  (C) Copyright Greg Colvin and Beman Dawes 1998, 1999.
//  Copyright (c) 2001, 2002 Peter Dimov
//
//  Permission to copy, use, modify, sell and distribute this software
//  is granted provided this copyright notice appears in all copies.
//  This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.
//
//  See http://www.boost.org/libs/smart_ptr/scoped_ptr.htm for documentation.
//


// This file is derived from the following files in
// toolkit/crashreporter/google-breakpad:
//   src/common/unique_string.h
//   src/common/scoped_ptr.h
//   src/common/module.h

// External interface for the "Common" component of LUL.

#ifndef LulCommonExt_h
#define LulCommonExt_h

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <string>
#include <map>
#include <vector>
#include <cstddef>            // for std::ptrdiff_t

#include "mozilla/Assertions.h"

namespace lul {

using std::string;
using std::map;


////////////////////////////////////////////////////////////////
// UniqueString
//

// Abstract type
class UniqueString;

// Get the contained C string (debugging only)
const char* const FromUniqueString(const UniqueString*);

// Is the given string empty (that is, "") ?
bool IsEmptyUniqueString(const UniqueString*);


////////////////////////////////////////////////////////////////
// UniqueStringUniverse
//

// All UniqueStrings live in some specific UniqueStringUniverse.
class UniqueStringUniverse {
public:
  UniqueStringUniverse() {}
  ~UniqueStringUniverse();
  // Convert a |string| to a UniqueString, that lives in this universe.
  const UniqueString* ToUniqueString(string str);
private:
  map<string, UniqueString*> map_;
};


////////////////////////////////////////////////////////////////
// GUID
//

typedef struct {
  uint32_t data1;
  uint16_t data2;
  uint16_t data3;
  uint8_t  data4[8];
} MDGUID;  // GUID

typedef MDGUID GUID;


////////////////////////////////////////////////////////////////
// scoped_ptr
//

//  scoped_ptr mimics a built-in pointer except that it guarantees deletion
//  of the object pointed to, either on destruction of the scoped_ptr or via
//  an explicit reset(). scoped_ptr is a simple solution for simple needs;
//  use shared_ptr or std::auto_ptr if your needs are more complex.

//  *** NOTE ***
//  If your scoped_ptr is a class member of class FOO pointing to a
//  forward declared type BAR (as shown below), then you MUST use a non-inlined
//  version of the destructor.  The destructor of a scoped_ptr (called from
//  FOO's destructor) must have a complete definition of BAR in order to
//  destroy it.  Example:
//
//  -- foo.h --
//  class BAR;
//
//  class FOO {
//   public:
//    FOO();
//    ~FOO();  // Required for sources that instantiate class FOO to compile!
//
//   private:
//    scoped_ptr<BAR> bar_;
//  };
//
//  -- foo.cc --
//  #include "foo.h"
//  FOO::~FOO() {} // Empty, but must be non-inlined to FOO's class definition.

//  scoped_ptr_malloc added by Google
//  When one of these goes out of scope, instead of doing a delete or
//  delete[], it calls free().  scoped_ptr_malloc<char> is likely to see
//  much more use than any other specializations.

//  release() added by Google
//  Use this to conditionally transfer ownership of a heap-allocated object
//  to the caller, usually on method success.

template <typename T>
class scoped_ptr {
 private:

  T* ptr;

  scoped_ptr(scoped_ptr const &);
  scoped_ptr & operator=(scoped_ptr const &);

 public:

  typedef T element_type;

  explicit scoped_ptr(T* p = 0): ptr(p) {}

  ~scoped_ptr() {
    delete ptr;
  }

  void reset(T* p = 0) {
    if (ptr != p) {
      delete ptr;
      ptr = p;
    }
  }

  T& operator*() const {
    MOZ_ASSERT(ptr != 0);
    return *ptr;
  }

  T* operator->() const  {
    MOZ_ASSERT(ptr != 0);
    return ptr;
  }

  bool operator==(T* p) const {
    return ptr == p;
  }

  bool operator!=(T* p) const {
    return ptr != p;
  }

  T* get() const  {
    return ptr;
  }

  void swap(scoped_ptr & b) {
    T* tmp = b.ptr;
    b.ptr = ptr;
    ptr = tmp;
  }

  T* release() {
    T* tmp = ptr;
    ptr = 0;
    return tmp;
  }

 private:

  // no reason to use these: each scoped_ptr should have its own object
  template <typename U> bool operator==(scoped_ptr<U> const& p) const;
  template <typename U> bool operator!=(scoped_ptr<U> const& p) const;
};

template<typename T> inline
void swap(scoped_ptr<T>& a, scoped_ptr<T>& b) {
  a.swap(b);
}

template<typename T> inline
bool operator==(T* p, const scoped_ptr<T>& b) {
  return p == b.get();
}

template<typename T> inline
bool operator!=(T* p, const scoped_ptr<T>& b) {
  return p != b.get();
}

//  scoped_array extends scoped_ptr to arrays. Deletion of the array pointed to
//  is guaranteed, either on destruction of the scoped_array or via an explicit
//  reset(). Use shared_array or std::vector if your needs are more complex.

template<typename T>
class scoped_array {
 private:

  T* ptr;

  scoped_array(scoped_array const &);
  scoped_array & operator=(scoped_array const &);

 public:

  typedef T element_type;

  explicit scoped_array(T* p = 0) : ptr(p) {}

  ~scoped_array() {
    delete[] ptr;
  }

  void reset(T* p = 0) {
    if (ptr != p) {
      delete [] ptr;
      ptr = p;
    }
  }

  T& operator[](std::ptrdiff_t i) const {
    MOZ_ASSERT(ptr != 0);
    MOZ_ASSERT(i >= 0);
    return ptr[i];
  }

  bool operator==(T* p) const {
    return ptr == p;
  }

  bool operator!=(T* p) const {
    return ptr != p;
  }

  T* get() const {
    return ptr;
  }

  void swap(scoped_array & b) {
    T* tmp = b.ptr;
    b.ptr = ptr;
    ptr = tmp;
  }

  T* release() {
    T* tmp = ptr;
    ptr = 0;
    return tmp;
  }

 private:

  // no reason to use these: each scoped_array should have its own object
  template <typename U> bool operator==(scoped_array<U> const& p) const;
  template <typename U> bool operator!=(scoped_array<U> const& p) const;
};

template<class T> inline
void swap(scoped_array<T>& a, scoped_array<T>& b) {
  a.swap(b);
}

template<typename T> inline
bool operator==(T* p, const scoped_array<T>& b) {
  return p == b.get();
}

template<typename T> inline
bool operator!=(T* p, const scoped_array<T>& b) {
  return p != b.get();
}


// This class wraps the c library function free() in a class that can be
// passed as a template argument to scoped_ptr_malloc below.
class ScopedPtrMallocFree {
 public:
  inline void operator()(void* x) const {
    free(x);
  }
};

// scoped_ptr_malloc<> is similar to scoped_ptr<>, but it accepts a
// second template argument, the functor used to free the object.

template<typename T, typename FreeProc = ScopedPtrMallocFree>
class scoped_ptr_malloc {
 private:

  T* ptr;

  scoped_ptr_malloc(scoped_ptr_malloc const &);
  scoped_ptr_malloc & operator=(scoped_ptr_malloc const &);

 public:

  typedef T element_type;

  explicit scoped_ptr_malloc(T* p = 0): ptr(p) {}

  ~scoped_ptr_malloc() {
    free_((void*) ptr);
  }

  void reset(T* p = 0) {
    if (ptr != p) {
      free_((void*) ptr);
      ptr = p;
    }
  }

  T& operator*() const {
    MOZ_ASSERT(ptr != 0);
    return *ptr;
  }

  T* operator->() const {
    MOZ_ASSERT(ptr != 0);
    return ptr;
  }

  bool operator==(T* p) const {
    return ptr == p;
  }

  bool operator!=(T* p) const {
    return ptr != p;
  }

  T* get() const {
    return ptr;
  }

  void swap(scoped_ptr_malloc & b) {
    T* tmp = b.ptr;
    b.ptr = ptr;
    ptr = tmp;
  }

  T* release() {
    T* tmp = ptr;
    ptr = 0;
    return tmp;
  }

 private:

  // no reason to use these: each scoped_ptr_malloc should have its own object
  template <typename U, typename GP>
  bool operator==(scoped_ptr_malloc<U, GP> const& p) const;
  template <typename U, typename GP>
  bool operator!=(scoped_ptr_malloc<U, GP> const& p) const;

  static FreeProc const free_;
};

template<typename T, typename FP>
FP const scoped_ptr_malloc<T,FP>::free_ = FP();

template<typename T, typename FP> inline
void swap(scoped_ptr_malloc<T,FP>& a, scoped_ptr_malloc<T,FP>& b) {
  a.swap(b);
}

template<typename T, typename FP> inline
bool operator==(T* p, const scoped_ptr_malloc<T,FP>& b) {
  return p == b.get();
}

template<typename T, typename FP> inline
bool operator!=(T* p, const scoped_ptr_malloc<T,FP>& b) {
  return p != b.get();
}


////////////////////////////////////////////////////////////////
// Module
//

// A Module represents the contents of a module, and supports methods
// for adding information produced by parsing STABS or DWARF data
// --- possibly both from the same file --- and then writing out the
// unified contents as a Breakpad-format symbol file.
class Module {
public:
  // The type of addresses and sizes in a symbol table.
  typedef uint64_t Address;

  // Representation of an expression.  This can either be a postfix
  // expression, in which case it is stored as a string, or a simple
  // expression of the form (identifier + imm) or *(identifier + imm).
  // It can also be invalid (denoting "no value").
  enum ExprHow {
    kExprInvalid = 1,
    kExprPostfix,
    kExprSimple,
    kExprSimpleMem
  };

  struct Expr {
    // Construct a simple-form expression
    Expr(const UniqueString* ident, long offset, bool deref) {
      if (IsEmptyUniqueString(ident)) {
        Expr();
      } else {
        postfix_ = "";
        ident_ = ident;
        offset_ = offset;
        how_ = deref ? kExprSimpleMem : kExprSimple;
      }
    }

    // Construct an invalid expression
    Expr() {
      postfix_ = "";
      ident_ = nullptr;
      offset_ = 0;
      how_ = kExprInvalid;
    }

    // Return the postfix expression string, either directly,
    // if this is a postfix expression, or by synthesising it
    // for a simple expression.
    std::string getExprPostfix() const {
      switch (how_) {
        case kExprPostfix:
          return postfix_;
        case kExprSimple:
        case kExprSimpleMem: {
          char buf[40];
          sprintf(buf, " %ld %c%s", labs(offset_), offset_ < 0 ? '-' : '+',
                                    how_ == kExprSimple ? "" : " ^");
          return std::string(FromUniqueString(ident_)) + std::string(buf);
        }
        case kExprInvalid:
        default:
          MOZ_ASSERT(0 && "getExprPostfix: invalid Module::Expr type");
          return "Expr::genExprPostfix: kExprInvalid";
      }
    }

    // The identifier that gives the starting value for simple expressions.
    const UniqueString* ident_;
    // The offset to add for simple expressions.
    long        offset_;
    // The Postfix expression string to evaluate for non-simple expressions.
    std::string postfix_;
    // The operation expressed by this expression.
    ExprHow     how_;
  };

  // A map from register names to expressions that recover
  // their values. This can represent a complete set of rules to
  // follow at some address, or a set of changes to be applied to an
  // extant set of rules.
  // NOTE! there are two completely different types called RuleMap.  This
  // is one of them.
  typedef std::map<const UniqueString*, Expr> RuleMap;

  // A map from addresses to RuleMaps, representing changes that take
  // effect at given addresses.
  typedef std::map<Address, RuleMap> RuleChangeMap;

  // A range of 'STACK CFI' stack walking information. An instance of
  // this structure corresponds to a 'STACK CFI INIT' record and the
  // subsequent 'STACK CFI' records that fall within its range.
  struct StackFrameEntry {
    // The starting address and number of bytes of machine code this
    // entry covers.
    Address address, size;

    // The initial register recovery rules, in force at the starting
    // address.
    RuleMap initial_rules;

    // A map from addresses to rule changes. To find the rules in
    // force at a given address, start with initial_rules, and then
    // apply the changes given in this map for all addresses up to and
    // including the address you're interested in.
    RuleChangeMap rule_changes;
  };

  // Create a new module with the given name, operating system,
  // architecture, and ID string.
  Module(const std::string &name, const std::string &os,
         const std::string &architecture, const std::string &id);
  ~Module();

private:

  // Module header entries.
  std::string name_, os_, architecture_, id_;
};


}  // namespace lul

#endif // LulCommonExt_h
