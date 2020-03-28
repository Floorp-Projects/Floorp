/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAtom_h
#define nsAtom_h

#include <type_traits>

#include "nsISupportsImpl.h"
#include "nsString.h"
#include "mozilla/Atomics.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
struct AtomsSizes;
}

class nsStaticAtom;
class nsDynamicAtom;

// This class encompasses both static and dynamic atoms.
//
// - In places where static and dynamic atoms can be used, use RefPtr<nsAtom>.
//   This is by far the most common case.
//
// - In places where only static atoms can appear, use nsStaticAtom* to avoid
//   unnecessary refcounting. This is a moderately common case.
//
// - In places where only dynamic atoms can appear, it doesn't matter much
//   whether you use RefPtr<nsAtom> or RefPtr<nsDynamicAtom>. This is an
//   extremely rare case.
//
class nsAtom {
 public:
  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              mozilla::AtomsSizes& aSizes) const;

  bool Equals(char16ptr_t aString, uint32_t aLength) const {
    return mLength == aLength &&
           memcmp(GetUTF16String(), aString, mLength * sizeof(char16_t)) == 0;
  }

  bool Equals(const nsAString& aString) const {
    return Equals(aString.BeginReading(), aString.Length());
  }

  bool IsStatic() const { return mIsStatic; }
  bool IsDynamic() const { return !IsStatic(); }

  inline const nsStaticAtom* AsStatic() const;
  inline const nsDynamicAtom* AsDynamic() const;
  inline nsDynamicAtom* AsDynamic();

  char16ptr_t GetUTF16String() const;

  uint32_t GetLength() const { return mLength; }

  operator mozilla::Span<const char16_t>() const {
    return mozilla::MakeSpan(static_cast<const char16_t*>(GetUTF16String()),
                             GetLength());
  }

  void ToString(nsAString& aString) const;
  void ToUTF8String(nsACString& aString) const;

  // A hashcode that is better distributed than the actual atom pointer, for
  // use in situations that need a well-distributed hashcode. It's called hash()
  // rather than Hash() so we can use mozilla::BloomFilter<N, nsAtom>, because
  // BloomFilter requires elements to implement a function called hash().
  //
  uint32_t hash() const { return mHash; }

  // This function returns true if ToLowercaseASCII would return the string
  // unchanged.
  bool IsAsciiLowercase() const { return mIsAsciiLowercase; }

  // This function returns true if this is the empty atom. This is exactly
  // equivalent to `this == nsGkAtoms::_empty`, but it's a bit less foot-gunny,
  // since we also have `nsGkAtoms::empty`.
  //
  // Defined in nsGkAtoms.h
  inline bool IsEmpty() const;

  // We can't use NS_INLINE_DECL_THREADSAFE_REFCOUNTING because the refcounting
  // of this type is special.
  inline MozExternalRefCountType AddRef();
  inline MozExternalRefCountType Release();

  using HasThreadSafeRefCnt = std::true_type;

 protected:
  // Used by nsStaticAtom.
  constexpr nsAtom(uint32_t aLength, uint32_t aHash, bool aIsAsciiLowercase)
      : mLength(aLength),
        mIsStatic(true),
        mIsAsciiLowercase(aIsAsciiLowercase),
        mHash(aHash) {}

  // Used by nsDynamicAtom.
  nsAtom(const nsAString& aString, uint32_t aHash, bool aIsAsciiLowercase)
      : mLength(aString.Length()),
        mIsStatic(false),
        mIsAsciiLowercase(aIsAsciiLowercase),
        mHash(aHash) {}

  ~nsAtom() = default;

  const uint32_t mLength : 30;
  const uint32_t mIsStatic : 1;
  const uint32_t mIsAsciiLowercase : 1;
  const uint32_t mHash;
};

// This class would be |final| if it wasn't for nsCSSAnonBoxPseudoStaticAtom
// and nsCSSPseudoElementStaticAtom, which are trivial subclasses used to
// ensure only certain static atoms are passed to certain functions.
class nsStaticAtom : public nsAtom {
 public:
  // These are deleted so it's impossible to RefPtr<nsStaticAtom>. Raw
  // nsStaticAtom pointers should be used instead.
  MozExternalRefCountType AddRef() = delete;
  MozExternalRefCountType Release() = delete;

  // The static atom's precomputed hash value is an argument here, but it
  // must be the same as would be computed by mozilla::HashString(aStr),
  // which is what we use when atomizing strings. We compute this hash in
  // Atom.py and assert in nsAtomTable::RegisterStaticAtoms that the two
  // hashes match.
  constexpr nsStaticAtom(uint32_t aLength, uint32_t aHash,
                         uint32_t aStringOffset, bool aIsAsciiLowercase)
      : nsAtom(aLength, aHash, aIsAsciiLowercase),
        mStringOffset(aStringOffset) {}

  const char16_t* String() const {
    return reinterpret_cast<const char16_t*>(uintptr_t(this) - mStringOffset);
  }

  already_AddRefed<nsAtom> ToAddRefed() {
    return already_AddRefed<nsAtom>(static_cast<nsAtom*>(this));
  }

 private:
  // This is an offset to the string chars, which must be at a lower address in
  // memory.
  uint32_t mStringOffset;
};

