/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTHashKeys_h__
#define nsTHashKeys_h__

#include "nsID.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "PLDHashTable.h"
#include <new>

#include "nsString.h"
#include "nsCRTGlue.h"
#include "nsUnicharUtils.h"
#include "nsPointerHashKeys.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <type_traits>
#include <utility>

#include "mozilla/HashFunctions.h"

namespace mozilla {

// These are defined analogously to the HashString overloads in mfbt.

inline uint32_t HashString(const nsAString& aStr) {
  return HashString(aStr.BeginReading(), aStr.Length());
}

inline uint32_t HashString(const nsACString& aStr) {
  return HashString(aStr.BeginReading(), aStr.Length());
}

}  // namespace mozilla

/** @file nsHashKeys.h
 * standard HashKey classes for nsBaseHashtable and relatives. Each of these
 * classes follows the nsTHashtable::EntryType specification
 *
 * Lightweight keytypes provided here:
 * nsStringHashKey
 * nsCStringHashKey
 * nsUint32HashKey
 * nsUint64HashKey
 * nsFloatHashKey
 * IntPtrHashKey
 * nsPtrHashKey
 * nsVoidPtrHashKey
 * nsISupportsHashKey
 * nsIDHashKey
 * nsDepCharHashKey
 * nsCharPtrHashKey
 * nsUnicharPtrHashKey
 * nsGenericHashKey
 */

/**
 * hashkey wrapper using nsAString KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsStringHashKey : public PLDHashEntryHdr {
 public:
  typedef const nsAString& KeyType;
  typedef const nsAString* KeyTypePointer;

  explicit nsStringHashKey(KeyTypePointer aStr) : mStr(*aStr) {}
  nsStringHashKey(const nsStringHashKey&) = delete;
  nsStringHashKey(nsStringHashKey&& aToMove)
      : PLDHashEntryHdr(std::move(aToMove)), mStr(std::move(aToMove.mStr)) {}
  ~nsStringHashKey() = default;

  KeyType GetKey() const { return mStr; }
  bool KeyEquals(const KeyTypePointer aKey) const { return mStr.Equals(*aKey); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(const KeyTypePointer aKey) {
    return mozilla::HashString(*aKey);
  }

#ifdef MOZILLA_INTERNAL_API
  // To avoid double-counting, only measure the string if it is unshared.
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return GetKey().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
#endif

  enum { ALLOW_MEMMOVE = true };

 private:
  nsString mStr;
};

#ifdef MOZILLA_INTERNAL_API

namespace mozilla::detail {

template <class CharT, bool Unicode = true>
struct comparatorTraits {};

template <>
struct comparatorTraits<char, false> {
  static int caseInsensitiveCompare(const char* aLhs, const char* aRhs,
                                    size_t aLhsLength, size_t aRhsLength) {
    return nsCaseInsensitiveCStringComparator(aLhs, aRhs, aLhsLength,
                                              aRhsLength);
  };
};

template <>
struct comparatorTraits<char, true> {
  static int caseInsensitiveCompare(const char* aLhs, const char* aRhs,
                                    size_t aLhsLength, size_t aRhsLength) {
    return nsCaseInsensitiveUTF8StringComparator(aLhs, aRhs, aLhsLength,
                                                 aRhsLength);
  };
};

template <>
struct comparatorTraits<char16_t, true> {
  static int caseInsensitiveCompare(const char16_t* aLhs, const char16_t* aRhs,
                                    size_t aLhsLength, size_t aRhsLength) {
    return nsCaseInsensitiveStringComparator(aLhs, aRhs, aLhsLength,
                                             aRhsLength);
  };
};

}  // namespace mozilla::detail

/**
 * This is internal-API only because nsCaseInsensitive{C}StringComparator is
 * internal-only.
 *
 * @see nsTHashtable::EntryType for specification
 */

template <typename T, bool Unicode>
class nsTStringCaseInsensitiveHashKey : public PLDHashEntryHdr {
 public:
  typedef const nsTSubstring<T>& KeyType;
  typedef const nsTSubstring<T>* KeyTypePointer;

