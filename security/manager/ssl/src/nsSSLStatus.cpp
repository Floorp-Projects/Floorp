/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
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

#include "nsSSLStatus.h"
#include "plstr.h"

NS_IMETHODIMP
nsSSLStatus::GetServerCert(nsIX509Cert** _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

  *_result = mServerCert;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetKeyLength(PRUint32* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");
  if (!mHaveKeyLengthAndCipher)
    return NS_ERROR_NOT_AVAILABLE;

  *_result = mKeyLength;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetSecretKeyLength(PRUint32* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");
  if (!mHaveKeyLengthAndCipher)
    return NS_ERROR_NOT_AVAILABLE;

  *_result = mSecretKeyLength;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetCipherName(char** _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");
  if (!mHaveKeyLengthAndCipher)
    return NS_ERROR_NOT_AVAILABLE;

  *_result = PL_strdup(mCipherName.get());

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetIsDomainMismatch(PRBool* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");
  if (!mHaveCertStatus)
    return NS_ERROR_NOT_AVAILABLE;

  *_result = mIsDomainMismatch;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetIsNotValidAtThisTime(PRBool* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");
  if (!mHaveCertStatus)
    return NS_ERROR_NOT_AVAILABLE;

  *_result = mIsNotValidAtThisTime;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetIsUntrusted(PRBool* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");
  if (!mHaveCertStatus)
    return NS_ERROR_NOT_AVAILABLE;

  *_result = mIsUntrusted;

  return NS_OK;
}

nsSSLStatus::nsSSLStatus()
: mKeyLength(0), mSecretKeyLength(0)
, mIsDomainMismatch(PR_FALSE)
, mIsNotValidAtThisTime(PR_FALSE)
, mIsUntrusted(PR_FALSE)
, mHaveKeyLengthAndCipher(PR_FALSE)
, mHaveCertStatus(PR_FALSE)
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSSLStatus, nsISSLStatus)

nsSSLStatus::~nsSSLStatus()
{
}
