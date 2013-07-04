/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsISupports.h"
#include "nsISupportsArray.h"
#include "nsIPK11TokenDB.h"
#include "prerror.h"
#include "secerr.h"
#include "nsReadableUtils.h"
#include "nsNSSComponent.h"
#include "nsServiceManagerUtils.h"

#include "nsPK11TokenDB.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

NS_IMPL_ISUPPORTS1(nsPK11Token, nsIPK11Token)

nsPK11Token::nsPK11Token(PK11SlotInfo *slot)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  PK11_ReferenceSlot(slot);
  mSlot = slot;
  mSeries = PK11_GetSlotSeries(slot);

  refreshTokenInfo();
  mUIContext = new PipUIContext();
}

void  
nsPK11Token::refreshTokenInfo()
{
  mTokenName = NS_ConvertUTF8toUTF16(PK11_GetTokenName(mSlot));

  SECStatus srv;

  CK_TOKEN_INFO tok_info;
  srv = PK11_GetTokenInfo(mSlot, &tok_info);
  if (srv == SECSuccess) {
    // Set the Label field

    const char *ccLabel = (const char*)tok_info.label;
    const nsACString &cLabel = Substring(
      ccLabel, 
      ccLabel+PL_strnlen(ccLabel, sizeof(tok_info.label)));
    mTokenLabel = NS_ConvertUTF8toUTF16(cLabel);
    mTokenLabel.Trim(" ", false, true);

    // Set the Manufacturer field
    const char *ccManID = (const char*)tok_info.manufacturerID;
    const nsACString &cManID = Substring(
      ccManID, 
      ccManID+PL_strnlen(ccManID, sizeof(tok_info.manufacturerID)));
    mTokenManID = NS_ConvertUTF8toUTF16(cManID);
    mTokenManID.Trim(" ", false, true);

    // Set the Hardware Version field
    mTokenHWVersion.AppendInt(tok_info.hardwareVersion.major);
    mTokenHWVersion.AppendLiteral(".");
    mTokenHWVersion.AppendInt(tok_info.hardwareVersion.minor);
    // Set the Firmware Version field
    mTokenFWVersion.AppendInt(tok_info.firmwareVersion.major);
    mTokenFWVersion.AppendLiteral(".");
    mTokenFWVersion.AppendInt(tok_info.firmwareVersion.minor);
    // Set the Serial Number field
    const char *ccSerial = (const char*)tok_info.serialNumber;
    const nsACString &cSerial = Substring(
      ccSerial, 
      ccSerial+PL_strnlen(ccSerial, sizeof(tok_info.serialNumber)));
    mTokenSerialNum = NS_ConvertUTF8toUTF16(cSerial);
    mTokenSerialNum.Trim(" ", false, true);
  }

}

nsPK11Token::~nsPK11Token()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsPK11Token::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsPK11Token::destructorSafeDestroyNSSReference()
{
  if (isAlreadyShutDown())
    return;

  if (mSlot) {
    PK11_FreeSlot(mSlot);
    mSlot = nullptr;
  }
}

