/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <softpub.h>
#include <wintrust.h>

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

#ifdef UNICODE

#ifndef _T
#define __T(x)   L ## x
#define _T(x)    __T(x)
#define _TEXT(x) __T(x)
#endif

#else

#ifndef _T
#define _T(x)    x
#define _TEXT(x) x
#endif

#endif // UNICODE

static const int ENCODING = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[MAX_PATH];
} stack_t;

int popstring(stack_t **stacktop, LPTSTR str, int len);
void pushstring(stack_t **stacktop, LPCTSTR str, int len);

struct CertificateCheckInfo
{
  LPCWSTR name;
  LPCWSTR issuer;
};

/**
 * Checks to see if a file stored at filePath matches the specified info. This
 * only supports the name and issuer attributes currently.
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
  LPTSTR szName = NULL;

  // Pass in NULL to get the needed size of the issuer buffer.
  dwData = CertGetNameString(certContext,
                             CERT_NAME_SIMPLE_DISPLAY_TYPE,
                             CERT_NAME_ISSUER_FLAG, NULL,
                             NULL, 0);

  if (!dwData) {
    return FALSE;
  }

  // Allocate memory for Issuer name buffer.
  szName = (LPTSTR)LocalAlloc(LPTR, dwData * sizeof(WCHAR));
  if (!szName) {
    return FALSE;
  }

  // Get Issuer name.
  if (!CertGetNameString(certContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                         CERT_NAME_ISSUER_FLAG, NULL, szName, dwData)) {
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
  szName = NULL;

  // Pass in NULL to get the needed size of the name buffer.
  dwData = CertGetNameString(certContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                             0, NULL, NULL, 0);
  if (!dwData) {
    return FALSE;
  }

  // Allocate memory for the name buffer.
  szName = (LPTSTR)LocalAlloc(LPTR, dwData * sizeof(WCHAR));
  if (!szName) {
    return FALSE;
  }

  // Obtain the name.
  if (!(CertGetNameString(certContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0,
                          NULL, szName, dwData))) {
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

  // If there were any errors we would have aborted by now.
  return TRUE;
}

/**
 * Checks to see if a file stored at filePath matches the specified info. This
 * only supports the name and issuer attributes currently.
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
  HCERTSTORE certStore = NULL;
  HCRYPTMSG cryptMsg = NULL;
  PCCERT_CONTEXT certContext = NULL;
  PCMSG_SIGNER_INFO signerInfo = NULL;
  DWORD lastError = ERROR_SUCCESS;

  // Get the HCERTSTORE and HCRYPTMSG from the signed file.
  DWORD encoding, contentType, formatType;
  BOOL result = CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                                 filePath,
                                 CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                 CERT_QUERY_CONTENT_FLAG_ALL,
                                 0, &encoding, &contentType,
                                 &formatType, &certStore, &cryptMsg, NULL);
  if (!result) {
    lastError = GetLastError();
    goto cleanup;
  }

  // Pass in NULL to get the needed signer information size.
  DWORD signerInfoSize;
  result = CryptMsgGetParam(cryptMsg, CMSG_SIGNER_INFO_PARAM, 0,
                            NULL, &signerInfoSize);
  if (!result) {
    lastError = GetLastError();
    goto cleanup;
  }

  // Allocate the needed size for the signer information.
  signerInfo = (PCMSG_SIGNER_INFO)LocalAlloc(LPTR, signerInfoSize);
  if (!signerInfo) {
    lastError = GetLastError();
    goto cleanup;
  }

  // Get the signer information (PCMSG_SIGNER_INFO).
  // In particular we want the issuer and serial number.
  result = CryptMsgGetParam(cryptMsg, CMSG_SIGNER_INFO_PARAM, 0,
                            (PVOID)signerInfo, &signerInfoSize);
  if (!result) {
    lastError = GetLastError();
    goto cleanup;
  }

  // Search for the signer certificate in the certificate store.
  CERT_INFO certInfo;
  certInfo.Issuer = signerInfo->Issuer;
  certInfo.SerialNumber = signerInfo->SerialNumber;
  certContext = CertFindCertificateInStore(certStore, ENCODING, 0,
                                           CERT_FIND_SUBJECT_CERT,
                                           (PVOID)&certInfo, NULL);
  if (!certContext) {
    lastError = GetLastError();
    goto cleanup;
  }

  if (!DoCertificateAttributesMatch(certContext, infoToMatch)) {
    lastError = ERROR_NOT_FOUND;
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
 * Compares the certificate name and issuer values for a signed file's with the
 * values provided.
 *
 * @param  stacktop  A pointer to the top of the stack. The stack should contain
 *                   from the top the file's path, the expected value for the
 *                   certificate's name attribute, and the expected value for
 *                   the certificate's issuer attribute.
 * @param  variables A pointer to the NSIS variables
 * @return 1 if the certificate name and issuer attributes matched the expected
 *         values, 0 if they don't match the expected values.
 */
