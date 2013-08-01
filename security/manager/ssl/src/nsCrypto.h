/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _nsCrypto_h_
#define _nsCrypto_h_

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#ifndef MOZ_DISABLE_CRYPTOLEGACY
#include "Crypto.h"
#include "nsCOMPtr.h"
#include "nsIDOMCRMFObject.h"
#include "nsIDOMCryptoLegacy.h"
#include "nsIRunnable.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIPrincipal.h"

#define NS_CRYPTO_CID \
  {0x929d9320, 0x251e, 0x11d4, { 0x8a, 0x7c, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3} }
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

class nsCrypto: public mozilla::dom::Crypto
{
public:
  nsCrypto();
  virtual ~nsCrypto();

  NS_DECL_ISUPPORTS_INHERITED

  // If legacy DOM crypto is enabled this is the class that actually
  // implements the legacy methods.
  NS_DECL_NSIDOMCRYPTO

  virtual bool EnableSmartCardEvents() MOZ_OVERRIDE;
  virtual void SetEnableSmartCardEvents(bool aEnable,
                                        mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

  virtual void GetVersion(nsString& aVersion) MOZ_OVERRIDE;

  virtual already_AddRefed<nsIDOMCRMFObject>
  GenerateCRMFRequest(JSContext* aContext,
                      const nsCString& aReqDN,
                      const nsCString& aRegToken,
                      const nsCString& aAuthenticator,
                      const nsCString& aEaCert,
                      const nsCString& aJsCallback,
                      const mozilla::dom::Sequence<JS::Value>& aArgs,
                      mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

  virtual void ImportUserCertificates(const nsAString& aNickname,
                                      const nsAString& aCmmfResponse,
                                      bool aDoForcedBackup,
                                      nsAString& aReturn,
                                      mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

  virtual void PopChallengeResponse(const nsAString& aChallenge,
                                    nsAString& aReturn,
                                    mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

  virtual void Random(int32_t aNumBytes,
                      nsAString& aReturn,
                      mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

  virtual void SignText(JSContext* aContext,
                        const nsAString& aStringToSign,
                        const nsAString& aCaOption,
                        const mozilla::dom::Sequence<nsCString>& aArgs,
                        nsAString& aReturn) MOZ_OVERRIDE;

  virtual void Logout(mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

  virtual void DisableRightClick(mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

private:
  static already_AddRefed<nsIPrincipal> GetScriptPrincipal(JSContext *cx);

  bool mEnableSmartCardEvents;
};
#endif // MOZ_DISABLE_CRYPTOLEGACY

#include "nsIPKCS11.h"

#define NS_PKCS11_CID \
  {0x74b7a390, 0x3b41, 0x11d4, { 0x8a, 0x80, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3} }

class nsPkcs11 : public nsIPKCS11
{
public:
  nsPkcs11();
  virtual ~nsPkcs11();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11

};

#endif //_nsCrypto_h_
