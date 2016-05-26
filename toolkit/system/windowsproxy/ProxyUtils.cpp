/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyUtils.h"

namespace mozilla {
namespace toolkit {
namespace system {

bool IsHostProxyEntry(const nsACString& aHost, const nsACString& aOverride)
{
  nsAutoCString host(aHost);
  nsAutoCString override(aOverride);
  int32_t overrideLength = override.Length();
  int32_t tokenStart = 0;
  int32_t offset = 0;
  bool star = false;

  while (tokenStart < overrideLength) {
    int32_t tokenEnd = override.FindChar('*', tokenStart);
    if (tokenEnd == tokenStart) {
      star = true;
      tokenStart++;
      // If the character following the '*' is a '.' character then skip
      // it so that "*.foo.com" allows "foo.com".
      if (override.FindChar('.', tokenStart) == tokenStart) {
        tokenStart++;
      }
    } else {
      if (tokenEnd == -1) {
        tokenEnd = overrideLength;
      }
      nsAutoCString token(Substring(override, tokenStart,
                                    tokenEnd - tokenStart));
      offset = host.Find(token, offset);
      if (offset == -1 || (!star && offset)) {
        return false;
      }
      star = false;
      tokenStart = tokenEnd;
      offset += token.Length();
    }
  }

  return (star || (offset == host.Length()));
}

} // namespace system
} // namespace toolkit
} // namespace mozilla
