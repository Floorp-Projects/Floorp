/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _nsCrypto_h_
#define _nsCrypto_h_

#include "nsIPKCS11.h"

#define NS_PKCS11_CID \
  {0x74b7a390, 0x3b41, 0x11d4, { 0x8a, 0x80, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3} }

class nsPkcs11 : public nsIPKCS11
{
public:
  nsPkcs11();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11

protected:
  virtual ~nsPkcs11();
};

#endif //_nsCrypto_h_