extern "C" void __declspec(dllexport)
VerifyCertNameIssuer(HWND hwndParent, int string_size,
               TCHAR *variables, stack_t **stacktop, void *extra)
{
  TCHAR tmp1[MAX_PATH + 1] = { _T('\0') };
  TCHAR tmp2[MAX_PATH + 1] = { _T('\0') };
  TCHAR tmp3[MAX_PATH + 1] = { _T('\0') };
  WCHAR filePath[MAX_PATH + 1] = { L'\0' };
  WCHAR certName[MAX_PATH + 1] = { L'\0' };
  WCHAR certIssuer[MAX_PATH + 1] = { L'\0' };

  popstring(stacktop, tmp1, MAX_PATH);
  popstring(stacktop, tmp2, MAX_PATH);
  popstring(stacktop, tmp3, MAX_PATH);

#if !defined(UNICODE)
    MultiByteToWideChar(CP_ACP, 0, tmp1, -1, filePath, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, tmp2, -1, certName, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, tmp3, -1, certIssuer, MAX_PATH);
#else
    wcsncpy(filePath, tmp1, MAX_PATH);
    wcsncpy(certName, tmp2, MAX_PATH);
    wcsncpy(certIssuer, tmp3, MAX_PATH);
#endif

  CertificateCheckInfo allowedCertificate = {
    certName,
    certIssuer,
  };

  LONG retCode = CheckCertificateForPEFile(filePath, allowedCertificate);
  if (retCode == ERROR_SUCCESS) {
    pushstring(stacktop, TEXT("1"), 2);
  } else {
    pushstring(stacktop, TEXT("0"), 2);
  }
}

/**
 * Verifies the trust of a signed file's certificate.
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
  trustData.pPolicyCallbackData = NULL;
  trustData.pSIPClientData = NULL;
  trustData.dwUIChoice = WTD_UI_NONE;
  trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
  trustData.dwUnionChoice = WTD_CHOICE_FILE;
  trustData.dwStateAction = 0;
  trustData.hWVTStateData = NULL;
  trustData.pwszURLReference = NULL;
  // no UI
  trustData.dwUIContext = 0;
  trustData.pFile = &fileToCheck;

  GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  // Check if the file is signed by something that is trusted.
  LONG ret = WinVerifyTrust(NULL, &policyGUID, &trustData);
  return ret;
}

/**
 * Verifies the trust of a signed file's certificate.
 *
 * @param  stacktop  A pointer to the top of the stack. This should be the file
 *                   path for the file that will have its trust verified.
 * @param  variables A pointer to the NSIS variables
 * @return 1 if the file's trust was verified successfully, 0 if it was not
 */
extern "C" void __declspec(dllexport)
VerifyCertTrust(HWND hwndParent, int string_size,
                TCHAR *variables, stack_t **stacktop, void *extra)
{
  TCHAR tmp[MAX_PATH + 1] = { _T('\0') };
  WCHAR filePath[MAX_PATH + 1] = { L'\0' };

  popstring(stacktop, tmp, MAX_PATH);

#if !defined(UNICODE)
    MultiByteToWideChar(CP_ACP, 0, tmp, -1, filePath, MAX_PATH);
#else
    wcsncpy(filePath, tmp, MAX_PATH);
#endif

  LONG retCode = VerifyCertificateTrustForFile(filePath);
  if (retCode == ERROR_SUCCESS) {
    pushstring(stacktop, TEXT("1"), 2);
  } else {
    pushstring(stacktop, TEXT("0"), 2);
  }
}

BOOL WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
  return TRUE;
}

/**
 * Removes an element from the top of the NSIS stack
 *
 * @param  stacktop A pointer to the top of the stack
 * @param  str      The string to pop to
 * @param  len      The max length
 * @return 0 on success
*/
int popstring(stack_t **stacktop, TCHAR *str, int len)
{
  // Removes the element from the top of the stack and puts it in the buffer
  stack_t *th;
  if (!stacktop || !*stacktop) {
    return 1;
  }

  th = (*stacktop);
  lstrcpyn(str,th->text, len);
  *stacktop = th->next;
  GlobalFree((HGLOBAL)th);
  return 0;
}

/**
 * Adds an element to the top of the NSIS stack
 *
 * @param  stacktop A pointer to the top of the stack
 * @param  str      The string to push on the stack
 * @param  len      The length of the string to push on the stack
 * @return 0 on success
*/
void pushstring(stack_t **stacktop, const TCHAR *str, int len)
{
  stack_t *th;
  if (!stacktop) { 
    return;
  }

  th = (stack_t*)GlobalAlloc(GPTR, sizeof(stack_t) + len);
  lstrcpyn(th->text, str, len);
  th->next = *stacktop;
  *stacktop = th;
}
