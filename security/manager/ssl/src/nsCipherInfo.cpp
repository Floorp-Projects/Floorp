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
#include "ssl.h"
#include "sslproto.h"

nsCiphers* nsCiphers::singleton = nsnull;

void nsCiphers::InitSingleton()
{
  NS_ASSERTION(!singleton, "trying to instantiate nsCiphers::singleton twice");

  singleton = new nsCiphers();
}

void nsCiphers::DestroySingleton()
{
  delete singleton;
  singleton = nsnull;
}


struct struct_historical_cipher_pref_strings
{
  PRUint16 cipher_id;
  const char *pref_string;
}
  const historical_cipher_pref_strings[] = 
{
  { SSL_EN_RC4_128_WITH_MD5, "security.ssl2.rc4_128" },
  { SSL_EN_RC2_128_CBC_WITH_MD5, "security.ssl2.rc2_128" },
  { SSL_EN_DES_192_EDE3_CBC_WITH_MD5, "security.ssl2.des_ede3_192" },
  { SSL_EN_DES_64_CBC_WITH_MD5, "security.ssl2.des_64" },
  { SSL_EN_RC4_128_EXPORT40_WITH_MD5, "security.ssl2.rc4_40" },
  { SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5, "security.ssl2.rc2_40" },
  { SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA, "security.ssl3.fortezza_fortezza_sha" },
  { SSL_FORTEZZA_DMS_WITH_RC4_128_SHA, "security.ssl3.fortezza_rc4_sha" },
  { SSL_RSA_WITH_RC4_128_MD5, "security.ssl3.rsa_rc4_128_md5" },
  { SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA, "security.ssl3.rsa_fips_des_ede3_sha" },
  { SSL_RSA_WITH_3DES_EDE_CBC_SHA, "security.ssl3.rsa_des_ede3_sha" },
  { SSL_RSA_FIPS_WITH_DES_CBC_SHA, "security.ssl3.rsa_fips_des_sha" },
  { SSL_RSA_WITH_DES_CBC_SHA, "security.ssl3.rsa_des_sha" },
  { TLS_RSA_EXPORT1024_WITH_RC4_56_SHA, "security.ssl3.rsa_1024_rc4_56_sha" },
  { TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA, "security.ssl3.rsa_1024_des_cbc_sha" },
  { SSL_RSA_EXPORT_WITH_RC4_40_MD5, "security.ssl3.rsa_rc4_40_md5" },
  { SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5, "security.ssl3.rsa_rc2_40_md5" },
  { SSL_FORTEZZA_DMS_WITH_NULL_SHA, "security.ssl3.fortezza_null_sha" },
  { SSL_RSA_WITH_NULL_MD5, "security.ssl3.rsa_null_md5" }
};

const PRUint16 number_of_historical_cipher_pref_strings =
  sizeof(historical_cipher_pref_strings)
  / sizeof(struct_historical_cipher_pref_strings);

PRBool isCipherWithHistoricaPrefString(const PRUint16 cipher_id, PRUint16 &out_index_into_array)
{
  for (PRUint16 i = 0; i < number_of_historical_cipher_pref_strings; ++i)
  {
    if (cipher_id == historical_cipher_pref_strings[i].cipher_id)
    {
      out_index_into_array = i;
      return PR_TRUE;
    }
  }
  
  return PR_FALSE;
}

