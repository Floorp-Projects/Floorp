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
#include "prerror.h"
#include "secerr.h"
#include "nsReadableUtils.h"

#include "nsPK11TokenDB.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

NS_IMPL_ISUPPORTS1(nsPK11Token, nsIPK11Token)

nsPK11Token::nsPK11Token(PK11SlotInfo *slot)
{
  NS_INIT_ISUPPORTS();

  PK11_ReferenceSlot(slot);
  mSlot = slot;
  
  mTokenName = NS_ConvertUTF8toUCS2(PK11_GetTokenName(slot));

  SECStatus srv;

  CK_TOKEN_INFO tok_info;
  srv = PK11_GetTokenInfo(mSlot, &tok_info);
  if (srv == SECSuccess) {
    // Set the Label field
    mTokenLabel.AssignWithConversion((char *)tok_info.label, 
                                     sizeof(tok_info.label));
    mTokenLabel.Trim(" ", PR_FALSE, PR_TRUE);
    // Set the Manufacturer field
    mTokenManID.AssignWithConversion((char *)tok_info.manufacturerID, 
                                     sizeof(tok_info.manufacturerID));
    mTokenManID.Trim(" ", PR_FALSE, PR_TRUE);
    // Set the Hardware Version field
    mTokenHWVersion.AppendInt(tok_info.hardwareVersion.major);
    mTokenHWVersion.AppendWithConversion(".");
    mTokenHWVersion.AppendInt(tok_info.hardwareVersion.minor);
    // Set the Firmware Version field
    mTokenFWVersion.AppendInt(tok_info.firmwareVersion.major);
    mTokenFWVersion.AppendWithConversion(".");
    mTokenFWVersion.AppendInt(tok_info.firmwareVersion.minor);
    // Set the Serial Number field
    mTokenSerialNum.AssignWithConversion((char *)tok_info.serialNumber, 
                                         sizeof(tok_info.serialNumber));
    mTokenSerialNum.Trim(" ", PR_FALSE, PR_TRUE);
  }

  mUIContext = new PipUIContext();
}

nsPK11Token::~nsPK11Token()
{
  if (mSlot) PK11_FreeSlot(mSlot);
  /* destructor code */
}

