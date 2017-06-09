/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PKCS11_h
#define PKCS11_h

#include "nsIPKCS11.h"

#include "nsNSSShutDown.h"
#include "nsString.h"

namespace mozilla { namespace psm {

#define NS_PKCS11_CID \
  {0x74b7a390, 0x3b41, 0x11d4, { 0x8a, 0x80, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3} }

class PKCS11 : public nsIPKCS11
             , public nsNSSShutDownObject
{
public:
  PKCS11();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11

protected:
  virtual ~PKCS11();

private:
  virtual void virtualDestroyNSSReference() override {}
};

void GetModuleNameForTelemetry(/*in*/ const SECMODModule* module,
                               /*out*/nsString& result);

} } // namespace mozilla::psm

#endif // PKCS11_h