nsCiphers::nsCiphers()
{
  // count number of wanted ciphers
  
  mCiphers = new CipherData[SSL_NumImplementedCiphers];

  if (!mCiphers)
    return;

  for (PRUint16 i = 0; i < SSL_NumImplementedCiphers; ++i)
  {
    CipherData &data = mCiphers[i];
  
    data.id = SSL_ImplementedCiphers[i];
  
    switch (data.id)
    {
      case SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA:
      case SSL_RSA_FIPS_WITH_DES_CBC_SHA:
        // filter out no longer supported ciphers
        data.isWanted = PR_FALSE;
        break;

      case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:
      case SSL_FORTEZZA_DMS_WITH_RC4_128_SHA:
      case SSL_FORTEZZA_DMS_WITH_NULL_SHA:
        // filter out fortezza ciphers until we implement proper UI handling
        data.isWanted = PR_FALSE;
        break;

      default:
        data.isWanted = PR_TRUE;
        break;
    }

    if (!data.isWanted)
      continue;

    // In past versions, there was a hardcoded mapping from cipher IDs
    // to preference strings.
    // In order to be backwards compatible with regards to preferences,
    // we need to continue using those strings.
    // However, we are now using the available ciphers from NSS dynamically,
    // therefore we are using automatic preference string creation for
    // any other ciphers.


    data.isGood = (
      (SECSuccess == SSL_GetCipherSuiteInfo(data.id, &data.info, sizeof(data.info)))
      &&
      (sizeof(data.info) == data.info.length));

    if (!data.isGood)
    {
      NS_ASSERTION(0, "unable to get info for implemented cipher");
      continue;
    }

    PRUint16 array_index = 0;
    if (isCipherWithHistoricaPrefString(data.id, array_index))
    {
      data.prefString = historical_cipher_pref_strings[array_index].pref_string;
      data.isHeapString = PR_FALSE;
    }
    else
    {
      nsCAutoString pref;
      pref.Append("security.");
      pref.Append( SSL_IS_SSL2_CIPHER(data.info.cipherSuite) ? "ssl2." : "ssl3." );
      pref.Append(data.info.cipherSuiteName);
      ToLowerCase(pref);
      data.prefString = ToNewCString(pref);
      data.isHeapString = PR_TRUE;
    }
  }
}

nsCiphers::~nsCiphers()
{
  delete [] mCiphers;
}

void nsCiphers::SetAllCiphersFromPrefs(nsIPref *ipref)
{
  PRBool enabled;
  for (PRUint16 iCipher = 0; iCipher < SSL_NumImplementedCiphers; ++iCipher)
  {
    if (!singleton->mCiphers[iCipher].isWanted || !singleton->mCiphers[iCipher].isGood)
      continue;

    CipherData &cd = singleton->mCiphers[iCipher];

    ipref->GetBoolPref(cd.prefString, &enabled);
    SSL_CipherPrefSetDefault(cd.id, enabled);
  }
}

void nsCiphers::SetCipherFromPref(nsIPref *ipref, const char *prefname)
{
  PRBool enabled;
  for (PRUint16 iCipher = 0; iCipher < SSL_NumImplementedCiphers; ++iCipher)
  {
    if (!singleton->mCiphers[iCipher].isWanted || !singleton->mCiphers[iCipher].isGood)
      continue;

    CipherData &cd = singleton->mCiphers[iCipher];

    // find cipher ID
    if (!nsCRT::strcmp(prefname, cd.prefString))
    {
      ipref->GetBoolPref(cd.prefString, &enabled);
      SSL_CipherPrefSetDefault(cd.id, enabled);
      break;
    }
  }
}

PRBool nsCiphers::IsImplementedCipherWanted(PRUint16 implemented_cipher_index)
{
  NS_ASSERTION(implemented_cipher_index < SSL_NumImplementedCiphers,
    "internal error");
  
  return 
    singleton->mCiphers[implemented_cipher_index].isWanted
    &&
    singleton->mCiphers[implemented_cipher_index].isGood;
}

NS_IMPL_ISUPPORTS1(nsCipherInfoService, nsICipherInfoService)

nsCipherInfoService::nsCipherInfoService()
{
  NS_INIT_ISUPPORTS();
}

nsCipherInfoService::~nsCipherInfoService()
{
}

NS_IMETHODIMP nsCipherInfoService::ListCiphers(nsISimpleEnumerator **_retval)
{
  nsresult rv = NS_OK;

  if (!mArray)
  {
    rv = NS_NewISupportsArray(getter_AddRefs(mArray));
    if (NS_FAILED(rv))
      return rv;

    for (PRUint16 i = 0; i < SSL_NumImplementedCiphers; ++i)
    {
      if (!nsCiphers::IsImplementedCipherWanted(i))
        continue;

      nsCipherInfo *nsCI = nsnull;
      NS_NEWXPCOM(nsCI, nsCipherInfo);
      nsCI->setCipherByImplementedCipherIndex(i);
      mArray->AppendElement(NS_STATIC_CAST(nsICipherInfo*, nsCI));
    }
  }

  return NS_NewArrayEnumerator(_retval, mArray);
}