/* readonly attribute wstring tokenName; */
NS_IMETHODIMP nsPK11Token::GetTokenName(PRUnichar * *aTokenName)
{
  *aTokenName = ToNewUnicode(mTokenName);
  if (!*aTokenName) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

/* readonly attribute wstring tokenDesc; */
NS_IMETHODIMP nsPK11Token::GetTokenLabel(PRUnichar **aTokLabel)
{
  *aTokLabel = ToNewUnicode(mTokenLabel);
  if (!*aTokLabel) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring tokenManID; */
NS_IMETHODIMP nsPK11Token::GetTokenManID(PRUnichar **aTokManID)
{
  *aTokManID = ToNewUnicode(mTokenManID);
  if (!*aTokManID) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring tokenHWVersion; */
NS_IMETHODIMP nsPK11Token::GetTokenHWVersion(PRUnichar **aTokHWVersion)
{
  *aTokHWVersion = ToNewUnicode(mTokenHWVersion);
  if (!*aTokHWVersion) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring tokenFWVersion; */
NS_IMETHODIMP nsPK11Token::GetTokenFWVersion(PRUnichar **aTokFWVersion)
{
  *aTokFWVersion = ToNewUnicode(mTokenFWVersion);
  if (!*aTokFWVersion) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring tokenSerialNumber; */
NS_IMETHODIMP nsPK11Token::GetTokenSerialNumber(PRUnichar **aTokSerialNum)
{
  *aTokSerialNum = ToNewUnicode(mTokenSerialNum);
  if (!*aTokSerialNum) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* boolean isLoggedIn (); */
NS_IMETHODIMP nsPK11Token::IsLoggedIn(PRBool *_retval)
{
  nsresult rv = NS_OK;

  *_retval = PK11_IsLoggedIn(mSlot, 0);

  return rv;
}

/* void logout (in boolean force); */
NS_IMETHODIMP 
nsPK11Token::Login(PRBool force)
{
  nsresult rv;
  SECStatus srv;
  PRBool test;
  rv = this->NeedsLogin(&test);
  if (NS_FAILED(rv)) return rv;
  if (test && force) {
    rv = this->Logout();
    if (NS_FAILED(rv)) return rv;
  }
  rv = setPassword(mSlot, mUIContext);
  if (NS_FAILED(rv)) return rv;
  srv = PK11_Authenticate(mSlot, PR_TRUE, mUIContext);
  return (srv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

/* void logout (); */
NS_IMETHODIMP nsPK11Token::Logout()
{
  // PK11_MapError sets CKR_USER_NOT_LOGGED_IN to SEC_ERROR_LIBRARY_FAILURE,
  // so not going to learn anything here by a failure.  Treat it like void.
  PK11_Logout(mSlot);
  return NS_OK;
}

/* void reset (); */
NS_IMETHODIMP nsPK11Token::Reset()
{
  PK11_ResetToken(mSlot, 0);
  return NS_OK;
}

/* readonly attribute long minimumPasswordLength; */
NS_IMETHODIMP nsPK11Token::GetMinimumPasswordLength(PRInt32 *aMinimumPasswordLength)
{
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
  SECStatus srv;
  PRInt32 prerr;
  srv = PK11_CheckUserPassword(mSlot, 
                  NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(password).get()));
  if (srv != SECSuccess) {
    *_retval =  PR_FALSE;
    prerr = PR_GetError();
    if (prerr != SEC_ERROR_BAD_PASSWORD) {
      /* something really bad happened - throw an exception */
      return NS_ERROR_FAILURE;
    }
  } else {
    *_retval =  PR_TRUE;
  }
  return NS_OK;
}

/* void initPassword (in wstring initialPassword); */
NS_IMETHODIMP nsPK11Token::InitPassword(const PRUnichar *initialPassword)
{
    nsresult rv = NS_OK;
    SECStatus status;

    status = PK11_InitPin(mSlot, "", NS_CONST_CAST(char*, NS_ConvertUCS2toUTF8(initialPassword).get()));
    if (status == SECFailure) { rv = NS_ERROR_FAILURE; goto done; }

done:
    return rv;
}

/* long getAskPasswordTimes(); */
NS_IMETHODIMP 
nsPK11Token::GetAskPasswordTimes(PRInt32 *rvAskTimes)
{
    int askTimes, askTimeout;
    PK11_GetSlotPWValues(mSlot, &askTimes, &askTimeout);
    *rvAskTimes = askTimes;
    return NS_OK;
}

/* long getAskPasswordTimeout(); */
NS_IMETHODIMP 
nsPK11Token::GetAskPasswordTimeout(PRInt32 *rvAskTimeout)
{
    int askTimes, askTimeout;
    PK11_GetSlotPWValues(mSlot, &askTimes, &askTimeout);
    *rvAskTimeout = askTimeout;
    return NS_OK;
}

/* void setAskPasswordDefaults(in unsigned long askTimes,
 *                             in unsigned long timeout);
 */
NS_IMETHODIMP 
nsPK11Token::SetAskPasswordDefaults(const PRInt32 askTimes,
                                    const PRInt32 askTimeout)
{
    PK11_SetSlotPWValues(mSlot, askTimes, askTimeout);
    return NS_OK;
}

/* void changePassword (in wstring oldPassword, in wstring newPassword); */
NS_IMETHODIMP nsPK11Token::ChangePassword(const PRUnichar *oldPassword, const PRUnichar *newPassword)
{
  SECStatus rv;
  rv = PK11_ChangePW(mSlot, 
               NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(oldPassword).get()), 
               NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(newPassword).get()));
  return (rv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
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
  nsCOMPtr<nsIPK11Token> token;

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
  slot = PK11_FindSlotByName(NS_CONST_CAST(char*, NS_ConvertUCS2toUTF8(tokenName).get()));
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
    nsCOMPtr<nsIPK11Token> token = new nsPK11Token(le->slot);

    array->AppendElement(token);
  }

  rv = array->Enumerate(_retval);

done:
  if (list) PK11_FreeSlotList(list);
  return rv;
}

