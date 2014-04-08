/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileUtils__h__
#define CacheFileUtils__h__

#include "nsError.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsILoadContextInfo;
class nsACString;

namespace mozilla {
namespace net {
namespace CacheFileUtils {

already_AddRefed<nsILoadContextInfo>
ParseKey(const nsCSubstring &aKey,
         nsCSubstring *aIdEnhance = nullptr,
         nsCSubstring *aURISpec = nullptr);

void
AppendKeyPrefix(nsILoadContextInfo *aInfo, nsACString &_retval);

void
AppendTagWithValue(nsACString & aTarget, char const aTag, nsCSubstring const & aValue);

} // CacheFileUtils
} // net
} // mozilla

#endif