  explicit nsTStringCaseInsensitiveHashKey(KeyTypePointer aStr) : mStr(*aStr) {
    // take it easy just deal HashKey
  }

  nsTStringCaseInsensitiveHashKey(const nsTStringCaseInsensitiveHashKey&) =
      delete;
  nsTStringCaseInsensitiveHashKey(nsTStringCaseInsensitiveHashKey&& aToMove)
      : PLDHashEntryHdr(std::move(aToMove)), mStr(std::move(aToMove.mStr)) {}
  ~nsTStringCaseInsensitiveHashKey() = default;

  KeyType GetKey() const { return mStr; }
  bool KeyEquals(const KeyTypePointer aKey) const {
    using comparator = typename mozilla::detail::comparatorTraits<T, Unicode>;
    return mStr.Equals(*aKey, comparator::caseInsensitiveCompare);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(const KeyTypePointer aKey) {
    nsTAutoString<T> tmKey(*aKey);
    ToLowerCase(tmKey);
    return mozilla::HashString(tmKey);
  }
  enum { ALLOW_MEMMOVE = true };

  // To avoid double-counting, only measure the string if it is unshared.
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return GetKey().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

 private:
  const nsTString<T> mStr;
};

using nsStringCaseInsensitiveHashKey =
    nsTStringCaseInsensitiveHashKey<char16_t, true>;
using nsCStringASCIICaseInsensitiveHashKey =
    nsTStringCaseInsensitiveHashKey<char, false>;
using nsCStringUTF8CaseInsensitiveHashKey =
    nsTStringCaseInsensitiveHashKey<char, true>;

#endif  // MOZILLA_INTERNAL_API

/**
 * hashkey wrapper using nsACString KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsCStringHashKey : public PLDHashEntryHdr {
 public:
  typedef const nsACString& KeyType;
  typedef const nsACString* KeyTypePointer;

  explicit nsCStringHashKey(const nsACString* aStr) : mStr(*aStr) {}
  nsCStringHashKey(nsCStringHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)), mStr(std::move(aOther.mStr)) {}
  ~nsCStringHashKey() = default;

  KeyType GetKey() const { return mStr; }
  bool KeyEquals(KeyTypePointer aKey) const { return mStr.Equals(*aKey); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashString(*aKey);
  }

#ifdef MOZILLA_INTERNAL_API
  // To avoid double-counting, only measure the string if it is unshared.
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return GetKey().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
#endif

  enum { ALLOW_MEMMOVE = true };

 private:
  const nsCString mStr;
};

/**
 * hashkey wrapper using integral or enum KeyTypes
 *
 * @see nsTHashtable::EntryType for specification
 */
template <typename T,
          std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>, int> = 0>
class nsIntegralHashKey : public PLDHashEntryHdr {
 public:
  using KeyType = const T&;
  using KeyTypePointer = const T*;

  explicit nsIntegralHashKey(KeyTypePointer aKey) : mValue(*aKey) {}
  nsIntegralHashKey(nsIntegralHashKey&& aOther) noexcept
      : PLDHashEntryHdr(std::move(aOther)), mValue(aOther.mValue) {}
  ~nsIntegralHashKey() = default;

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashGeneric(*aKey);
  }
  enum { ALLOW_MEMMOVE = true };

 private:
  const T mValue;
};