NS_IMPL_ISUPPORTS1(nsCipherInfo, nsICipherInfo)

nsCipherInfo::nsCipherInfo()
:mIsInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsCipherInfo::~nsCipherInfo()
{
}

void nsCipherInfo::setCipherByImplementedCipherIndex(PRUint16 i)
{
  NS_ASSERTION(i < SSL_NumImplementedCiphers, "internal error");

  mIsInitialized = PR_TRUE;
  mCipherIndex = i;
}

NS_IMETHODIMP nsCipherInfo::GetLongName(char * *aLongName)
{
  NS_ENSURE_ARG_POINTER(aLongName);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  *aLongName = ToNewCString(nsDependentCString(nsCiphers::singleton->mCiphers[mCipherIndex].info.cipherSuiteName));
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetIsSSL2(PRBool *aIsSSL2)
{
  NS_ENSURE_ARG_POINTER(aIsSSL2);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  *aIsSSL2 = SSL_IS_SSL2_CIPHER(nsCiphers::singleton->mCiphers[mCipherIndex].info.cipherSuite);
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetIsFIPS(PRBool *aIsFIPS)
{
  NS_ENSURE_ARG_POINTER(aIsFIPS);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  *aIsFIPS = nsCiphers::singleton->mCiphers[mCipherIndex].info.isFIPS;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetIsExportable(PRBool *aIsExportable)
{
  NS_ENSURE_ARG_POINTER(aIsExportable);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  *aIsExportable = nsCiphers::singleton->mCiphers[mCipherIndex].info.isExportable;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetNonStandard(PRBool *aNonStandard)
{
  NS_ENSURE_ARG_POINTER(aNonStandard);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  *aNonStandard = nsCiphers::singleton->mCiphers[mCipherIndex].info.nonStandard;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetSymCipherName(char * *aSymCipherName)
{
  NS_ENSURE_ARG_POINTER(aSymCipherName);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  *aSymCipherName = ToNewCString(nsDependentCString(nsCiphers::singleton->mCiphers[mCipherIndex].info.symCipherName));
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetAuthAlgorithmName(char * *aAuthAlgorithmName)
{
  NS_ENSURE_ARG_POINTER(aAuthAlgorithmName);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  *aAuthAlgorithmName = ToNewCString(nsDependentCString(nsCiphers::singleton->mCiphers[mCipherIndex].info.authAlgorithmName));
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetKeaTypeName(char * *aKeaTypeName)
{
  NS_ENSURE_ARG_POINTER(aKeaTypeName);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  *aKeaTypeName = ToNewCString(nsDependentCString(nsCiphers::singleton->mCiphers[mCipherIndex].info.keaTypeName));
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetMacAlgorithmName(char * *aMacAlgorithmName)
{
  NS_ENSURE_ARG_POINTER(aMacAlgorithmName);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  *aMacAlgorithmName = ToNewCString(nsDependentCString(nsCiphers::singleton->mCiphers[mCipherIndex].info.macAlgorithmName));
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetEffectiveKeyBits(PRInt32 *aEffectiveKeyBits)
{
  NS_ENSURE_ARG_POINTER(aEffectiveKeyBits);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  *aEffectiveKeyBits = nsCiphers::singleton->mCiphers[mCipherIndex].info.effectiveKeyBits;
  return NS_OK;
}

NS_IMETHODIMP nsCipherInfo::GetPrefString(char * *aPrefString)
{
  NS_ENSURE_ARG_POINTER(aPrefString);

  if (!mIsInitialized || !nsCiphers::singleton->mCiphers[mCipherIndex].isGood)
    return NS_ERROR_NOT_INITIALIZED;

  if (!nsCiphers::singleton->mCiphers[mCipherIndex].isWanted)
  {
    *aPrefString = nsnull;
    return NS_OK;
  }

  *aPrefString = ToNewCString(nsDependentCString(nsCiphers::singleton->mCiphers[mCipherIndex].prefString));
  return NS_OK;
}
