/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsKeyModule.h"
#include "nsString.h"

using namespace mozilla;
using namespace mozilla::psm;

NS_IMPL_ISUPPORTS(nsKeyObject, nsIKeyObject)

nsKeyObject::nsKeyObject()
  : mSymKey(nullptr)
{
}

nsKeyObject::~nsKeyObject()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void
nsKeyObject::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void
nsKeyObject::destructorSafeDestroyNSSReference()
{
  mSymKey = nullptr;
}

//////////////////////////////////////////////////////////////////////////////
// nsIKeyObject

NS_IMETHODIMP
nsKeyObject::InitKey(int16_t aAlgorithm, PK11SymKey* aKey)
{
  if (!aKey || aAlgorithm != nsIKeyObject::HMAC) {
    return NS_ERROR_INVALID_ARG;
  }

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mSymKey.reset(aKey);
  return NS_OK;
}

NS_IMETHODIMP
nsKeyObject::GetKeyObj(PK11SymKey** _retval)
{
  if (!_retval) {
    return NS_ERROR_INVALID_ARG;
  }

  *_retval = nullptr;

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mSymKey) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *_retval = mSymKey.get();
  return NS_OK;
}

NS_IMETHODIMP
nsKeyObject::GetType(int16_t *_retval)
{
  if (!_retval) {
    return NS_ERROR_INVALID_ARG;
  }
  *_retval = nsIKeyObject::SYM_KEY;
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// nsIKeyObjectFactory

NS_IMPL_ISUPPORTS(nsKeyObjectFactory, nsIKeyObjectFactory)

nsKeyObjectFactory::nsKeyObjectFactory()
{
}

nsKeyObjectFactory::~nsKeyObjectFactory()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(calledFromObject);
}

NS_IMETHODIMP
nsKeyObjectFactory::KeyFromString(int16_t aAlgorithm, const nsACString& aKey,
                                  nsIKeyObject** _retval)
{
  if (!_retval || aAlgorithm != nsIKeyObject::HMAC) {
    return NS_ERROR_INVALID_ARG;
  }

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CK_MECHANISM_TYPE cipherMech = CKM_GENERIC_SECRET_KEY_GEN;
  CK_ATTRIBUTE_TYPE cipherOperation = CKA_SIGN;

  nsresult rv;
  nsCOMPtr<nsIKeyObject> key(
    do_CreateInstance(NS_KEYMODULEOBJECT_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Convert the raw string into a SECItem
  const nsCString& flatKey = PromiseFlatCString(aKey);
  SECItem keyItem;
  keyItem.data = (unsigned char*)flatKey.get();
  keyItem.len = flatKey.Length();

  UniquePK11SlotInfo slot(PK11_GetBestSlot(cipherMech, nullptr));
  if (!slot) {
    return NS_ERROR_FAILURE;
  }

  UniquePK11SymKey symKey(PK11_ImportSymKey(slot.get(), cipherMech,
                                            PK11_OriginUnwrap, cipherOperation,
                                            &keyItem, nullptr));
  if (!symKey) {
    return NS_ERROR_FAILURE;
  }

  rv = key->InitKey(aAlgorithm, symKey.release());
  if (NS_FAILED(rv)) {
    return rv;
  }

  key.swap(*_retval);
  return NS_OK;
}
