/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is WeaveCrypto code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mills <thunder@mozilla.com> (original author)
 *   Honza Bambas <honzab@allpeers.com>
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

#ifndef WeaveCrypto_h_
#define WeaveCrypto_h_

#include "IWeaveCrypto.h"
#include "pk11pub.h"

#define WEAVE_CRYPTO_CONTRACTID "@labs.mozilla.com/Weave/Crypto;1"
#define WEAVE_CRYPTO_CLASSNAME "Weave crypto module"
#define WEAVE_CRYPTO_CID { 0xd3b0f750, 0xc976, 0x46d0, \
                           { 0xbe, 0x20, 0x96, 0xb2, 0x4f, 0x46, 0x84, 0xbc } }

class WeaveCrypto : public IWeaveCrypto
{
public:
  WeaveCrypto();

  NS_DECL_ISUPPORTS
  NS_DECL_IWEAVECRYPTO

private:
  ~WeaveCrypto();

  SECOidTag mAlgorithm;
  PRUint32  mKeypairBits;

  nsresult DecodeBase64(const nsACString& base64, nsACString& retval);
  nsresult DecodeBase64(const nsACString& base64, char *aData, PRUint32 *aLength);
  void EncodeBase64(const nsACString& binary, nsACString& retval);
  void EncodeBase64(const char *aData, PRUint32 aLength, nsACString& retval);

  nsresult CommonCrypt(const char *input, PRUint32 inputSize,
                       char *output, PRUint32 *outputSize,
                       const nsACString& aSymmetricKey,
                       const nsACString& aIV,
                       CK_ATTRIBUTE_TYPE aOperation);


  nsresult DeriveKeyFromPassphrase(const nsACString& aPassphrase,
                                   const nsACString& aSalt,
                                   PK11SymKey **aSymKey);

  nsresult WrapPrivateKey(SECKEYPrivateKey *aPrivateKey,
                          const nsACString& aPassphrase,
                          const nsACString& aSalt,
                          const nsACString& aIV,
                          nsACString& aEncodedPublicKey);
  nsresult EncodePublicKey(SECKEYPublicKey *aPublicKey,
                           nsACString& aEncodedPublicKey);


};

#endif // WeaveCrypto_h_
