/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <softpub.h>
#include <wintrust.h>

#include "certificatecheck.h"
#include "servicebase.h"

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

static const int ENCODING = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

/**
 * Checks to see if a file stored at filePath matches the specified info.
 *
 * @param  filePath    The PE file path to check
 * @param  infoToMatch The acceptable information to match
 * @return ERROR_SUCCESS if successful, ERROR_NOT_FOUND if the info 
 *         does not match, or the last error otherwise.
 */
DWORD
CheckCertificateForPEFile(LPCWSTR filePath, 
                          CertificateCheckInfo &infoToMatch)
{
  HCERTSTORE certStore = nullptr;
  HCRYPTMSG cryptMsg = nullptr; 
  PCCERT_CONTEXT certContext = nullptr;
  PCMSG_SIGNER_INFO signerInfo = nullptr;
  DWORD lastError = ERROR_SUCCESS;

  // Get the HCERTSTORE and HCRYPTMSG from the signed file.
  DWORD encoding, contentType, formatType;
  BOOL result = CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                                  filePath, 
                                  CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                  CERT_QUERY_CONTENT_FLAG_ALL, 
                                  0, &encoding, &contentType,
                                  &formatType, &certStore, &cryptMsg, nullptr);
  if (!result) {
    lastError = GetLastError();
    LOG_WARN(("CryptQueryObject failed.  (%d)", lastError));
    goto cleanup;
  }

  // Pass in nullptr to get the needed signer information size.
  DWORD signerInfoSize;
  result = CryptMsgGetParam(cryptMsg, CMSG_SIGNER_INFO_PARAM, 0, 
                            nullptr, &signerInfoSize);
  if (!result) {
    lastError = GetLastError();
    LOG_WARN(("CryptMsgGetParam failed.  (%d)", lastError));
    goto cleanup;
  }

  // Allocate the needed size for the signer information.
  signerInfo = (PCMSG_SIGNER_INFO)LocalAlloc(LPTR, signerInfoSize);
  if (!signerInfo) {
    lastError = GetLastError();
    LOG_WARN(("Unable to allocate memory for Signer Info.  (%d)", lastError));
    goto cleanup;
  }

  // Get the signer information (PCMSG_SIGNER_INFO).
  // In particular we want the issuer and serial number.
  result = CryptMsgGetParam(cryptMsg, CMSG_SIGNER_INFO_PARAM, 0, 
                            (PVOID)signerInfo, &signerInfoSize);
  if (!result) {
    lastError = GetLastError();
    LOG_WARN(("CryptMsgGetParam failed.  (%d)", lastError));
    goto cleanup;
  }

  // Search for the signer certificate in the certificate store.
  CERT_INFO certInfo;     
  certInfo.Issuer = signerInfo->Issuer;
  certInfo.SerialNumber = signerInfo->SerialNumber;
  certContext = CertFindCertificateInStore(certStore, ENCODING, 0, 
                                           CERT_FIND_SUBJECT_CERT,
                                           (PVOID)&certInfo, nullptr);
  if (!certContext) {
    lastError = GetLastError();
    LOG_WARN(("CertFindCertificateInStore failed.  (%d)", lastError));
    goto cleanup;
  }

  if (!DoCertificateAttributesMatch(certContext, infoToMatch)) {
    lastError = ERROR_NOT_FOUND;
    LOG_WARN(("Certificate did not match issuer or name.  (%d)", lastError));
    goto cleanup;
  }

cleanup:
  if (signerInfo) {
    LocalFree(signerInfo);
  }
  if (certContext) {
    CertFreeCertificateContext(certContext);
  }
  if (certStore) { 
    CertCloseStore(certStore, 0);
  }
  if (cryptMsg) { 
    CryptMsgClose(cryptMsg);
  }
  return lastError;
}

/**
 * Checks to see if a file stored at filePath matches the specified info.
 *
 * @param  certContext  The certificate context of the file
 * @param  infoToMatch  The acceptable information to match
 * @return FALSE if the info does not match or if any error occurs in the check
 */
