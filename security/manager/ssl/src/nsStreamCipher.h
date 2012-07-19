/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_STREAMCIPHER_H_
#define _NS_STREAMCIPHER_H_

#include "nsIStreamCipher.h"
#include "nsString.h"
#include "pk11func.h"
#include "mozilla/Attributes.h"

#define NS_STREAMCIPHER_CLASSNAME  "Stream Cipher Component"
/* dbfcbe4a-10f7-4d6f-a481-68e6d6b71d21 */
#define NS_STREAMCIPHER_CID   \
{ 0xdbfcbe4a, 0x10f7, 0x4d6f, {0xa4, 0x81, 0x68, 0xe6, 0xd6, 0xb7, 0x1d, 0x21}}
#define NS_STREAMCIPHER_CONTRACTID "@mozilla.org/security/streamcipher;1"

class nsStreamCipher MOZ_FINAL : public nsIStreamCipher
{
public:
  nsStreamCipher();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMCIPHER

private:
  ~nsStreamCipher();

  // Helper method for initializing this object.
  // aIV may be null.
  nsresult InitWithIV_(nsIKeyObject *aKey, SECItem* aIV);
  
  // Disallow copy constructor
  nsStreamCipher(nsStreamCipher&);

  // Holds our stream cipher context.
  PK11Context* mContext;

  // Holds the amount we've computed so far.
  nsCString mValue;
};

#endif // _NS_STREAMCIPHER_H_
