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

// This class would be |final| if it wasn't for nsICSSAnonBoxPseudo and
// nsICSSPseudoElement, which are trivial subclasses used to ensure only
// certain atoms are passed to certain functions.
class nsAtom
{
public:
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  enum class AtomKind : uint8_t {
    DynamicAtom = 0,
    StaticAtom = 1,
    HTML5Atom = 2,
  };

  bool Equals(char16ptr_t aString, uint32_t aLength) const
  {
    return mLength == aLength &&
           memcmp(mString, aString, mLength * sizeof(char16_t)) == 0;
  }

  bool Equals(const nsAString& aString) const
  {
    return Equals(aString.BeginReading(), aString.Length());
  }

  void SetKind(AtomKind aKind)
  {
    mKind = static_cast<uint32_t>(aKind);
    MOZ_ASSERT(Kind() == aKind);
  }

  AtomKind Kind() const { return static_cast<AtomKind>(mKind); }

  bool IsDynamicAtom() const { return Kind() == AtomKind::DynamicAtom; }
  bool IsHTML5Atom()   const { return Kind() == AtomKind::HTML5Atom; }
  bool IsStaticAtom()  const { return Kind() == AtomKind::StaticAtom; }

  char16ptr_t GetUTF16String() const { return mString; }

  uint32_t GetLength() const { return mLength; }

  void ToString(nsAString& aBuf) const
  {
    // See the comment on |mString|'s declaration.
    nsStringBuffer::FromData(mString)->ToString(mLength, aBuf);
  }

  void ToUTF8String(nsACString& aString) const;

  nsStringBuffer* GetStringBuffer() const
  {
    // See the comment on |mString|'s declaration.
    return nsStringBuffer::FromData(mString);
  }

  // A hashcode that is better distributed than the actual atom pointer, for
  // use in situations that need a well-distributed hashcode.
  uint32_t hash() const
  {
    MOZ_ASSERT(!IsHTML5Atom());
    return mHash;
  }

  // We can't use NS_INLINE_DECL_THREADSAFE_REFCOUNTING because the refcounting
  // of this type is special.
  MozExternalRefCountType AddRef();
  MozExternalRefCountType Release();

  typedef mozilla::TrueType HasThreadSafeRefCnt;

private:
  friend class nsAtomFriend;
  friend class nsHtml5AtomEntry;

  // Construction and destruction is done entirely by |friend|s.
  nsAtom(AtomKind aKind, const nsAString& aString, uint32_t aHash);
  nsAtom(nsStringBuffer* aStringBuffer, uint32_t aLength, uint32_t aHash);
protected:
  ~nsAtom();

private:
  mozilla::ThreadSafeAutoRefCnt mRefCnt;
  uint32_t mLength: 30;
  uint32_t mKind: 2; // nsAtom::AtomKind
  uint32_t mHash;
  // WARNING! There is an invisible constraint on |mString|: the chars it
  // points to must belong to an nsStringBuffer. This is so that the
  // nsStringBuffer::FromData() calls above are valid.
  char16_t* mString;
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
nsrefcnt NS_GetNumberOfAtoms();

// Return a pointer for a static atom for the string or null if there's no
// static atom for this string.
nsAtom* NS_GetStaticAtom(const nsAString& aUTF16String);

// Seal the static atom table.
void NS_SealStaticAtomTable();

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

#endif  // nsAtom_h