BOOL 
DoCertificateAttributesMatch(PCCERT_CONTEXT certContext, 
                             CertificateCheckInfo &infoToMatch)
{
  DWORD dwData;
  LPTSTR szName = nullptr;

  if (infoToMatch.issuer) {
    // Pass in nullptr to get the needed size of the issuer buffer.
    dwData = CertGetNameString(certContext, 
                               CERT_NAME_SIMPLE_DISPLAY_TYPE,
                               CERT_NAME_ISSUER_FLAG, nullptr,
                               nullptr, 0);

    if (!dwData) {
      LOG_WARN(("CertGetNameString failed.  (%d)", GetLastError()));
      return FALSE;
    }

    // Allocate memory for Issuer name buffer.
    LPTSTR szName = (LPTSTR)LocalAlloc(LPTR, dwData * sizeof(WCHAR));
    if (!szName) {
      LOG_WARN(("Unable to allocate memory for issuer name.  (%d)",
                GetLastError()));
      return FALSE;
    }

    // Get Issuer name.
    if (!CertGetNameString(certContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                           CERT_NAME_ISSUER_FLAG, nullptr, szName, dwData)) {
      LOG_WARN(("CertGetNameString failed.  (%d)", GetLastError()));
      LocalFree(szName);
      return FALSE;
    }

    // If the issuer does not match, return a failure.
    if (!infoToMatch.issuer ||
        wcscmp(szName, infoToMatch.issuer)) {
      LocalFree(szName);
      return FALSE;
    }

    LocalFree(szName);
    szName = nullptr;
  }

  if (infoToMatch.name) {
    // Pass in nullptr to get the needed size of the name buffer.
    dwData = CertGetNameString(certContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                               0, nullptr, nullptr, 0);
    if (!dwData) {
      LOG_WARN(("CertGetNameString failed.  (%d)", GetLastError()));
      return FALSE;
    }

    // Allocate memory for the name buffer.
    szName = (LPTSTR)LocalAlloc(LPTR, dwData * sizeof(WCHAR));
    if (!szName) {
      LOG_WARN(("Unable to allocate memory for subject name.  (%d)",
                GetLastError()));
      return FALSE;
    }

    // Obtain the name.
    if (!(CertGetNameString(certContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0,
                            nullptr, szName, dwData))) {
      LOG_WARN(("CertGetNameString failed.  (%d)", GetLastError()));
      LocalFree(szName);
      return FALSE;
    }

    // If the issuer does not match, return a failure.
    if (!infoToMatch.name || 
        wcscmp(szName, infoToMatch.name)) {
      LocalFree(szName);
      return FALSE;
    }

    // We have a match!
    LocalFree(szName);
  }

  // If there were any errors we would have aborted by now.
  return TRUE;
}

/**
 * Duplicates the specified string
 *
 * @param  inputString The string to duplicate
 * @return The duplicated string which should be freed by the caller.
 */
LPWSTR 
AllocateAndCopyWideString(LPCWSTR inputString)
{
  LPWSTR outputString = 
    (LPWSTR)LocalAlloc(LPTR, (wcslen(inputString) + 1) * sizeof(WCHAR));
  if (outputString) {
    lstrcpyW(outputString, inputString);
  }
  return outputString;
}

/**
 * Verifies the trust of the specified file path.
 *
 * @param  filePath  The file path to check.
 * @return ERROR_SUCCESS if successful, or the last error code otherwise.
 */
DWORD
VerifyCertificateTrustForFile(LPCWSTR filePath)
{
  // Setup the file to check.
  WINTRUST_FILE_INFO fileToCheck;
  ZeroMemory(&fileToCheck, sizeof(fileToCheck));
  fileToCheck.cbStruct = sizeof(WINTRUST_FILE_INFO);
  fileToCheck.pcwszFilePath = filePath;

  // Setup what to check, we want to check it is signed and trusted.
  WINTRUST_DATA trustData;
  ZeroMemory(&trustData, sizeof(trustData));
  trustData.cbStruct = sizeof(trustData);
  trustData.pPolicyCallbackData = nullptr;
  trustData.pSIPClientData = nullptr;
  trustData.dwUIChoice = WTD_UI_NONE;
  trustData.fdwRevocationChecks = WTD_REVOKE_NONE; 
  trustData.dwUnionChoice = WTD_CHOICE_FILE;
  trustData.dwStateAction = 0;
  trustData.hWVTStateData = nullptr;
  trustData.pwszURLReference = nullptr;
  // no UI
  trustData.dwUIContext = 0;
  trustData.pFile = &fileToCheck;

  GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  // Check if the file is signed by something that is trusted.
  LONG ret = WinVerifyTrust(nullptr, &policyGUID, &trustData);
  if (ERROR_SUCCESS == ret) {
    // The hash that represents the subject is trusted and there were no
    // verification errors.  No publisher nor time stamp chain errors.
    LOG(("The file \"%ls\" is signed and the signature was verified.",
         filePath));
      return ERROR_SUCCESS;
  }

  DWORD lastError = GetLastError();
  LOG_WARN(("There was an error validating trust of the certificate for file"
            " \"%ls\". Returned: %d.  (%d)", filePath, ret, lastError));
  return ret;
}