/* readonly attribute wstring tokenName; */
NS_IMETHODIMP nsPK11Token::GetTokenName(PRUnichar * *aTokenName)
{
  // handle removals/insertions
  if (mSeries != PK11_GetSlotSeries(mSlot)) {
    refreshTokenInfo();
  }
  *aTokenName = ToNewUnicode(mTokenName);
  if (!*aTokenName) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

/* readonly attribute wstring tokenDesc; */
NS_IMETHODIMP nsPK11Token::GetTokenLabel(PRUnichar **aTokLabel)
{
  // handle removals/insertions
  if (mSeries != PK11_GetSlotSeries(mSlot)) {
    refreshTokenInfo();
  }
  *aTokLabel = ToNewUnicode(mTokenLabel);
  if (!*aTokLabel) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring tokenManID; */
NS_IMETHODIMP nsPK11Token::GetTokenManID(PRUnichar **aTokManID)
{
  // handle removals/insertions
  if (mSeries != PK11_GetSlotSeries(mSlot)) {
    refreshTokenInfo();
  }
  *aTokManID = ToNewUnicode(mTokenManID);
  if (!*aTokManID) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring tokenHWVersion; */
NS_IMETHODIMP nsPK11Token::GetTokenHWVersion(PRUnichar **aTokHWVersion)
{
  // handle removals/insertions
  if (mSeries != PK11_GetSlotSeries(mSlot)) {
    refreshTokenInfo();
  }
  *aTokHWVersion = ToNewUnicode(mTokenHWVersion);
  if (!*aTokHWVersion) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring tokenFWVersion; */
NS_IMETHODIMP nsPK11Token::GetTokenFWVersion(PRUnichar **aTokFWVersion)
{
  // handle removals/insertions
  if (mSeries != PK11_GetSlotSeries(mSlot)) {
    refreshTokenInfo();
  }
  *aTokFWVersion = ToNewUnicode(mTokenFWVersion);
  if (!*aTokFWVersion) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring tokenSerialNumber; */
NS_IMETHODIMP nsPK11Token::GetTokenSerialNumber(PRUnichar **aTokSerialNum)
{
  // handle removals/insertions
  if (mSeries != PK11_GetSlotSeries(mSlot)) {
    refreshTokenInfo();
  }
  *aTokSerialNum = ToNewUnicode(mTokenSerialNum);
  if (!*aTokSerialNum) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* boolean isLoggedIn (); */
NS_IMETHODIMP nsPK11Token::IsLoggedIn(bool *_retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = NS_OK;

  *_retval = PK11_IsLoggedIn(mSlot, 0);

  return rv;
}

/* void logout (in boolean force); */
NS_IMETHODIMP 
nsPK11Token::Login(bool force)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv;
  SECStatus srv;
  bool test;
  rv = this->NeedsLogin(&test);
  if (NS_FAILED(rv)) return rv;
  if (test && force) {
    rv = this->LogoutSimple();
    if (NS_FAILED(rv)) return rv;
  }
  rv = setPassword(mSlot, mUIContext);
  if (NS_FAILED(rv)) return rv;
  srv = PK11_Authenticate(mSlot, true, mUIContext);
  return (srv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPK11Token::LogoutSimple()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  // PK11_MapError sets CKR_USER_NOT_LOGGED_IN to SEC_ERROR_LIBRARY_FAILURE,
  // so not going to learn anything here by a failure.  Treat it like void.
  PK11_Logout(mSlot);
  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::LogoutAndDropAuthenticatedResources()
{
  nsresult rv = LogoutSimple();

  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  return nssComponent->LogoutAuthenticatedPK11();
}

/* void reset (); */
NS_IMETHODIMP nsPK11Token::Reset()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PK11_ResetToken(mSlot, 0);
  return NS_OK;
}

/* readonly attribute long minimumPasswordLength; */
NS_IMETHODIMP nsPK11Token::GetMinimumPasswordLength(int32_t *aMinimumPasswordLength)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *aMinimumPasswordLength = PK11_GetMinimumPwdLength(mSlot);

  return NS_OK;
}

/* readonly attribute boolean needsUserInit; */
NS_IMETHODIMP nsPK11Token::GetNeedsUserInit(bool *aNeedsUserInit)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *aNeedsUserInit = PK11_NeedUserInit(mSlot);
  return NS_OK;
}

/* boolean checkPassword (in wstring password); */
NS_IMETHODIMP nsPK11Token::CheckPassword(const PRUnichar *password, bool *_retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  SECStatus srv;
  int32_t prerr;
  NS_ConvertUTF16toUTF8 aUtf8Password(password);
  srv = PK11_CheckUserPassword(mSlot, 
                  const_cast<char *>(aUtf8Password.get()));
  if (srv != SECSuccess) {
    *_retval =  false;
    prerr = PR_GetError();
    if (prerr != SEC_ERROR_BAD_PASSWORD) {
      /* something really bad happened - throw an exception */
      return NS_ERROR_FAILURE;
    }
  } else {
    *_retval =  true;
  }
  return NS_OK;
}

/* void initPassword (in wstring initialPassword); */
NS_IMETHODIMP nsPK11Token::InitPassword(const PRUnichar *initialPassword)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

    nsresult rv = NS_OK;
    SECStatus status;

    NS_ConvertUTF16toUTF8 aUtf8InitialPassword(initialPassword);
    status = PK11_InitPin(mSlot, "", const_cast<char*>(aUtf8InitialPassword.get()));
    if (status == SECFailure) { rv = NS_ERROR_FAILURE; goto done; }

done:
    return rv;
}

/* long getAskPasswordTimes(); */
NS_IMETHODIMP 
nsPK11Token::GetAskPasswordTimes(int32_t *rvAskTimes)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

    int askTimes, askTimeout;
    PK11_GetSlotPWValues(mSlot, &askTimes, &askTimeout);
    *rvAskTimes = askTimes;
    return NS_OK;
}

/* long getAskPasswordTimeout(); */
NS_IMETHODIMP 
nsPK11Token::GetAskPasswordTimeout(int32_t *rvAskTimeout)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

    int askTimes, askTimeout;
    PK11_GetSlotPWValues(mSlot, &askTimes, &askTimeout);
    *rvAskTimeout = askTimeout;
    return NS_OK;
}

/* void setAskPasswordDefaults(in unsigned long askTimes,
 *                             in unsigned long timeout);
 */
NS_IMETHODIMP 
nsPK11Token::SetAskPasswordDefaults(const int32_t askTimes,
                                    const int32_t askTimeout)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

    PK11_SetSlotPWValues(mSlot, askTimes, askTimeout);
    return NS_OK;
}

