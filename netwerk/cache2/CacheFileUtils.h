/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileUtils__h__
#define CacheFileUtils__h__

#include "nsError.h"

class nsILoadContextInfo;
class nsACString;

namespace mozilla {
namespace net {
namespace CacheFileUtils {

nsresult ParseKey(const nsACString &aKey,
                  nsILoadContextInfo **aInfo,
                  nsACString *aURL);

void CreateKeyPrefix(nsILoadContextInfo* aInfo, nsACString &_retval);

} // CacheFileUtils
} // net
} // mozilla

#endif
