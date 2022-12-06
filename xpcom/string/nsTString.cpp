/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTString.h"
#include "nsString.h"
#include "prdtoa.h"

/**
 * nsTString::SetCharAt
 */

template <typename T>
bool nsTString<T>::SetCharAt(char16_t aChar, index_type aIndex) {
  if (aIndex >= this->mLength) {
    return false;
  }

  if (!this->EnsureMutable()) {
    this->AllocFailed(this->mLength);
  }

  this->mData[aIndex] = char_type(aChar);
  return true;
}

template <typename T>
void nsTString<T>::Rebind(const char_type* data, size_type length) {
  // If we currently own a buffer, release it.
  this->Finalize();

  this->SetData(const_cast<char_type*>(data), length, DataFlags::TERMINATED);
  this->AssertValidDependentString();
}

template class nsTString<char>;
template class nsTString<char16_t>;

template class nsTAutoStringN<char, 64>;
template class nsTAutoStringN<char16_t, 64>;
