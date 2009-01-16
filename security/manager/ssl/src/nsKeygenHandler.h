/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Drinan. (ddrinan@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _NSKEYGENHANDLER_H_
#define _NSKEYGENHANDLER_H_
// Form Processor 
#include "nsIFormProcessor.h" 

nsresult GetSlotWithMechanism(PRUint32 mechanism,
                              nsIInterfaceRequestor *ctx,
                              PK11SlotInfo **retSlot);

#define DEFAULT_RSA_KEYGEN_PE 65537L
#define DEFAULT_RSA_KEYGEN_ALG SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION

SECKEYECParams *decode_ec_params(char *curve);

class nsKeygenFormProcessor : public nsIFormProcessor { 
public: 
  nsKeygenFormProcessor(); 
  virtual ~nsKeygenFormProcessor();
  nsresult Init();

  NS_IMETHOD ProcessValue(nsIDOMHTMLElement *aElement, 
                          const nsAString& aName, 
                          nsAString& aValue); 

  NS_IMETHOD ProvideContent(const nsAString& aFormType, 
                            nsStringArray& aContent, 
                            nsAString& aAttribute); 
  NS_DECL_ISUPPORTS 

  static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);

protected:
  nsresult GetPublicKey(nsAString& aValue, nsAString& aChallenge, 
			nsAFlatString& akeyType, nsAString& aOutPublicKey,
			nsAString& aPqg);
  nsresult GetSlot(PRUint32 aMechanism, PK11SlotInfo** aSlot);
private:
  nsCOMPtr<nsIInterfaceRequestor> m_ctx;

  typedef struct SECKeySizeChoiceInfoStr {
      nsString name;
      int size;
  } SECKeySizeChoiceInfo;

  enum { number_of_key_size_choices = 2 };

  SECKeySizeChoiceInfo mSECKeySizeChoiceList[number_of_key_size_choices];
};

#endif //_NSKEYGENHANDLER_H_
