/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsPK11TokenDB.h"

#include "mozilla/unused.h"
#include "nsIMutableArray.h"
#include "nsISupports.h"
#include "nsNSSComponent.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "prerror.h"
#include "ScopedNSSTypes.h"
#include "secerr.h"

extern mozilla::LazyLogModule gPIPNSSLog;

NS_IMPL_ISUPPORTS(nsPK11Token, nsIPK11Token)

nsPK11Token::nsPK11Token(PK11SlotInfo *slot)
  : mUIContext(new PipUIContext())
{
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
  mTokenName = NS_ConvertUTF8toUTF16(PK11_GetTokenName(mSlot.get()));

  CK_TOKEN_INFO tokInfo;
  nsresult rv = MapSECStatus(PK11_GetTokenInfo(mSlot.get(), &tokInfo));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Set the Label field
  const char* ccLabel = reinterpret_cast<const char*>(tokInfo.label);
  const nsACString& cLabel = Substring(
    ccLabel,
    ccLabel + PL_strnlen(ccLabel, sizeof(tokInfo.label)));
  mTokenLabel = NS_ConvertUTF8toUTF16(cLabel);
  mTokenLabel.Trim(" ", false, true);

  // Set the Manufacturer field
  const char* ccManID = reinterpret_cast<const char*>(tokInfo.manufacturerID);
  const nsACString& cManID = Substring(
    ccManID,
    ccManID + PL_strnlen(ccManID, sizeof(tokInfo.manufacturerID)));
  mTokenManID = NS_ConvertUTF8toUTF16(cManID);
  mTokenManID.Trim(" ", false, true);

  // Set the Hardware Version field
  mTokenHWVersion.AppendInt(tokInfo.hardwareVersion.major);
  mTokenHWVersion.Append('.');
  mTokenHWVersion.AppendInt(tokInfo.hardwareVersion.minor);

  // Set the Firmware Version field
  mTokenFWVersion.AppendInt(tokInfo.firmwareVersion.major);
  mTokenFWVersion.Append('.');
  mTokenFWVersion.AppendInt(tokInfo.firmwareVersion.minor);

  // Set the Serial Number field
  const char* ccSerial = reinterpret_cast<const char*>(tokInfo.serialNumber);
  const nsACString& cSerial = Substring(
    ccSerial,
    ccSerial + PL_strnlen(ccSerial, sizeof(tokInfo.serialNumber)));
  mTokenSerialNum = NS_ConvertUTF8toUTF16(cSerial);
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
  shutdown(calledFromObject);
}

void nsPK11Token::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsPK11Token::destructorSafeDestroyNSSReference()
{
  mSlot = nullptr;
}

NS_IMETHODIMP nsPK11Token::GetTokenName(char16_t * *aTokenName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // handle removals/insertions
  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshTokenInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  *aTokenName = ToNewUnicode(mTokenName);
  if (!*aTokenName) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::GetTokenLabel(char16_t **aTokLabel)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // handle removals/insertions
  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshTokenInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  *aTokLabel = ToNewUnicode(mTokenLabel);
  if (!*aTokLabel) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::GetTokenManID(char16_t **aTokManID)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // handle removals/insertions
  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshTokenInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  *aTokManID = ToNewUnicode(mTokenManID);
  if (!*aTokManID) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::GetTokenHWVersion(char16_t **aTokHWVersion)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // handle removals/insertions
  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshTokenInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  *aTokHWVersion = ToNewUnicode(mTokenHWVersion);
  if (!*aTokHWVersion) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::GetTokenFWVersion(char16_t **aTokFWVersion)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // handle removals/insertions
  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshTokenInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  *aTokFWVersion = ToNewUnicode(mTokenFWVersion);
  if (!*aTokFWVersion) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::GetTokenSerialNumber(char16_t **aTokSerialNum)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // handle removals/insertions
  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshTokenInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  *aTokSerialNum = ToNewUnicode(mTokenSerialNum);
  if (!*aTokSerialNum) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::IsLoggedIn(bool *_retval)
{
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

NS_IMETHODIMP nsPK11Token::LogoutSimple()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  // PK11_Logout() can fail if the user wasn't logged in beforehand. We want
  // this method to succeed even in this case, so we ignore the return value.
  Unused << PK11_Logout(mSlot.get());
  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::LogoutAndDropAuthenticatedResources()
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

NS_IMETHODIMP nsPK11Token::Reset()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  return MapSECStatus(PK11_ResetToken(mSlot.get(), nullptr));
}

NS_IMETHODIMP nsPK11Token::GetMinimumPasswordLength(int32_t *aMinimumPasswordLength)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *aMinimumPasswordLength = PK11_GetMinimumPwdLength(mSlot.get());

  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::GetNeedsUserInit(bool *aNeedsUserInit)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *aNeedsUserInit = PK11_NeedUserInit(mSlot.get());
  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::CheckPassword(const char16_t *password, bool *_retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ConvertUTF16toUTF8 utf8Password(password);
  SECStatus srv =
    PK11_CheckUserPassword(mSlot.get(), const_cast<char*>(utf8Password.get()));
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

NS_IMETHODIMP nsPK11Token::InitPassword(const char16_t *initialPassword)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ConvertUTF16toUTF8 utf8Password(initialPassword);
  return MapSECStatus(
    PK11_InitPin(mSlot.get(), "", const_cast<char*>(utf8Password.get())));
}

NS_IMETHODIMP
nsPK11Token::GetAskPasswordTimes(int32_t* askTimes)
{
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

NS_IMETHODIMP nsPK11Token::ChangePassword(const char16_t *oldPassword, const char16_t *newPassword)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ConvertUTF16toUTF8 utf8OldPassword(oldPassword);
  NS_ConvertUTF16toUTF8 utf8NewPassword(newPassword);

  // nsCString.get() will return an empty string instead of nullptr even if it
  // was initialized with nullptr. PK11_ChangePW() has different semantics for
  // the empty string and for nullptr, so we can't just use get().
  // See Bug 447589.
  return MapSECStatus(PK11_ChangePW(
    mSlot.get(),
    (oldPassword ? const_cast<char*>(utf8OldPassword.get()) : nullptr),
    (newPassword ? const_cast<char*>(utf8NewPassword.get()) : nullptr)));
}

NS_IMETHODIMP nsPK11Token::IsHardwareToken(bool *_retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = PK11_IsHW(mSlot.get());

  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::NeedsLogin(bool *_retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = PK11_NeedLogin(mSlot.get());

  return NS_OK;
}

NS_IMETHODIMP nsPK11Token::IsFriendly(bool *_retval)
{
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

  shutdown(calledFromObject);
}

NS_IMETHODIMP nsPK11TokenDB::GetInternalKeyToken(nsIPK11Token **_retval)
{
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

NS_IMETHODIMP nsPK11TokenDB::
FindTokenByName(const char16_t* tokenName, nsIPK11Token **_retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ConvertUTF16toUTF8 utf8TokenName(tokenName);
  UniquePK11SlotInfo slot(
    PK11_FindSlotByName(const_cast<char*>(utf8TokenName.get())));
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
