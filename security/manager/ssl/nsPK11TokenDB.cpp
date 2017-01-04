/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsPK11TokenDB.h"

#include <string.h>

#include "ScopedNSSTypes.h"
#include "mozilla/Casting.h"
#include "mozilla/Unused.h"
#include "nsIMutableArray.h"
#include "nsISupports.h"
#include "nsNSSComponent.h"
#include "nsPromiseFlatString.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "prerror.h"
#include "secerr.h"

extern mozilla::LazyLogModule gPIPNSSLog;

NS_IMPL_ISUPPORTS(nsPK11Token, nsIPK11Token)

nsPK11Token::nsPK11Token(PK11SlotInfo* slot)
  : mUIContext(new PipUIContext())
{
  MOZ_ASSERT(slot);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  mSlot.reset(PK11_ReferenceSlot(slot));
  mSeries = PK11_GetSlotSeries(slot);

  Unused << refreshTokenInfo(locker);
}

nsresult
nsPK11Token::refreshTokenInfo(const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  mTokenName = PK11_GetTokenName(mSlot.get());

  CK_TOKEN_INFO tokInfo;
  nsresult rv = MapSECStatus(PK11_GetTokenInfo(mSlot.get(), &tokInfo));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Set the Label field
  const char* ccLabel = mozilla::BitwiseCast<char*, CK_UTF8CHAR*>(tokInfo.label);
  mTokenLabel.Assign(ccLabel, strnlen(ccLabel, sizeof(tokInfo.label)));
  mTokenLabel.Trim(" ", false, true);

  // Set the Manufacturer field
  const char* ccManID =
    mozilla::BitwiseCast<char*, CK_UTF8CHAR*>(tokInfo.manufacturerID);
  mTokenManufacturerID.Assign(
    ccManID,
    strnlen(ccManID, sizeof(tokInfo.manufacturerID)));
  mTokenManufacturerID.Trim(" ", false, true);

  // Set the Hardware Version field
  mTokenHWVersion.Truncate();
  mTokenHWVersion.AppendInt(tokInfo.hardwareVersion.major);
  mTokenHWVersion.Append('.');
  mTokenHWVersion.AppendInt(tokInfo.hardwareVersion.minor);

  // Set the Firmware Version field
  mTokenFWVersion.Truncate();
  mTokenFWVersion.AppendInt(tokInfo.firmwareVersion.major);
  mTokenFWVersion.Append('.');
  mTokenFWVersion.AppendInt(tokInfo.firmwareVersion.minor);

  // Set the Serial Number field
  const char* ccSerial =
    mozilla::BitwiseCast<char*, CK_CHAR*>(tokInfo.serialNumber);
  mTokenSerialNum.Assign(ccSerial,
                         strnlen(ccSerial, sizeof(tokInfo.serialNumber)));
  mTokenSerialNum.Trim(" ", false, true);

  return NS_OK;
}

nsPK11Token::~nsPK11Token()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(ShutdownCalledFrom::Object);
}

void
nsPK11Token::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void
nsPK11Token::destructorSafeDestroyNSSReference()
{
  mSlot = nullptr;
}

nsresult
nsPK11Token::GetAttributeHelper(const nsACString& attribute,
                        /*out*/ nsACString& xpcomOutParam)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Handle removals/insertions.
  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshTokenInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  xpcomOutParam = attribute;
  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::GetTokenName(/*out*/ nsACString& tokenName)
{
  return GetAttributeHelper(mTokenName, tokenName);
}

NS_IMETHODIMP
nsPK11Token::GetTokenLabel(/*out*/ nsACString& tokenLabel)
{
  return GetAttributeHelper(mTokenLabel, tokenLabel);
}

NS_IMETHODIMP
nsPK11Token::GetTokenManID(/*out*/ nsACString& tokenManufacturerID)
{
  return GetAttributeHelper(mTokenManufacturerID, tokenManufacturerID);
}

NS_IMETHODIMP
nsPK11Token::GetTokenHWVersion(/*out*/ nsACString& tokenHWVersion)
{
  return GetAttributeHelper(mTokenHWVersion, tokenHWVersion);
}

NS_IMETHODIMP
nsPK11Token::GetTokenFWVersion(/*out*/ nsACString& tokenFWVersion)
{
  return GetAttributeHelper(mTokenFWVersion, tokenFWVersion);
}

NS_IMETHODIMP
nsPK11Token::GetTokenSerialNumber(/*out*/ nsACString& tokenSerialNum)
{
  return GetAttributeHelper(mTokenSerialNum, tokenSerialNum);
}

NS_IMETHODIMP
nsPK11Token::IsLoggedIn(bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = PK11_IsLoggedIn(mSlot.get(), 0);

  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::Login(bool force)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv;
  bool test;
  rv = this->NeedsLogin(&test);
  if (NS_FAILED(rv)) return rv;
  if (test && force) {
    rv = this->LogoutSimple();
    if (NS_FAILED(rv)) return rv;
  }
  rv = setPassword(mSlot.get(), mUIContext, locker);
  if (NS_FAILED(rv)) return rv;

  return MapSECStatus(PK11_Authenticate(mSlot.get(), true, mUIContext));
}

