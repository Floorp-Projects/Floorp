/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTDependentString.h"

template <typename T>
nsTDependentString<T>::nsTDependentString(const char_type* aStart,
                                          const char_type* aEnd)
    : string_type(const_cast<char_type*>(aStart), aEnd - aStart,
                  DataFlags::TERMINATED, ClassFlags(0)) {
  MOZ_RELEASE_ASSERT(aStart <= aEnd, "Overflow!");
  this->AssertValidDependentString();
}

template <typename T>
void nsTDependentString<T>::Rebind(const string_type& str,
                                   index_type startPos) {
  MOZ_ASSERT(str.GetDataFlags() & DataFlags::TERMINATED,
             "Unterminated flat string");

  // If we currently own a buffer, release it.
  this->Finalize();

  size_type strLength = str.Length();

  if (startPos > strLength) {
    startPos = strLength;
  }

  char_type* newData =
      const_cast<char_type*>(static_cast<const char_type*>(str.Data())) +
      startPos;
  size_type newLen = strLength - startPos;
  DataFlags newDataFlags =
      str.GetDataFlags() & (DataFlags::TERMINATED | DataFlags::LITERAL);
  this->SetData(newData, newLen, newDataFlags);
}

template <typename T>
void nsTDependentString<T>::Rebind(const char_type* aStart,
                                   const char_type* aEnd) {
  MOZ_RELEASE_ASSERT(aStart <= aEnd, "Overflow!");
  this->Rebind(aStart, aEnd - aStart);
}
