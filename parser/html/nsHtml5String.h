/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5String_h
#define nsHtml5String_h

#include "nsAtom.h"
#include "nsString.h"
#include "nsStringBuffer.h"

class nsHtml5TreeBuilder;

/**
 * A pass-by-value type that can represent
 *  * nullptr
 *  * empty string
 *  * Non-empty string as exactly-sized (capacity is length) `nsStringBuffer*`
 *  * Non-empty string as an nsAtom*
 *
 * Holding or passing this type is as unsafe as holding or passing
 * `nsStringBuffer*`/`nsAtom*`.
 */
class nsHtml5String final
{
private:
  static const uintptr_t kKindMask = uintptr_t(3);

  static const uintptr_t kPtrMask = ~kKindMask;

  enum Kind : uintptr_t
  {
    eNull = 0,
    eEmpty = 1,
    eStringBuffer = 2,
    eAtom = 3,
  };

  inline Kind GetKind() const { return (Kind)(mBits & kKindMask); }

  inline nsStringBuffer* AsStringBuffer() const
  {
    MOZ_ASSERT(GetKind() == eStringBuffer);
    return reinterpret_cast<nsStringBuffer*>(mBits & kPtrMask);
  }

  inline nsAtom* AsAtom() const
  {
    MOZ_ASSERT(GetKind() == eAtom);
    return reinterpret_cast<nsAtom*>(mBits & kPtrMask);
  }

  inline const char16_t* AsPtr() const
  {
    switch (GetKind()) {
      case eStringBuffer:
        return reinterpret_cast<char16_t*>(AsStringBuffer()->Data());
      case eAtom:
        return AsAtom()->GetUTF16String();
      default:
        return nullptr;
    }
  }

public:
  /**
   * Default constructor.
   */
  inline nsHtml5String()
    : nsHtml5String(nullptr)
  {
  }

  /**
   * Constructor from nullptr.
   */
  inline MOZ_IMPLICIT nsHtml5String(decltype(nullptr))
    : mBits(eNull)
  {
  }

  inline uint32_t Length() const
  {
    switch (GetKind()) {
      case eStringBuffer:
        return (AsStringBuffer()->StorageSize() / sizeof(char16_t) - 1);
      case eAtom:
        return AsAtom()->GetLength();
      default:
        return 0;
    }
  }

  /**
   * False iff the string is logically null
   */
  inline MOZ_IMPLICIT operator bool() const { return mBits; }

  /**
   * Get the underlying nsAtom* or nullptr if this nsHtml5String
   * does not hold an atom.
   */
  inline nsAtom* MaybeAsAtom()
  {
    if (GetKind() == eAtom) {
      return AsAtom();
    }
    return nullptr;
  }

  void ToString(nsAString& aString);

  void CopyToBuffer(char16_t* aBuffer) const;

  bool LowerCaseEqualsASCII(const char* aLowerCaseLiteral) const;

  bool EqualsASCII(const char* aLiteral) const;

  bool LowerCaseStartsWithASCII(const char* aLowerCaseLiteral) const;

  bool Equals(nsHtml5String aOther) const;

  nsHtml5String Clone();

  void Release();

  static nsHtml5String FromBuffer(char16_t* aBuffer,
                                  int32_t aLength,
                                  nsHtml5TreeBuilder* aTreeBuilder);

  static nsHtml5String FromLiteral(const char* aLiteral);

  static nsHtml5String FromString(const nsAString& aString);

  static nsHtml5String FromAtom(already_AddRefed<nsAtom> aAtom);

  static nsHtml5String EmptyString();

private:
  /**
   * Constructor from raw bits.
   */
  explicit nsHtml5String(uintptr_t aBits)
    : mBits(aBits){};

  /**
   * Zero if null, one if empty, otherwise tagged pointer
   * to either nsAtom or nsStringBuffer. The two least-significant
   * bits are tag bits.
   */
  uintptr_t mBits;
};

#endif // nsHtml5String_h
