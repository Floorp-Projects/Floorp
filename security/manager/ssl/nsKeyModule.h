/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsKeyModule_h
#define nsKeyModule_h

#include "ScopedNSSTypes.h"
#include "nsIKeyModule.h"
#include "pk11pub.h"

#define NS_KEYMODULEOBJECT_CID                       \
  {                                                  \
    0x9d383ddd, 0x6856, 0x4187, {                    \
      0x84, 0x85, 0xf3, 0x61, 0x95, 0xb2, 0x9a, 0x0e \
    }                                                \
  }
#define NS_KEYMODULEOBJECT_CONTRACTID "@mozilla.org/security/keyobject;1"

#define NS_KEYMODULEOBJECTFACTORY_CID                \
  {                                                  \
    0x2a35dd47, 0xb026, 0x4e8d, {                    \
      0xb6, 0xb7, 0x57, 0x40, 0xf6, 0x1a, 0xb9, 0x02 \
    }                                                \
  }
#define NS_KEYMODULEOBJECTFACTORY_CONTRACTID \
  "@mozilla.org/security/keyobjectfactory;1"

class nsKeyObject final : public nsIKeyObject {
 public:
  nsKeyObject();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIKEYOBJECT

 private:
  ~nsKeyObject() {}

  // Disallow copy constructor
  nsKeyObject(nsKeyObject&);

  mozilla::UniquePK11SymKey mSymKey;
};

class nsKeyObjectFactory final : public nsIKeyObjectFactory {
 public:
  nsKeyObjectFactory() {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIKEYOBJECTFACTORY

 private:
  ~nsKeyObjectFactory() {}

  // Disallow copy constructor
  nsKeyObjectFactory(nsKeyObjectFactory&);
};

#endif  // nsKeyModule_h
