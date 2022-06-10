/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTStringRepr.h"

#include "nsString.h"

namespace mozilla::detail {

template <typename T>
typename nsTStringRepr<T>::char_type nsTStringRepr<T>::First() const {
  MOZ_RELEASE_ASSERT(this->mLength > 0, "|First()| called on an empty string");
  return this->mData[0];
}

template <typename T>
typename nsTStringRepr<T>::char_type nsTStringRepr<T>::Last() const {
  MOZ_RELEASE_ASSERT(this->mLength > 0, "|Last()| called on an empty string");
  return this->mData[this->mLength - 1];
}

template <typename T>
bool nsTStringRepr<T>::Equals(const self_type& aStr) const {
  return this->mLength == aStr.mLength &&
         char_traits::compare(this->mData, aStr.mData, this->mLength) == 0;
}

template <typename T>
bool nsTStringRepr<T>::Equals(const self_type& aStr,
                              comparator_type aComp) const {
  return this->mLength == aStr.mLength &&
         aComp(this->mData, aStr.mData, this->mLength, aStr.mLength) == 0;
}

template <typename T>
bool nsTStringRepr<T>::Equals(const substring_tuple_type& aTuple) const {
  return Equals(substring_type(aTuple));
}

template <typename T>
bool nsTStringRepr<T>::Equals(const substring_tuple_type& aTuple,
                              comparator_type aComp) const {
  return Equals(substring_type(aTuple), aComp);
}

template <typename T>
bool nsTStringRepr<T>::Equals(const char_type* aData) const {
  // unfortunately, some callers pass null :-(
  if (!aData) {
    MOZ_ASSERT_UNREACHABLE("null data pointer");
    return this->mLength == 0;
  }

  // XXX avoid length calculation?
  size_type length = char_traits::length(aData);
  return this->mLength == length &&
         char_traits::compare(this->mData, aData, this->mLength) == 0;
}

template <typename T>
bool nsTStringRepr<T>::Equals(const char_type* aData,
                              comparator_type aComp) const {
  // unfortunately, some callers pass null :-(
  if (!aData) {
    MOZ_ASSERT_UNREACHABLE("null data pointer");
    return this->mLength == 0;
  }

  // XXX avoid length calculation?
  size_type length = char_traits::length(aData);
  return this->mLength == length &&
         aComp(this->mData, aData, this->mLength, length) == 0;
}

template <typename T>
bool nsTStringRepr<T>::EqualsASCII(const char* aData, size_type aLen) const {
  return this->mLength == aLen &&
         char_traits::compareASCII(this->mData, aData, aLen) == 0;
}

template <typename T>
bool nsTStringRepr<T>::EqualsASCII(const char* aData) const {
  return char_traits::compareASCIINullTerminated(this->mData, this->mLength,
                                                 aData) == 0;
}

template <typename T>
bool nsTStringRepr<T>::EqualsLatin1(const char* aData,
                                    const size_type aLength) const {
  return (this->mLength == aLength) &&
         char_traits::equalsLatin1(this->mData, aData, aLength);
}

template <typename T>
bool nsTStringRepr<T>::LowerCaseEqualsASCII(const char* aData,
                                            size_type aLen) const {
  return this->mLength == aLen &&
         char_traits::compareLowerCaseToASCII(this->mData, aData, aLen) == 0;
}

template <typename T>
bool nsTStringRepr<T>::LowerCaseEqualsASCII(const char* aData) const {
  return char_traits::compareLowerCaseToASCIINullTerminated(
             this->mData, this->mLength, aData) == 0;
}

}  // namespace mozilla::detail
