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
 *   Terry Hayes <thayes@netscape.com>
 */
#include "nsISupports.h"
#include "nsIPK11TokenDB.h"

#include "nsPK11TokenDB.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsString.h"
#include "pk11func.h"

class nsPK11Token : public nsIPK11Token
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPK11TOKEN

  nsPK11Token(PK11SlotInfo *slot);
  virtual ~nsPK11Token();
  /* additional members */

private:
  friend class nsPK11TokenDB;

  nsString mTokenName;
  PK11SlotInfo *mSlot;
};

NS_IMPL_ISUPPORTS1(nsPK11Token, nsIPK11Token)

nsPK11Token::nsPK11Token(PK11SlotInfo *slot)
{
  NS_INIT_ISUPPORTS();

  PK11_ReferenceSlot(slot);
  mSlot = slot;
  
  mTokenName = NS_ConvertUTF8toUCS2(PK11_GetTokenName(slot));
}

nsPK11Token::~nsPK11Token()
{
  if (mSlot) PK11_FreeSlot(mSlot);
  /* destructor code */
}

/* readonly attribute wstring tokenName; */
NS_IMETHODIMP nsPK11Token::GetTokenName(PRUnichar * *aTokenName)
{
  *aTokenName = mTokenName.ToNewUnicode();
  if (!*aTokenName) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

/* boolean isLoggedIn (); */
NS_IMETHODIMP nsPK11Token::IsLoggedIn(PRBool *_retval)
{
  nsresult rv = NS_OK;

  *_retval = PK11_IsLoggedIn(mSlot, 0);

  return rv;
}

/* void logout (); */
NS_IMETHODIMP nsPK11Token::Logout()
{
  nsresult rv = NS_OK;

  PK11_Logout(mSlot);

  return rv;
}

/* readonly attribute long minimumPasswordLength; */
NS_IMETHODIMP nsPK11Token::GetMinimumPasswordLength(PRInt32 *aMinimumPasswordLength)
{
  nsresult rv = NS_OK;

  *aMinimumPasswordLength = PK11_GetMinimumPwdLength(mSlot);

  return NS_OK;
}

/* readonly attribute boolean needsUserInit; */
NS_IMETHODIMP nsPK11Token::GetNeedsUserInit(PRBool *aNeedsUserInit)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean checkPassword (in wstring password); */
NS_IMETHODIMP nsPK11Token::CheckPassword(const PRUnichar *password, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void initPassword (in wstring initialPassword); */
NS_IMETHODIMP nsPK11Token::InitPassword(const PRUnichar *initialPassword)
{
    nsresult rv;
    SECStatus status;

    status = PK11_InitPin(mSlot, "", NS_ConvertUCS2toUTF8(initialPassword));
    if (status == SECFailure) { rv = NS_ERROR_FAILURE; goto done; }

done:
    return rv;
}

/* void changePassword (in wstring oldPassword, in wstring newPassword); */
NS_IMETHODIMP nsPK11Token::ChangePassword(const PRUnichar *oldPassword, const PRUnichar *newPassword)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isHardwareToken (); */
NS_IMETHODIMP nsPK11Token::IsHardwareToken(PRBool *_retval)
{
  nsresult rv = NS_OK;

  *_retval = PK11_IsHW(mSlot);

  return rv;
}

/* boolean needsLogin (); */
NS_IMETHODIMP nsPK11Token::NeedsLogin(PRBool *_retval)
{
  nsresult rv = NS_OK;

  *_retval = PK11_NeedLogin(mSlot);

  return rv;
}

/* boolean isFriendly (); */
NS_IMETHODIMP nsPK11Token::IsFriendly(PRBool *_retval)
{
  nsresult rv = NS_OK;

  *_retval = PK11_IsFriendly(mSlot);

  return rv;
}
/*=========================================================*/

NS_IMPL_ISUPPORTS1(nsPK11TokenDB, nsIPK11TokenDB)

nsPK11TokenDB::nsPK11TokenDB()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsPK11TokenDB::~nsPK11TokenDB()
{
  /* destructor code */
}

/* nsIPK11Token getInternalKeyToken (); */
NS_IMETHODIMP nsPK11TokenDB::GetInternalKeyToken(nsIPK11Token **_retval)
{
  nsresult rv = NS_OK;
  PK11SlotInfo *slot = 0;
  nsCOMPtr<nsPK11Token> token;

  slot = PK11_GetInternalKeySlot();
  if (!slot) { rv = NS_ERROR_FAILURE; goto done; }

  token = new nsPK11Token(slot);
  if (!token) { rv = NS_ERROR_OUT_OF_MEMORY; goto done; }

  *_retval = token;
  NS_ADDREF(*_retval);

done:
  if (slot) PK11_FreeSlot(slot);
  return rv;
}

/* nsIPK11Token findTokenByName (in wchar tokenName); */
NS_IMETHODIMP nsPK11TokenDB::
FindTokenByName(const PRUnichar* tokenName, nsIPK11Token **_retval)
{
  nsresult rv = NS_OK;
  PK11SlotInfo *slot = 0;
  slot = PK11_FindSlotByName(NS_ConvertUCS2toUTF8(tokenName));
  if (!slot) { rv = NS_ERROR_FAILURE; goto done; }

  *_retval = new nsPK11Token(slot);
  if (!*_retval) { rv = NS_ERROR_OUT_OF_MEMORY; goto done; }

  NS_ADDREF(*_retval);

done:
  if (slot) PK11_FreeSlot(slot);
  return rv;
}

/* nsIEnumerator listTokens (); */
NS_IMETHODIMP nsPK11TokenDB::ListTokens(nsIEnumerator* *_retval)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupportsArray> array;
  PK11SlotList *list = 0;
  PK11SlotListElement *le;

  rv = NS_NewISupportsArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) { goto done; }

  /* List all tokens, creating PK11Token objects and putting them
   * into the array.
   */
  list = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE, 0);
  if (!list) { rv = NS_ERROR_FAILURE; goto done; }

  for (le = PK11_GetFirstSafe(list); le; le = PK11_GetNextSafe(list, le, PR_FALSE)) {
    nsCOMPtr<nsPK11Token> token = new nsPK11Token(le->slot);

    array->AppendElement(token);
  }

  rv = array->Enumerate(_retval);

done:
  if (list) PK11_FreeSlotList(list);
  return rv;
}