class nsDynamicAtom : public nsAtom {
 public:
  // We can't use NS_INLINE_DECL_THREADSAFE_REFCOUNTING because the refcounting
  // of this type is special.
  MozExternalRefCountType AddRef() {
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
    nsrefcnt count = ++mRefCnt;
    if (count == 1) {
      gUnusedAtomCount--;
    }
    return count;
  }

  MozExternalRefCountType Release() {
#ifdef DEBUG
    // We set a lower GC threshold for atoms in debug builds so that we exercise
    // the GC machinery more often.
    static const int32_t kAtomGCThreshold = 20;
#else
    static const int32_t kAtomGCThreshold = 10000;
#endif

    MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
    nsrefcnt count = --mRefCnt;
    if (count == 0) {
      if (++gUnusedAtomCount >= kAtomGCThreshold) {
        GCAtomTable();
      }
    }

    return count;
  }

  const char16_t* String() const {
    return reinterpret_cast<const char16_t*>(this + 1);
  }

  static nsDynamicAtom* FromChars(char16_t* chars) {
    return reinterpret_cast<nsDynamicAtom*>(chars) - 1;
  }

 private:
  friend class nsAtomTable;
  friend class nsAtomSubTable;
  friend int32_t NS_GetUnusedAtomCount();

  static mozilla::Atomic<int32_t, mozilla::ReleaseAcquire> gUnusedAtomCount;
  static void GCAtomTable();

  // These shouldn't be used directly, even by friend classes. The
  // Create()/Destroy() methods use them.
  nsDynamicAtom(const nsAString& aString, uint32_t aHash,
                bool aIsAsciiLowercase);
  ~nsDynamicAtom() = default;

  static nsDynamicAtom* Create(const nsAString& aString, uint32_t aHash);
  static void Destroy(nsDynamicAtom* aAtom);

  mozilla::ThreadSafeAutoRefCnt mRefCnt;

  // The atom's chars are stored at the end of the struct.
};

const nsStaticAtom* nsAtom::AsStatic() const {
  MOZ_ASSERT(IsStatic());
  return static_cast<const nsStaticAtom*>(this);
}

const nsDynamicAtom* nsAtom::AsDynamic() const {
  MOZ_ASSERT(IsDynamic());
  return static_cast<const nsDynamicAtom*>(this);
}

nsDynamicAtom* nsAtom::AsDynamic() {
  MOZ_ASSERT(IsDynamic());
  return static_cast<nsDynamicAtom*>(this);
}

MozExternalRefCountType nsAtom::AddRef() {
  return IsStatic() ? 2 : AsDynamic()->AddRef();
}

MozExternalRefCountType nsAtom::Release() {
  return IsStatic() ? 1 : AsDynamic()->Release();
}

// The four forms of NS_Atomize (for use with |RefPtr<nsAtom>|) return the
// atom for the string given. At any given time there will always be one atom
// representing a given string. Atoms are intended to make string comparison
// cheaper by simplifying it to pointer equality. A pointer to the atom that
// does not own a reference is not guaranteed to be valid.

// Find an atom that matches the given UTF-8 string. The string is assumed to
// be zero terminated. Never returns null.
already_AddRefed<nsAtom> NS_Atomize(const char* aUTF8String);

// Find an atom that matches the given UTF-8 string. Never returns null.
already_AddRefed<nsAtom> NS_Atomize(const nsACString& aUTF8String);

// Find an atom that matches the given UTF-16 string. The string is assumed to
// be zero terminated. Never returns null.
already_AddRefed<nsAtom> NS_Atomize(const char16_t* aUTF16String);

// Find an atom that matches the given UTF-16 string. Never returns null.
already_AddRefed<nsAtom> NS_Atomize(const nsAString& aUTF16String);

// An optimized version of the method above for the main thread.
already_AddRefed<nsAtom> NS_AtomizeMainThread(const nsAString& aUTF16String);

// Return a count of the total number of atoms currently alive in the system.
//
// Note that the result is imprecise and racy if other threads are currently
// operating on atoms. It's also slow, since it triggers a GC before counting.
// Currently this function is only used in tests, which should probably remain
// the case.
nsrefcnt NS_GetNumberOfAtoms();

// Return a pointer for a static atom for the string or null if there's no
// static atom for this string.
nsStaticAtom* NS_GetStaticAtom(const nsAString& aUTF16String);

class nsAtomString : public nsString {
 public:
  explicit nsAtomString(const nsAtom* aAtom) { aAtom->ToString(*this); }
};

class nsAtomCString : public nsCString {
 public:
  explicit nsAtomCString(const nsAtom* aAtom) { aAtom->ToUTF8String(*this); }
};

class nsDependentAtomString : public nsDependentString {
 public:
  explicit nsDependentAtomString(const nsAtom* aAtom)
      : nsDependentString(aAtom->GetUTF16String(), aAtom->GetLength()) {}
};

// Checks if the ascii chars in a given atom are already lowercase.
// If they are, no-op. Otherwise, converts all the ascii uppercase
// chars to lowercase and atomizes, storing the result in the inout
// param.
void ToLowerCaseASCII(RefPtr<nsAtom>& aAtom);

#endif  // nsAtom_h
