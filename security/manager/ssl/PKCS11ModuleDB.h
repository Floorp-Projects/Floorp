/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PKCS11ModuleDB_h
#define PKCS11ModuleDB_h

#include "nsIPKCS11ModuleDB.h"

namespace mozilla {
namespace psm {

#define NS_PKCS11MODULEDB_CID                        \
  {                                                  \
    0xff9fbcd7, 0x9517, 0x4334, {                    \
      0xb9, 0x7a, 0xce, 0xed, 0x78, 0x90, 0x99, 0x74 \
    }                                                \
  }

class PKCS11ModuleDB : public nsIPKCS11ModuleDB {
 public:
  PKCS11ModuleDB() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11MODULEDB

 protected:
  virtual ~PKCS11ModuleDB() {}
};

}  // namespace psm
}  // namespace mozilla

#endif  // PKCS11ModuleDB_h
