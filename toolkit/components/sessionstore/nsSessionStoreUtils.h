/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSessionStoreUtils_h
#define nsSessionStoreUtils_h

#include "nsCycleCollectionParticipant.h"
#include "nsISessionStoreUtils.h"
#include "nsIDOMEventListener.h"
#include "nsCOMPtr.h"

#define NS_SESSIONSTOREUTILS_CID \
  {0xd713b4be, 0x8285, 0x4cab, {0x9c, 0x0e, 0x0b, 0xbc, 0x38, 0xbf, 0xb9, 0x3c}}

#define NS_SESSIONSTOREUTILS_CONTRACTID \
  "@mozilla.org/browser/sessionstore/utils;1"

class nsSessionStoreUtils final : public nsISessionStoreUtils
{
public:
  NS_DECL_NSISESSIONSTOREUTILS
  NS_DECL_ISUPPORTS

private:
  ~nsSessionStoreUtils() { }
};

#endif // nsSessionStoreUtils_h