/* void changePassword (in wstring oldPassword, in wstring newPassword); */
NS_IMETHODIMP nsPK11Token::ChangePassword(const PRUnichar *oldPassword, const PRUnichar *newPassword)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  SECStatus rv;
  NS_ConvertUTF16toUTF8 aUtf8OldPassword(oldPassword);
  NS_ConvertUTF16toUTF8 aUtf8NewPassword(newPassword);

  rv = PK11_ChangePW(mSlot, 
         (oldPassword ? const_cast<char *>(aUtf8OldPassword.get()) : nullptr),
         (newPassword ? const_cast<char *>(aUtf8NewPassword.get()) : nullptr));
  return (rv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

/* boolean isHardwareToken (); */
NS_IMETHODIMP nsPK11Token::IsHardwareToken(bool *_retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = NS_OK;

  *_retval = PK11_IsHW(mSlot);

  return rv;
}

/* boolean needsLogin (); */
NS_IMETHODIMP nsPK11Token::NeedsLogin(bool *_retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = NS_OK;

  *_retval = PK11_NeedLogin(mSlot);

  return rv;
}

/* boolean isFriendly (); */
NS_IMETHODIMP nsPK11Token::IsFriendly(bool *_retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = NS_OK;

  *_retval = PK11_IsFriendly(mSlot);

  return rv;
}

/*=========================================================*/

NS_IMPL_ISUPPORTS1(nsPK11TokenDB, nsIPK11TokenDB)

nsPK11TokenDB::nsPK11TokenDB()
{
  /* member initializers and constructor code */
}

nsPK11TokenDB::~nsPK11TokenDB()
{
  /* destructor code */
}

/* nsIPK11Token getInternalKeyToken (); */
NS_IMETHODIMP nsPK11TokenDB::GetInternalKeyToken(nsIPK11Token **_retval)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  PK11SlotInfo *slot = 0;
  nsCOMPtr<nsIPK11Token> token;

  slot = PK11_GetInternalKeySlot();
  if (!slot) { rv = NS_ERROR_FAILURE; goto done; }

  token = new nsPK11Token(slot);
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
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  PK11SlotInfo *slot = 0;
  NS_ConvertUTF16toUTF8 aUtf8TokenName(tokenName);
  slot = PK11_FindSlotByName(const_cast<char*>(aUtf8TokenName.get()));
  if (!slot) { rv = NS_ERROR_FAILURE; goto done; }

  *_retval = new nsPK11Token(slot);
  NS_ADDREF(*_retval);

done:
  if (slot) PK11_FreeSlot(slot);
  return rv;
}

/* nsIEnumerator listTokens (); */
NS_IMETHODIMP nsPK11TokenDB::ListTokens(nsIEnumerator* *_retval)
{
  nsNSSShutDownPreventionLock locker;
  nsCOMPtr<nsISupportsArray> array;
  PK11SlotList *list = 0;
  PK11SlotListElement *le;

  *_retval = nullptr;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) { goto done; }

  /* List all tokens, creating PK11Token objects and putting them
   * into the array.
   */
  list = PK11_GetAllTokens(CKM_INVALID_MECHANISM, false, false, 0);
  if (!list) { rv = NS_ERROR_FAILURE; goto done; }

  for (le = PK11_GetFirstSafe(list); le; le = PK11_GetNextSafe(list, le, false)) {
    nsCOMPtr<nsIPK11Token> token = new nsPK11Token(le->slot);
    rv = array->AppendElement(token);
    if (NS_FAILED(rv)) {
      PK11_FreeSlotListElement(list, le);
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
  }

  rv = array->Enumerate(_retval);

done:
  if (list) PK11_FreeSlotList(list);
  return rv;
}

