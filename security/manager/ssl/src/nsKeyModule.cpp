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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tony Chang <tc@google.com>
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

#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsKeyModule.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS1(nsKeyObject, nsIKeyObject)

nsKeyObject::nsKeyObject()
  : mKeyType(0), mSymKey(nsnull), mPrivateKey(nsnull),
    mPublicKey(nsnull)
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
      PK11_DeleteTokenPrivateKey(mPrivateKey, PR_TRUE /* force */);
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
nsKeyObject::InitKey(PRInt16 aAlgorithm, void * aKey)
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
nsKeyObject::GetType(PRInt16 *_retval)
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
nsKeyObjectFactory::UnwrapKey(PRInt16 aAlgorithm, const PRUint8 *aWrappedKey,
                              PRUint32 aWrappedKeyLen, nsIKeyObject **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsKeyObjectFactory::KeyFromString(PRInt16 aAlgorithm, const nsACString & aKey,
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

  PK11SlotInfo *slot = nsnull;
  slot = PK11_GetBestSlot(cipherMech, nsnull);
  if (!slot) {
    NS_ERROR("no slot");
    return NS_ERROR_FAILURE;
  }

  PK11SymKey* symKey = PK11_ImportSymKey(slot, cipherMech, PK11_OriginUnwrap,
                                         cipherOperation, &keyItem, nsnull);
  // cleanup code
  if (slot)
    PK11_FreeSlot(slot);

  if (!symKey) {
    return NS_ERROR_FAILURE;
  }
  
  rv = key->InitKey(aAlgorithm, (void*)symKey);
  NS_ENSURE_SUCCESS(rv, rv);

  key.swap(*_retval);
  return NS_OK;
}
