/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5String_h
#define nsHtml5String_h

#include "nsString.h"

class nsHtml5TreeBuilder;

/**
 * A pass-by-value type that combines an unsafe `nsStringBuffer*` with its
 * logical length (`uint32_t`). (`nsStringBuffer` knows its capacity but not
 * its logical length, i.e. how much of the capacity is in use.)
 *
 * Holding or passing this type is as unsafe as holding or passing
 * `nsStringBuffer*`.
 *
 * Empty strings and null strings are distinct. Since an empty nsString does
 * not have a an `nsStringBuffer`, both empty and null `nsHtml5String` have
 * `nullptr` as `mBuffer`. If `mBuffer` is `nullptr`, the empty case is marked
 * with `mLength` being zero and the null case with `mLength` being non-zero.
 */
class nsHtml5String final
{
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
    : mBuffer(nullptr)
    , mLength(UINT32_MAX)
  {
  }

  inline uint32_t Length() const { return mBuffer ? mLength : 0; }

  /**
   * False iff the string is logically null
   */
  inline MOZ_IMPLICIT operator bool() const { return !(!mBuffer && mLength); }

  void ToString(nsAString& aString);

  void CopyToBuffer(char16_t* aBuffer);

  bool LowerCaseEqualsASCII(const char* aLowerCaseLiteral);

  bool EqualsASCII(const char* aLiteral);

  bool LowerCaseStartsWithASCII(const char* aLowerCaseLiteral);

  bool Equals(nsHtml5String aOther);

  nsHtml5String Clone();

  void Release();

  static nsHtml5String FromBuffer(char16_t* aBuffer,
                                  int32_t aLength,
                                  nsHtml5TreeBuilder* aTreeBuilder);

  static nsHtml5String FromLiteral(const char* aLiteral);

  static nsHtml5String FromString(const nsAString& aString);

  static nsHtml5String EmptyString();

private:
  /**
   * Constructor from raw parts.
   */
  nsHtml5String(already_AddRefed<nsStringBuffer> aBuffer, uint32_t aLength);

  /**
   * nullptr if the string is logically null or logically empty
   */
  nsStringBuffer* mBuffer;

  /**
   * The length of the string. non-zero if the string is logically null.
   */
  uint32_t mLength;
};

#endif // nsHtml5String_h
