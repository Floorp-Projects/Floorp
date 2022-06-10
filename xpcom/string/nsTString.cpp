/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTString.h"
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

/**
 * nsTString::ToDouble
 */
template <>
double nsTString<char>::ToDouble(bool aAllowTrailingChars,
                                 nsresult* aErrorCode) const {
  double res = 0.0;
  if (this->Length() > 0) {
    char* conv_stopped;
    const char* str = this->get();
    // Use PR_strtod, not strtod, since we don't want locale involved.
    res = PR_strtod(str, &conv_stopped);
    if (aAllowTrailingChars && conv_stopped != str) {
      *aErrorCode = NS_OK;
    } else if (!aAllowTrailingChars && conv_stopped == str + this->Length()) {
      *aErrorCode = NS_OK;
    } else {
      *aErrorCode = NS_ERROR_ILLEGAL_VALUE;
    }
  } else {
    // The string was too short (0 characters)
    *aErrorCode = NS_ERROR_ILLEGAL_VALUE;
  }
  return res;
}

template <>
double nsTString<char16_t>::ToDouble(bool aAllowTrailingChars,
                                     nsresult* aErrorCode) const {
  NS_LossyConvertUTF16toASCII cString(BeginReading(), Length());
  return aAllowTrailingChars ? cString.ToDoubleAllowTrailingChars(aErrorCode)
                             : cString.ToDouble(aErrorCode);
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
