/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAtom_h
#define nsAtom_h

#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "mozilla/HashFunctions.h"

namespace mozilla {
struct AtomsSizes;
}

class nsStaticAtom;
class nsDynamicAtom;

// This class encompasses both static and dynamic atoms.
//
// - In places where static and dynamic atoms can be used, use RefPtr<nsAtom>.
//   This is by far the most common case. (The exception to this is the HTML5
//   parser, which does its own weird thing, and uses non-refcounted dynamic
//   atoms.)
//
// - In places where only static atoms can appear, use nsStaticAtom* to avoid
//   unnecessary refcounting. This is a moderately common case.
//
// - In places where only dynamic atoms can appear, it doesn't matter much
//   whether you use RefPtr<nsAtom> or RefPtr<nsDynamicAtom>. This is an
//   extremely rare case.
//
class nsAtom
{
public:
  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              mozilla::AtomsSizes& aSizes) const;

  // Dynamic HTML5 atoms are just like vanilla dynamic atoms, but we disallow
  // various operations, the most important of which is AddRef/Release.
  // XXX: we'd like to get rid of dynamic HTML5 atoms. See bug 1392185 for
  // details.
  enum class AtomKind : uint8_t {
    Static = 0,
    DynamicNormal = 1,
    DynamicHTML5 = 2,
  };

  bool Equals(char16ptr_t aString, uint32_t aLength) const
  {
    return mLength == aLength &&
           memcmp(GetUTF16String(), aString, mLength * sizeof(char16_t)) == 0;
  }

  bool Equals(const nsAString& aString) const
  {
    return Equals(aString.BeginReading(), aString.Length());
  }

  AtomKind Kind() const { return static_cast<AtomKind>(mKind); }

  bool IsStatic() const { return Kind() == AtomKind::Static; }
  bool IsDynamic() const
  {
    return Kind() == AtomKind::DynamicNormal ||
           Kind() == AtomKind::DynamicHTML5;
  }
  bool IsDynamicHTML5() const
  {
    return Kind() == AtomKind::DynamicHTML5;
  }

  const nsStaticAtom* AsStatic() const;
  const nsDynamicAtom* AsDynamic() const;
  nsDynamicAtom* AsDynamic();

  char16ptr_t GetUTF16String() const;

  uint32_t GetLength() const { return mLength; }

  void ToString(nsAString& aString) const;
  void ToUTF8String(nsACString& aString) const;

  // A hashcode that is better distributed than the actual atom pointer, for
  // use in situations that need a well-distributed hashcode. It's called hash()
  // rather than Hash() so we can use mozilla::BloomFilter<N, nsAtom>, because
  // BloomFilter requires elements to implement a function called hash().
  //
  uint32_t hash() const
  {
    MOZ_ASSERT(!IsDynamicHTML5());
    return mHash;
  }

  // We can't use NS_INLINE_DECL_THREADSAFE_REFCOUNTING because the refcounting
  // of this type is special.
  MozExternalRefCountType AddRef();
  MozExternalRefCountType Release();

  typedef mozilla::TrueType HasThreadSafeRefCnt;

protected:
  // Used by nsStaticAtom.
  constexpr nsAtom(const char16_t* aStr, uint32_t aLength)
    : mLength(aLength)
    , mKind(static_cast<uint32_t>(nsAtom::AtomKind::Static))
    , mHash(mozilla::HashString(aStr))
  {}

  // Used by nsDynamicAtom.
  nsAtom(AtomKind aKind, const nsAString& aString, uint32_t aHash)
    : mLength(aString.Length())
    , mKind(static_cast<uint32_t>(aKind))
    , mHash(aHash)
  {
    MOZ_ASSERT(aKind == AtomKind::DynamicNormal ||
               aKind == AtomKind::DynamicHTML5);
  }

  ~nsAtom() = default;

  const uint32_t mLength:30;
  const uint32_t mKind:2; // nsAtom::AtomKind
  const uint32_t mHash;
};

// This class would be |final| if it wasn't for nsICSSAnonBoxPseudo and
// nsICSSPseudoElement, which are trivial subclasses used to ensure only
// certain static atoms are passed to certain functions.
class nsStaticAtom : public nsAtom
{
public:
  // These are deleted so it's impossible to RefPtr<nsStaticAtom>. Raw
  // nsStaticAtom pointers should be used instead.
  MozExternalRefCountType AddRef() = delete;
  MozExternalRefCountType Release() = delete;

  constexpr nsStaticAtom(const char16_t* aStr, uint32_t aLength,
                         uint32_t aStringOffset)
    : nsAtom(aStr, aLength)
    , mStringOffset(aStringOffset)
  {}

  const char16_t* String() const
  {
    return reinterpret_cast<const char16_t*>(uintptr_t(this) - mStringOffset);
  }

  already_AddRefed<nsAtom> ToAddRefed() {
    return already_AddRefed<nsAtom>(static_cast<nsAtom*>(this));
  }

private:
  // This is an offset to the string chars, which must be at a lower address in
  // memory. This should be achieved by using the macros in nsStaticAtom.h.
  uint32_t mStringOffset;
};

class nsDynamicAtom : public nsAtom
{
public:
  // We can't use NS_INLINE_DECL_THREADSAFE_REFCOUNTING because the refcounting
  // of this type is special.
  MozExternalRefCountType AddRef();
  MozExternalRefCountType Release();

  ~nsDynamicAtom();

  const char16_t* String() const { return mString; }

  // The caller must *not* mutate the string buffer, otherwise all hell will
  // break loose.
  nsStringBuffer* GetStringBuffer() const
  {
    // See the comment on |mString|'s declaration.
    MOZ_ASSERT(IsDynamic());
    return nsStringBuffer::FromData(const_cast<char16_t*>(mString));
  }

private:
  friend class nsAtomTable;
  friend class nsAtomSubTable;
  // XXX: we'd like to remove nsHtml5AtomEntry. See bug 1392185.
  friend class nsHtml5AtomEntry;

  // Construction is done by |friend|s.
  // The first constructor is for dynamic normal atoms, the second is for
  // dynamic HTML5 atoms.
  nsDynamicAtom(const nsAString& aString, uint32_t aHash);
  explicit nsDynamicAtom(const nsAString& aString);

  mozilla::ThreadSafeAutoRefCnt mRefCnt;
  // Note: this points to the chars in an nsStringBuffer, which is obtained
  // with nsStringBuffer::FromData(mString).
  const char16_t* const mString;
};

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

// Record that all static atoms have been inserted.
void NS_SetStaticAtomsDone();

class nsAtomString : public nsString
{
public:
  explicit nsAtomString(const nsAtom* aAtom) { aAtom->ToString(*this); }
};

class nsAtomCString : public nsCString
{
public:
  explicit nsAtomCString(nsAtom* aAtom) { aAtom->ToUTF8String(*this); }
};

class nsDependentAtomString : public nsDependentString
{
public:
  explicit nsDependentAtomString(const nsAtom* aAtom)
    : nsDependentString(aAtom->GetUTF16String(), aAtom->GetLength())
  {}
};

// Checks if the ascii chars in a given atom are already lowercase.
// If they are, no-op. Otherwise, converts all the ascii uppercase
// chars to lowercase and atomizes, storing the result in the inout
// param.
void ToLowerCaseASCII(RefPtr<nsAtom>& aAtom);

#endif  // nsAtom_h
