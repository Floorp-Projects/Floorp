/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * David Drinan. (ddrinan@netscape.com)
 *
 */

#ifndef _NSKEYGENHANDLER_H_
#define _NSKEYGENHANDLER_H_
// Form Processor 
#include "nsIFormProcessor.h" 

typedef struct SECKeySizeChoiceInfoStr {
    PRUnichar *name;
    int size;
} SECKeySizeChoiceInfo;

nsresult GetSlotWithMechanism(PRUint32 mechanism,
                              nsIInterfaceRequestor *ctx,
                              PK11SlotInfo **retSlot);

#define DEFAULT_RSA_KEYGEN_PE 65537L
#define DEFAULT_RSA_KEYGEN_ALG SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION


class nsKeygenFormProcessor : public nsIFormProcessor { 
public: 
  nsKeygenFormProcessor(); 
  virtual ~nsKeygenFormProcessor();
  nsresult Init();

  NS_IMETHOD ProcessValue(nsIDOMHTMLElement *aElement, 
                          const nsString& aName, 
                          nsString& aValue); 

  NS_IMETHOD ProvideContent(const nsString& aFormType, 
                            nsVoidArray& aContent, 
                            nsString& aAttribute); 
  NS_DECL_ISUPPORTS 

  static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);

protected:
  nsresult GetPublicKey(nsAString& aValue, nsAString& aChallenge, 
			nsAString& akeyType, nsAString& aOutPublicKey,
			nsAString& aPqg);
  nsresult GetSlot(PRUint32 aMechanism, PK11SlotInfo** aSlot);
private:
  nsCOMPtr<nsIInterfaceRequestor> m_ctx;

}; 

#endif //_NSKEYGENHANDLER_H_
