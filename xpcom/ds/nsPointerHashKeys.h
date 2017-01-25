/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Definitions for nsPtrHashKey<T> and nsVoidPtrHashKey.  */

#ifndef nsPointerHashKeys_h
#define nsPointerHashKeys_h

#include "nscore.h"

#include "mozilla/Attributes.h"

/**
 * hashkey wrapper using T* KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
template<class T>
class nsPtrHashKey : public PLDHashEntryHdr
{
public:
  typedef T* KeyType;
  typedef const T* KeyTypePointer;

  explicit nsPtrHashKey(const T* aKey) : mKey(const_cast<T*>(aKey)) {}
  nsPtrHashKey(const nsPtrHashKey<T>& aToCopy) : mKey(aToCopy.mKey) {}
  ~nsPtrHashKey() {}

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return aKey == mKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return NS_PTR_TO_UINT32(aKey) >> 2;
  }
  enum { ALLOW_MEMMOVE = true };

protected:
  T* MOZ_NON_OWNING_REF mKey;
};

typedef nsPtrHashKey<const void> nsVoidPtrHashKey;

#endif // nsPointerHashKeys_h
