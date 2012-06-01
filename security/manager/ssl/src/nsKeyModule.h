/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_KEYMODULE_H_
#define _NS_KEYMODULE_H_

#include "nsIKeyModule.h"
#include "pk11pub.h"

#define NS_KEYMODULEOBJECT_CLASSNAME "Key Object Component"
/* eae599aa-ecef-49c6-a8af-6ddcc6feb484 */
#define NS_KEYMODULEOBJECT_CID   \
{ 0xeae599aa, 0xecef, 0x49c6, {0xa8, 0xaf, 0x6d, 0xdc, 0xc6, 0xfe, 0xb4, 0x84} }
#define NS_KEYMODULEOBJECT_CONTRACTID "@mozilla.org/security/keyobject;1"

#define NS_KEYMODULEOBJECTFACTORY_CLASSNAME "Key Object Factory Component"
/* a39e0e9d-e567-41e3-b12c-5df67f18174d */
#define NS_KEYMODULEOBJECTFACTORY_CID   \
{ 0xa39e0e9d, 0xe567, 0x41e3, {0xb1, 0x2c, 0x5d, 0xf6, 0x7f, 0x18, 0x17, 0x4d} }
#define NS_KEYMODULEOBJECTFACTORY_CONTRACTID \
"@mozilla.org/security/keyobjectfactory;1"

class nsKeyObject : public nsIKeyObject
{
public:
  nsKeyObject();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIKEYOBJECT

private:
  ~nsKeyObject();
  
  // Disallow copy constructor
  nsKeyObject(nsKeyObject&);

  // 0 if not yet set, otherwise one of the nsIKeyObject::*KEY values
  PRUint32 mKeyType;
  
  // A union of our possible key types
  PK11SymKey* mSymKey;
  SECKEYPrivateKey* mPrivateKey;
  SECKEYPublicKey* mPublicKey;

  // Helper method to free memory used by keys.
  void CleanUp();
};


class nsKeyObjectFactory : public nsIKeyObjectFactory
{
public:
  nsKeyObjectFactory();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIKEYOBJECTFACTORY

private:
  ~nsKeyObjectFactory() {}

  // Disallow copy constructor
  nsKeyObjectFactory(nsKeyObjectFactory&);
};

#endif // _NS_KEYMODULE_H_