NS_IMETHODIMP
nsPK11Token::LogoutSimple()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  // PK11_Logout() can fail if the user wasn't logged in beforehand. We want
  // this method to succeed even in this case, so we ignore the return value.
  Unused << PK11_Logout(mSlot.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::LogoutAndDropAuthenticatedResources()
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsresult rv = LogoutSimple();

  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  return nssComponent->LogoutAuthenticatedPK11();
}

NS_IMETHODIMP
nsPK11Token::Reset()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  return MapSECStatus(PK11_ResetToken(mSlot.get(), nullptr));
}

NS_IMETHODIMP
nsPK11Token::GetMinimumPasswordLength(int32_t* aMinimumPasswordLength)
{
  NS_ENSURE_ARG_POINTER(aMinimumPasswordLength);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *aMinimumPasswordLength = PK11_GetMinimumPwdLength(mSlot.get());

  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::GetNeedsUserInit(bool* aNeedsUserInit)
{
  NS_ENSURE_ARG_POINTER(aNeedsUserInit);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *aNeedsUserInit = PK11_NeedUserInit(mSlot.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::CheckPassword(const nsACString& password, bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  SECStatus srv =
    PK11_CheckUserPassword(mSlot.get(), PromiseFlatCString(password).get());
  if (srv != SECSuccess) {
    *_retval =  false;
    PRErrorCode error = PR_GetError();
    if (error != SEC_ERROR_BAD_PASSWORD) {
      /* something really bad happened - throw an exception */
      return mozilla::psm::GetXPCOMFromNSSError(error);
    }
  } else {
    *_retval =  true;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::InitPassword(const nsACString& initialPassword)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  return MapSECStatus(
    PK11_InitPin(mSlot.get(), "", PromiseFlatCString(initialPassword).get()));
}

NS_IMETHODIMP
nsPK11Token::GetAskPasswordTimes(int32_t* askTimes)
{
  NS_ENSURE_ARG_POINTER(askTimes);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  int askTimeout;
  PK11_GetSlotPWValues(mSlot.get(), askTimes, &askTimeout);
  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::GetAskPasswordTimeout(int32_t* askTimeout)
{
  NS_ENSURE_ARG_POINTER(askTimeout);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  int askTimes;
  PK11_GetSlotPWValues(mSlot.get(), &askTimes, askTimeout);
  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::SetAskPasswordDefaults(const int32_t askTimes,
                                    const int32_t askTimeout)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PK11_SetSlotPWValues(mSlot.get(), askTimes, askTimeout);
  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::ChangePassword(const nsACString& oldPassword,
                            const nsACString& newPassword)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  // PK11_ChangePW() has different semantics for the empty string and for
  // nullptr. In order to support this difference, we need to check IsVoid() to
  // find out if our caller supplied null/undefined args or just empty strings.
  // See Bug 447589.
  return MapSECStatus(PK11_ChangePW(
    mSlot.get(),
    oldPassword.IsVoid() ? nullptr : PromiseFlatCString(oldPassword).get(),
    newPassword.IsVoid() ? nullptr : PromiseFlatCString(newPassword).get()));
}

NS_IMETHODIMP
nsPK11Token::GetHasPassword(bool* hasPassword)
{
  NS_ENSURE_ARG_POINTER(hasPassword);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // PK11_NeedLogin returns true if the token is currently configured to require
  // the user to log in (whether or not the user is actually logged in makes no
  // difference).
  *hasPassword = PK11_NeedLogin(mSlot.get()) && !PK11_NeedUserInit(mSlot.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::IsHardwareToken(bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = PK11_IsHW(mSlot.get());

  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::NeedsLogin(bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = PK11_NeedLogin(mSlot.get());

  return NS_OK;
}

NS_IMETHODIMP
nsPK11Token::IsFriendly(bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = PK11_IsFriendly(mSlot.get());

  return NS_OK;
}

/*=========================================================*/

NS_IMPL_ISUPPORTS(nsPK11TokenDB, nsIPK11TokenDB)

nsPK11TokenDB::nsPK11TokenDB()
{
}

nsPK11TokenDB::~nsPK11TokenDB()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }

  shutdown(ShutdownCalledFrom::Object);
}

NS_IMETHODIMP
nsPK11TokenDB::GetInternalKeyToken(nsIPK11Token** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPK11Token> token = new nsPK11Token(slot.get());
  token.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
nsPK11TokenDB::FindTokenByName(const nsACString& tokenName,
                       /*out*/ nsIPK11Token** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (tokenName.IsEmpty()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  UniquePK11SlotInfo slot(
    PK11_FindSlotByName(PromiseFlatCString(tokenName).get()));
  if (!slot) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPK11Token> token = new nsPK11Token(slot.get());
  token.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
nsPK11TokenDB::ListTokens(nsISimpleEnumerator** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!array) {
    return NS_ERROR_FAILURE;
  }

  *_retval = nullptr;

  UniquePK11SlotList list(
    PK11_GetAllTokens(CKM_INVALID_MECHANISM, false, false, 0));
  if (!list) {
    return NS_ERROR_FAILURE;
  }

  for (PK11SlotListElement* le = PK11_GetFirstSafe(list.get()); le;
       le = PK11_GetNextSafe(list.get(), le, false)) {
    nsCOMPtr<nsIPK11Token> token = new nsPK11Token(le->slot);
    nsresult rv = array->AppendElement(token, false);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return array->Enumerate(_retval);
}
