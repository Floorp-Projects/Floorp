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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kaie@netscape.com>
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

#include "nsCipherInfo.h"
#include "nsReadableUtils.h"
#include "nsEnumeratorUtils.h"
#include "nsCRT.h"
#include "nsNSSComponent.h"
#include "ssl.h"
#include "sslproto.h"

NS_IMPL_ISUPPORTS1(nsCipherInfoService, nsICipherInfoService)

nsCipherInfoService::nsCipherInfoService()
{
}

nsCipherInfoService::~nsCipherInfoService()
{
}

NS_IMETHODIMP nsCipherInfoService::GetCipherInfoByPrefString(const nsACString &aPrefString, nsICipherInfo * *aCipherInfo)
{
  NS_ENSURE_ARG_POINTER(aCipherInfo);

  PRUint16 cipher_id = 0;
  nsresult rv = nsNSSComponent::GetNSSCipherIDFromPrefString(aPrefString, cipher_id);
  if (NS_FAILED(rv))
    return rv;

  *aCipherInfo = new nsCipherInfo(cipher_id);
  NS_IF_ADDREF(*aCipherInfo);
  rv = *aCipherInfo != nsnull ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  return rv;
}

NS_IMPL_ISUPPORTS1(nsCipherInfo, nsICipherInfo)

nsCipherInfo::nsCipherInfo(PRUint16 aCipherId)
:mHaveInfo(PR_FALSE)
{
  for (PRUint16 i = 0; i < SSL_NumImplementedCiphers; ++i)
  {
    const PRUint16 i_id = SSL_ImplementedCiphers[i];
    if (i_id != aCipherId)
      continue;
  
    PRBool isGood = (
      (SECSuccess == SSL_GetCipherSuiteInfo(i_id, &mInfo, sizeof(mInfo)))
      &&
      (sizeof(mInfo) == mInfo.length));

    if (!isGood)
    {
      NS_ASSERTION(0, "unable to get info for implemented cipher");
      continue;
    }
    
    mHaveInfo = PR_TRUE;
  }
}

nsCipherInfo::~nsCipherInfo()
{
}

NS_IMETHODIMP nsCipherInfo::GetLongName(nsACString &aLongName)
{
  if (!mHaveInfo)
    return NS_ERROR_NOT_AVAILABLE;

  aLongName = ToNewCString(nsDependentCString(mInfo.cipherSuiteName));
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetIsSSL2(PRBool *aIsSSL2)
{
  NS_ENSURE_ARG_POINTER(aIsSSL2);

  if (!mHaveInfo)
    return NS_ERROR_NOT_AVAILABLE;

  *aIsSSL2 = SSL_IS_SSL2_CIPHER(mInfo.cipherSuite);
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetIsFIPS(PRBool *aIsFIPS)
{
  NS_ENSURE_ARG_POINTER(aIsFIPS);

  if (!mHaveInfo)
    return NS_ERROR_NOT_AVAILABLE;

  *aIsFIPS = mInfo.isFIPS;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetIsExportable(PRBool *aIsExportable)
{
  NS_ENSURE_ARG_POINTER(aIsExportable);

  if (!mHaveInfo)
    return NS_ERROR_NOT_AVAILABLE;

  *aIsExportable = mInfo.isExportable;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetNonStandard(PRBool *aNonStandard)
{
  NS_ENSURE_ARG_POINTER(aNonStandard);

  if (!mHaveInfo)
    return NS_ERROR_NOT_AVAILABLE;

  *aNonStandard = mInfo.nonStandard;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetSymCipherName(nsACString &aSymCipherName)
{
  if (!mHaveInfo)
    return NS_ERROR_NOT_AVAILABLE;

  aSymCipherName = mInfo.symCipherName;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetAuthAlgorithmName(nsACString &aAuthAlgorithmName)
{
  if (!mHaveInfo)
    return NS_ERROR_NOT_AVAILABLE;

  aAuthAlgorithmName = mInfo.authAlgorithmName;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetKeaTypeName(nsACString &aKeaTypeName)
{
  if (!mHaveInfo)
    return NS_ERROR_NOT_AVAILABLE;

  aKeaTypeName = mInfo.keaTypeName;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetMacAlgorithmName(nsACString &aMacAlgorithmName)
{
  if (!mHaveInfo)
    return NS_ERROR_NOT_AVAILABLE;

  aMacAlgorithmName = mInfo.macAlgorithmName;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetEffectiveKeyBits(PRInt32 *aEffectiveKeyBits)
{
  NS_ENSURE_ARG_POINTER(aEffectiveKeyBits);

  if (!mHaveInfo)
    return NS_ERROR_NOT_AVAILABLE;

  *aEffectiveKeyBits = mInfo.effectiveKeyBits;
  return NS_OK;
}
