/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsKeyModule.h"
#include "nsString.h"
#include "ScopedNSSTypes.h"

using namespace mozilla;
using namespace mozilla::psm;

NS_IMPL_ISUPPORTS1(nsKeyObject, nsIKeyObject)

nsKeyObject::nsKeyObject()
  : mKeyType(0), mSymKey(nullptr), mPrivateKey(nullptr),
    mPublicKey(nullptr)
{
}

nsKeyObject::~nsKeyObject()
{
  CleanUp();
}

void
nsKeyObject::CleanUp()
{
  switch (mKeyType) {
    case nsIKeyObject::SYM_KEY:
      PK11_FreeSymKey(mSymKey);
      break;
    
    case nsIKeyObject::PRIVATE_KEY:
      PK11_DeleteTokenPrivateKey(mPrivateKey, true /* force */);
      break;

    case nsIKeyObject::PUBLIC_KEY:
      PK11_DeleteTokenPublicKey(mPublicKey);
      break;
    
    default:
      // probably not initialized, do nothing
      break;
  }
  mKeyType = 0;
}

//////////////////////////////////////////////////////////////////////////////
// nsIKeyObject

/* [noscript] void initKey (in short aKeyType, in voidPtr aKey); */
NS_IMETHODIMP
nsKeyObject::InitKey(int16_t aAlgorithm, void * aKey)
{
  // Clear previous key data if it exists
  CleanUp();

  switch (aAlgorithm) {
    case nsIKeyObject::RC4:
    case nsIKeyObject::HMAC:
      mSymKey = reinterpret_cast<PK11SymKey*>(aKey);

      if (!mSymKey) {
        NS_ERROR("no symkey");
        break;
      }
      mKeyType = nsIKeyObject::SYM_KEY;
      break;

    case nsIKeyObject::AES_CBC:
      return NS_ERROR_NOT_IMPLEMENTED;

    default:
      return NS_ERROR_INVALID_ARG;
  }

  // One of these should have been created
  if (!mSymKey && !mPrivateKey && !mPublicKey)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

/* [noscript] voidPtr getKeyObj (); */
NS_IMETHODIMP
nsKeyObject::GetKeyObj(void * *_retval)
{
  if (mKeyType == 0)
    return NS_ERROR_NOT_INITIALIZED;

  switch (mKeyType) {
    case nsIKeyObject::SYM_KEY:
      *_retval = (void*)mSymKey;
      break;

    case nsIKeyObject::PRIVATE_KEY:
      *_retval = (void*)mPublicKey;
      break;

    case nsIKeyObject::PUBLIC_KEY:
      *_retval = (void*)mPrivateKey;
      break;

    default:
      // unknown key type?  How did that happen?
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/* short getType (); */
NS_IMETHODIMP
nsKeyObject::GetType(int16_t *_retval)
{
  if (mKeyType == 0)
    return NS_ERROR_NOT_INITIALIZED;

  *_retval = mKeyType;
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// nsIKeyObjectFactory

NS_IMPL_THREADSAFE_ISUPPORTS1(nsKeyObjectFactory, nsIKeyObjectFactory)

nsKeyObjectFactory::nsKeyObjectFactory()
{
}

/* nsIKeyObject lookupKeyByName (in ACString aName); */
NS_IMETHODIMP
nsKeyObjectFactory::LookupKeyByName(const nsACString & aName,
                                    nsIKeyObject **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP
nsKeyObjectFactory::UnwrapKey(int16_t aAlgorithm, const uint8_t *aWrappedKey,
                              uint32_t aWrappedKeyLen, nsIKeyObject **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsKeyObjectFactory::KeyFromString(int16_t aAlgorithm, const nsACString & aKey,
                                  nsIKeyObject **_retval)
{
  CK_MECHANISM_TYPE cipherMech;
  CK_ATTRIBUTE_TYPE cipherOperation;
  switch (aAlgorithm)
  {
  case nsIKeyObject::HMAC:
    cipherMech = CKM_GENERIC_SECRET_KEY_GEN;
    cipherOperation = CKA_SIGN;
    break;

  case nsIKeyObject::RC4:
    cipherMech = CKM_RC4;
    cipherOperation = CKA_ENCRYPT;
    break;

  default:
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsIKeyObject> key =
      do_CreateInstance(NS_KEYMODULEOBJECT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert the raw string into a SECItem
  const nsCString& flatKey = PromiseFlatCString(aKey);
  SECItem keyItem;
  keyItem.data = (unsigned char*)flatKey.get();
  keyItem.len = flatKey.Length();

  ScopedPK11SlotInfo slot(PK11_GetBestSlot(cipherMech, nullptr));
  if (!slot) {
    NS_ERROR("no slot");
    return NS_ERROR_FAILURE;
  }

  PK11SymKey* symKey = PK11_ImportSymKey(slot, cipherMech, PK11_OriginUnwrap,
                                         cipherOperation, &keyItem, nullptr);
  if (!symKey) {
    return NS_ERROR_FAILURE;
  }
  
  rv = key->InitKey(aAlgorithm, (void*)symKey);
  NS_ENSURE_SUCCESS(rv, rv);

  key.swap(*_retval);
  return NS_OK;
}
