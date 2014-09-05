/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JSSTREAMWRITER_H
#define JSSTREAMWRITER_H

#include <ostream>
#include <stdlib.h>
#include "nsDeque.h"

class JSStreamWriter
{
public:
  explicit JSStreamWriter(std::ostream& aStream);
  ~JSStreamWriter();

  void BeginObject();
  void EndObject();
  void BeginArray();
  void EndArray();
  void Name(const char *name);
  void Value(int value);
  void Value(double value);
  void Value(const char *value, size_t valueLength);
  void Value(const char *value);
  template <typename T>
  void NameValue(const char *aName, T aValue)
  {
    Name(aName);
    Value(aValue);
  }

private:
  std::ostream& mStream;
  bool mNeedsComma;
  bool mNeedsName;

  nsDeque mStack;

  // This class can't be copied
  JSStreamWriter(const JSStreamWriter&);
  JSStreamWriter& operator=(const JSStreamWriter&);

  void* operator new(size_t);
  void* operator new[](size_t);
  void operator delete(void*) {
    // Since JSStreamWriter has a virtual destructor the compiler
    // has to provide a destructor in the object file that will call
    // operate delete in case there is a derived class since its
    // destructor won't know how to free this instance.
    abort();
  }
  void operator delete[](void*);
};

#endif
