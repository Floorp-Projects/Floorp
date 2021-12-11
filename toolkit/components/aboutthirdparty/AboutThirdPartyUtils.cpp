/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AboutThirdPartyUtils.h"

#include "nsUnicharUtils.h"

namespace mozilla {

int32_t CompareIgnoreCase(const nsAString& aStr1, const nsAString& aStr2) {
  uint32_t len1 = aStr1.Length();
  uint32_t len2 = aStr2.Length();
  uint32_t lenMin = XPCOM_MIN(len1, len2);

  int32_t result = nsCaseInsensitiveStringComparator(
      aStr1.BeginReading(), aStr2.BeginReading(), lenMin, lenMin);
  return result ? result : len1 - len2;
}

bool MsiPackGuid(const nsAString& aGuid, nsAString& aPacked) {
  if (aGuid.Length() != 38 || aGuid.First() != u'{' || aGuid.Last() != u'}') {
    return false;
  }

  constexpr int kPackedLength = 32;
  const uint8_t kIndexMapping[kPackedLength] = {
      // clang-format off
      0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
      0x0d, 0x0c, 0x0b, 0x0a, 0x12, 0x11, 0x10, 0x0f,
      0x15, 0x14, 0x17, 0x16, 0x1a, 0x19, 0x1c, 0x1b,
      0x1e, 0x1d, 0x20, 0x1f, 0x22, 0x21, 0x24, 0x23,
      // clang-format on
  };

  int index = 0;
  aPacked.SetLength(kPackedLength);
  for (auto iter = aPacked.BeginWriting(), strEnd = aPacked.EndWriting();
       iter != strEnd; ++iter, ++index) {
    *iter = aGuid[kIndexMapping[index]];
  }

  return true;
}

bool CorrectMsiComponentPath(nsAString& aPath) {
  if (aPath.Length() < 3 || !aPath.BeginReading()[0]) {
    return false;
  }

  char16_t* strBegin = aPath.BeginWriting();

  if (strBegin[1] == u'?') {
    strBegin[1] = strBegin[0] == u'\\' ? u'\\' : u':';
  }

  if (strBegin[1] != u':' || strBegin[2] != u'\\') {
    return false;
  }

  if (aPath.Length() > 3 && aPath.BeginReading()[3] == u'?') {
    aPath.ReplaceLiteral(3, 1, u"");
  }
  return true;
}

}  // namespace mozilla