/**
 * hashkey wrapper using uint32_t KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
using nsUint32HashKey = nsIntegralHashKey<uint32_t>;

/**
 * hashkey wrapper using uint64_t KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
using nsUint64HashKey = nsIntegralHashKey<uint64_t>;

/**
 * hashkey wrapper using float KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsFloatHashKey : public PLDHashEntryHdr {
 public:
  typedef const float& KeyType;
  typedef const float* KeyTypePointer;

  explicit nsFloatHashKey(KeyTypePointer aKey) : mValue(*aKey) {}
  nsFloatHashKey(nsFloatHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)), mValue(std::move(aOther.mValue)) {}
  ~nsFloatHashKey() = default;

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return *reinterpret_cast<const uint32_t*>(aKey);
  }
  enum { ALLOW_MEMMOVE = true };

 private:
  const float mValue;
};

/**
 * hashkey wrapper using intptr_t KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
using IntPtrHashKey = nsIntegralHashKey<intptr_t>;

/**
 * hashkey wrapper using nsISupports* KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsISupportsHashKey : public PLDHashEntryHdr {
 public:
  using KeyType = nsISupports*;
  using KeyTypePointer = const nsISupports*;

  explicit nsISupportsHashKey(const nsISupports* aKey)
      : mSupports(const_cast<nsISupports*>(aKey)) {}
  nsISupportsHashKey(nsISupportsHashKey&& aOther) = default;
  ~nsISupportsHashKey() = default;

  KeyType GetKey() const { return mSupports; }
  bool KeyEquals(KeyTypePointer aKey) const { return aKey == mSupports; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashGeneric(aKey);
  }
  enum { ALLOW_MEMMOVE = true };

 private:
  nsCOMPtr<nsISupports> mSupports;
};

/**
 * hashkey wrapper using refcounted * KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
template <class T>
class nsRefPtrHashKey : public PLDHashEntryHdr {
 public:
  using KeyType = T*;
  using KeyTypePointer = const T*;

  explicit nsRefPtrHashKey(const T* aKey) : mKey(const_cast<T*>(aKey)) {}
  nsRefPtrHashKey(nsRefPtrHashKey&& aOther) = default;
  ~nsRefPtrHashKey() = default;

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return aKey == mKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashGeneric(aKey);
  }
  enum { ALLOW_MEMMOVE = true };

 private:
  RefPtr<T> mKey;
};

template <class T>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, nsRefPtrHashKey<T>& aField,
    const char* aName, uint32_t aFlags = 0) {
  CycleCollectionNoteChild(aCallback, aField.GetKey(), aName, aFlags);
}

/**
 * hashkey wrapper using a function pointer KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
template <class T>
class nsFuncPtrHashKey : public PLDHashEntryHdr {
 public:
  typedef T& KeyType;
  typedef const T* KeyTypePointer;

  explicit nsFuncPtrHashKey(const T* aKey) : mKey(*const_cast<T*>(aKey)) {}
  nsFuncPtrHashKey(const nsFuncPtrHashKey<T>& aToCopy) : mKey(aToCopy.mKey) {}
  ~nsFuncPtrHashKey() = default;

  KeyType GetKey() const { return const_cast<T&>(mKey); }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashGeneric(*aKey);
  }
  enum { ALLOW_MEMMOVE = true };

 protected:
  T mKey;
};

/**
 * hashkey wrapper using nsID KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsIDHashKey : public PLDHashEntryHdr {
 public:
  typedef const nsID& KeyType;
  typedef const nsID* KeyTypePointer;

  explicit nsIDHashKey(const nsID* aInID) : mID(*aInID) {}
  nsIDHashKey(nsIDHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)), mID(std::move(aOther.mID)) {}
  ~nsIDHashKey() = default;

  KeyType GetKey() const { return mID; }
  bool KeyEquals(KeyTypePointer aKey) const { return aKey->Equals(mID); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    // Hash the nsID object's raw bytes.
    return mozilla::HashBytes(aKey, sizeof(KeyType));
  }

  enum { ALLOW_MEMMOVE = true };

 private:
  nsID mID;
};

/**
 * hashkey wrapper using nsID* KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsIDPointerHashKey : public PLDHashEntryHdr {
 public:
  typedef const nsID* KeyType;
  typedef const nsID* KeyTypePointer;

  explicit nsIDPointerHashKey(const nsID* aInID) : mID(aInID) {}
  nsIDPointerHashKey(nsIDPointerHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)), mID(aOther.mID) {}
  ~nsIDPointerHashKey() = default;

  KeyType GetKey() const { return mID; }
  bool KeyEquals(KeyTypePointer aKey) const { return aKey->Equals(*mID); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    // Hash the nsID object's raw bytes.
    return mozilla::HashBytes(aKey, sizeof(*aKey));
  }

  enum { ALLOW_MEMMOVE = true };

 private:
  const nsID* mID;
};

/**
 * hashkey wrapper for "dependent" const char*; this class does not "own"
 * its string pointer.
 *
 * This class must only be used if the strings have a lifetime longer than
 * the hashtable they occupy. This normally occurs only for static
 * strings or strings that have been arena-allocated.
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsDepCharHashKey : public PLDHashEntryHdr {
 public:
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  explicit nsDepCharHashKey(const char* aKey) : mKey(aKey) {}
  nsDepCharHashKey(nsDepCharHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)), mKey(std::move(aOther.mKey)) {}
  ~nsDepCharHashKey() = default;

  const char* GetKey() const { return mKey; }
  bool KeyEquals(const char* aKey) const { return !strcmp(mKey, aKey); }

  static const char* KeyToPointer(const char* aKey) { return aKey; }
  static PLDHashNumber HashKey(const char* aKey) {
    return mozilla::HashString(aKey);
  }
  enum { ALLOW_MEMMOVE = true };

 private:
  const char* mKey;
};

/**
 * hashkey wrapper for const char*; at construction, this class duplicates
 * a string pointed to by the pointer so that it doesn't matter whether or not
 * the string lives longer than the hash table.
 */
