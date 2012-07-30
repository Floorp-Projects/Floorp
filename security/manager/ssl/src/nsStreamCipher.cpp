/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIKeyModule.h"
#include "nsStreamCipher.h"
#include "nsStreamUtils.h"
#include "base64.h"

NS_IMPL_ISUPPORTS1(nsStreamCipher, nsIStreamCipher)

nsStreamCipher::nsStreamCipher()
  : mContext(NULL)
{
}

nsStreamCipher::~nsStreamCipher()
{
  if (mContext)
    PK11_DestroyContext(mContext, true /* free sub-objects */);
}

nsresult
nsStreamCipher::InitWithIV_(nsIKeyObject *aKey, SECItem* aIV)
{
  NS_ENSURE_ARG_POINTER(aKey);

  // Make sure we have a SYM_KEY.
  PRInt16 keyType;
  nsresult rv = aKey->GetType(&keyType);
  NS_ENSURE_SUCCESS(rv, rv);
  if (keyType != nsIKeyObject::SYM_KEY)
    return NS_ERROR_INVALID_ARG;

  if (mContext)
    PK11_DestroyContext(mContext, true /* free sub-objects */);

  // Get the PK11SymKey out of the key object and create the PK11Context.
  void* keyObj;
  rv = aKey->GetKeyObj(&keyObj);
  NS_ENSURE_SUCCESS(rv, rv);

  PK11SymKey *symkey = reinterpret_cast<PK11SymKey*>(keyObj);
  if (!symkey)
    return NS_ERROR_FAILURE;

  CK_MECHANISM_TYPE cipherMech = PK11_GetMechanism(symkey);

  SECItem *param = nullptr;
  // aIV may be null
  param = PK11_ParamFromIV(cipherMech, aIV);
  if (!param)
    return NS_ERROR_FAILURE;

  mContext = PK11_CreateContextBySymKey(cipherMech, CKA_ENCRYPT,
                                        symkey, param);

  SECITEM_FreeItem(param, true);

  // Something went wrong if mContext doesn't exist.
  if (!mContext)
    return NS_ERROR_FAILURE;

  // Everything went ok.      
  mValue.Truncate();
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// nsIStreamCipher

NS_IMETHODIMP nsStreamCipher::Init(nsIKeyObject *aKey)
{
  return InitWithIV_(aKey, nullptr);
}

NS_IMETHODIMP nsStreamCipher::InitWithIV(nsIKeyObject *aKey,
                                         const PRUint8 *aIV, PRUint32 aIVLen)
{
  SECItem IV;
  IV.data = (unsigned char*)aIV;
  IV.len = aIVLen;
  return InitWithIV_(aKey, &IV);
}

NS_IMETHODIMP nsStreamCipher::Update(const PRUint8 *aData, PRUint32 aLen)
{
  if (!mContext)
    return NS_ERROR_NOT_INITIALIZED;

  unsigned char* output = new unsigned char[aLen];
  if (!output)
    return NS_ERROR_OUT_OF_MEMORY;
  unsigned char* input = (unsigned char*)aData;
  
  PRInt32 setLen;

#ifdef DEBUG
  SECStatus rv =
#endif
    PK11_CipherOp(mContext, output, &setLen, aLen, input, aLen);
  NS_ASSERTION(rv == SECSuccess, "failed to encrypt");
  NS_ASSERTION((PRUint32)setLen == aLen, "data length should not change");

  mValue.Append((const char*)output, aLen);

  delete [] output;

  return NS_OK;
}

NS_IMETHODIMP nsStreamCipher::UpdateFromStream(nsIInputStream *aStream,
                                               PRInt32 aLen)
{
  if (!mContext)
    return NS_ERROR_NOT_INITIALIZED;

  nsCString inputString;
  nsresult rv = NS_ConsumeStream(aStream, aLen, inputString);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return UpdateFromString(inputString);
}

NS_IMETHODIMP nsStreamCipher::UpdateFromString(const nsACString& aInput)
{
  if (!mContext)
    return NS_ERROR_NOT_INITIALIZED;

  const nsCString& flatInput = PromiseFlatCString(aInput);
  unsigned char* input = (unsigned char*)flatInput.get();
  PRUint32 len = aInput.Length();

  unsigned char* output = new unsigned char[len];
  if (!output)
    return NS_ERROR_OUT_OF_MEMORY;

  PRInt32 setLen;

#ifdef DEBUG
  SECStatus rv =
#endif
    PK11_CipherOp(mContext, output, &setLen, len, input, len);
  NS_ASSERTION(rv == SECSuccess, "failed to encrypt");
  NS_ASSERTION((PRUint32)setLen == len, "data length should not change");

  mValue.Append((const char*)output, len);
  delete [] output;

  return NS_OK;
}

NS_IMETHODIMP nsStreamCipher::Finish(bool aASCII, nsACString & _retval)
{
  if (!mContext)
    return NS_ERROR_NOT_INITIALIZED;

  if (aASCII) {
    char *asciiData = BTOA_DataToAscii((unsigned char*)(mValue.get()),
                                       mValue.Length());
    _retval.Assign(asciiData);
    PORT_Free(asciiData);
  } else {
    _retval.Assign(mValue);
  }

  return NS_OK;
}

NS_IMETHODIMP nsStreamCipher::Discard(PRInt32 aLen)
{
  if (!mContext)
    return NS_ERROR_NOT_INITIALIZED;

  unsigned char* output = new unsigned char[aLen];
  if (!output)
    return NS_ERROR_OUT_OF_MEMORY;

  unsigned char* input = new unsigned char[aLen];
  if (!input) {
    delete [] output;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 setLen;

#ifdef DEBUG
  SECStatus rv =
#endif
    PK11_CipherOp(mContext, output, &setLen, aLen, input, aLen);
  NS_ASSERTION(rv == SECSuccess, "failed to encrypt");
  NS_ASSERTION(setLen == aLen, "data length should not change");
  
  delete [] output;
  delete [] input;
  return NS_OK;
}
