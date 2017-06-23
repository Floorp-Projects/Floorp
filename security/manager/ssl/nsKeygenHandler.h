/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsKeygenHandler_h
#define nsKeygenHandler_h

#include "ScopedNSSTypes.h"
#include "keythi.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsIFormProcessor.h"
#include "nsIInterfaceRequestor.h"
#include "nsNSSShutDown.h"
#include "nsString.h"
#include "nsTArray.h"
#include "secmodt.h"

nsresult GetSlotWithMechanism(uint32_t mechanism,
                              nsIInterfaceRequestor* ctx,
                              PK11SlotInfo** retSlot,
                              nsNSSShutDownPreventionLock& /*proofOfLock*/);

#define DEFAULT_RSA_KEYGEN_PE 65537L
#define DEFAULT_RSA_KEYGEN_ALG SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION

mozilla::UniqueSECItem DecodeECParams(const char* curve);

class nsKeygenFormProcessor : public nsIFormProcessor
                            , public nsNSSShutDownObject
{
public:
  nsKeygenFormProcessor();
  nsresult Init();

  virtual nsresult ProcessValue(nsIDOMHTMLElement* aElement,
                                const nsAString& aName,
                                nsAString& aValue) override;

  virtual nsresult ProcessValueIPC(const nsAString& aOldValue,
                                   const nsAString& aChallenge,
                                   const nsAString& aKeyType,
                                   const nsAString& aKeyParams,
                                   nsAString& aNewValue) override;

  virtual nsresult ProvideContent(const nsAString& aFormType,
                                  nsTArray<nsString>& aContent,
                                  nsAString& aAttribute) override;
  NS_DECL_THREADSAFE_ISUPPORTS

  static nsresult Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);

  static void ExtractParams(nsIDOMHTMLElement* aElement,
                            nsAString& challengeValue,
                            nsAString& keyTypeValue,
                            nsAString& keyParamsValue);

  // Nothing to release.
  virtual void virtualDestroyNSSReference() override {}

protected:
  virtual ~nsKeygenFormProcessor();

  nsresult GetPublicKey(const nsAString& aValue, const nsAString& aChallenge,
                        const nsString& akeyType, nsAString& aOutPublicKey,
                        const nsAString& aPqg);
  nsresult GetSlot(uint32_t aMechanism, PK11SlotInfo** aSlot);
private:
  nsCOMPtr<nsIInterfaceRequestor> m_ctx;

  typedef struct SECKeySizeChoiceInfoStr {
      nsString name;
      int size;
  } SECKeySizeChoiceInfo;

  enum { number_of_key_size_choices = 2 };

  SECKeySizeChoiceInfo mSECKeySizeChoiceList[number_of_key_size_choices];
};

#endif // nsKeygenHandler_h