class nsCharPtrHashKey : public PLDHashEntryHdr {
 public:
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  explicit nsCharPtrHashKey(const char* aKey) : mKey(strdup(aKey)) {}

  nsCharPtrHashKey(const nsCharPtrHashKey&) = delete;
  nsCharPtrHashKey(nsCharPtrHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)), mKey(aOther.mKey) {
    aOther.mKey = nullptr;
  }

  ~nsCharPtrHashKey() {
    if (mKey) {
      free(const_cast<char*>(mKey));
    }
  }

  const char* GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return !strcmp(mKey, aKey); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashString(aKey);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(mKey);
  }

  enum { ALLOW_MEMMOVE = true };

 private:
  const char* mKey;
};

/**
 * hashkey wrapper for const char16_t*; at construction, this class duplicates
 * a string pointed to by the pointer so that it doesn't matter whether or not
 * the string lives longer than the hash table.
 */
class nsUnicharPtrHashKey : public PLDHashEntryHdr {
 public:
  typedef const char16_t* KeyType;
  typedef const char16_t* KeyTypePointer;

  explicit nsUnicharPtrHashKey(const char16_t* aKey) : mKey(NS_xstrdup(aKey)) {}
  nsUnicharPtrHashKey(const nsUnicharPtrHashKey& aToCopy) = delete;
  nsUnicharPtrHashKey(nsUnicharPtrHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)), mKey(aOther.mKey) {
    aOther.mKey = nullptr;
  }

  ~nsUnicharPtrHashKey() {
    if (mKey) {
      free(const_cast<char16_t*>(mKey));
    }
  }

  const char16_t* GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return !NS_strcmp(mKey, aKey); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashString(aKey);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(mKey);
  }

  enum { ALLOW_MEMMOVE = true };

 private:
  const char16_t* mKey;
};

namespace mozilla {

template <typename T>
PLDHashNumber Hash(const T& aValue) {
  return aValue.Hash();
}

}  // namespace mozilla

/**
 * Hashtable key class to use with objects for which Hash() and operator==()
 * are defined.
 */
template <typename T>
class nsGenericHashKey : public PLDHashEntryHdr {
 public:
  typedef const T& KeyType;
  typedef const T* KeyTypePointer;

  explicit nsGenericHashKey(KeyTypePointer aKey) : mKey(*aKey) {}
  nsGenericHashKey(const nsGenericHashKey&) = delete;
  nsGenericHashKey(nsGenericHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)), mKey(std::move(aOther.mKey)) {}

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return ::mozilla::Hash(*aKey);
  }
  enum { ALLOW_MEMMOVE = true };

 private:
  T mKey;
};

#endif  // nsTHashKeys_h__
