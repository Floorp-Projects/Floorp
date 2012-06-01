/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _nsCrypto_h_
#define _nsCrypto_h_
#include "nsCOMPtr.h"
#include "nsIDOMCRMFObject.h"
#include "nsIDOMCrypto.h"
#include "nsIPKCS11.h"
#include "nsIRunnable.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIPrincipal.h"

#define NS_CRYPTO_CLASSNAME "Crypto JavaScript Class"
#define NS_CRYPTO_CID \
  {0x929d9320, 0x251e, 0x11d4, { 0x8a, 0x7c, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3} }

#define NS_PKCS11_CLASSNAME "Pkcs11 JavaScript Class"
#define NS_PKCS11_CID \
  {0x74b7a390, 0x3b41, 0x11d4, { 0x8a, 0x80, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3} }

#define PSM_VERSION_STRING "2.4"

class nsIPSMComponent;
class nsIDOMScriptObjectFactory;


class nsCRMFObject : public nsIDOMCRMFObject
{
public:
  nsCRMFObject();
  virtual ~nsCRMFObject();

  NS_DECL_NSIDOMCRMFOBJECT
  NS_DECL_ISUPPORTS

  nsresult init();

  nsresult SetCRMFRequest(char *inRequest);
private:

  nsString mBase64Request;
};


class nsCrypto: public nsIDOMCrypto
{
public:
  nsCrypto();
  virtual ~nsCrypto();
  nsresult init();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMCRYPTO

private:
  static already_AddRefed<nsIPrincipal> GetScriptPrincipal(JSContext *cx);

  bool mEnableSmartCardEvents;
};

class nsPkcs11 : public nsIPKCS11
{
public:
  nsPkcs11();
  virtual ~nsPkcs11();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11

};

#endif //_nsCrypto_h_
