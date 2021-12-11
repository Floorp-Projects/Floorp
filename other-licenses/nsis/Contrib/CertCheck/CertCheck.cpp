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

#ifndef UNICODE
#error "This file only supports building in Unicode mode"
#endif

static const int ENCODING = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

// The definitions for NSPIM, the callback typedef, and
// extra_parameters all come from the NSIS plugin API source.
enum NSPIM
{
  NSPIM_UNLOAD,
  NSPIM_GUIUNLOAD,
};

typedef UINT_PTR(*NSISPLUGINCALLBACK)(enum NSPIM);

struct extra_parameters
{
  // The real type of exec_flags is exec_flags_t*, which is a large struct
  // whose definition is omitted here because this plugin doesn't need it.
  void* exec_flags;
  int (__stdcall *ExecuteCodeSegment)(int, HWND);
  void (__stdcall *validate_filename)(TCHAR*);
  int (__stdcall *RegisterPluginCallback)(HMODULE, NSISPLUGINCALLBACK);
};

typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[MAX_PATH];
} stack_t;

int popstring(stack_t **stacktop, LPTSTR str, int len);
void pushstring(stack_t **stacktop, LPCTSTR str, int len);

struct CertificateCheckInfo
{
  wchar_t filePath[MAX_PATH];
  wchar_t name[MAX_PATH];
  wchar_t issuer[MAX_PATH];
};

static HINSTANCE gHInst;
static HANDLE gCheckThread;
static HANDLE gCheckEvent;
static bool gCheckTrustPassed;
static bool gCheckAttributesPassed;

// We need a plugin callback not only to clean up our thread, but also
// because registering a callback prevents NSIS from unloading the DLL
// after each call from the script.
UINT_PTR __cdecl
NSISPluginCallback(NSPIM event)
{
  if (event == NSPIM_UNLOAD){
    if (gCheckThread != NULL &&
        WaitForSingleObject(gCheckThread, 0) != WAIT_OBJECT_0) {
      TerminateThread(gCheckThread, ERROR_OPERATION_ABORTED);
    }
    CloseHandle(gCheckThread);
    gCheckThread = NULL;
    CloseHandle(gCheckEvent);
    gCheckEvent = NULL;
  }
  return NULL;
}

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
                             CertificateCheckInfo* infoToMatch)
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
  if (!infoToMatch->issuer ||
      wcscmp(szName, infoToMatch->issuer)) {
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
  if (!infoToMatch->name ||
      wcscmp(szName, infoToMatch->name)) {
    LocalFree(szName);
    return FALSE;
  }

  // We have a match!
  LocalFree(szName);

  // If there were any errors we would have aborted by now.
  return TRUE;
}

/**
 * Checks to see if a file's signing cert matches the specified info. This
 * only supports the name and issuer attributes currently.
 *
 * @param  info  The acceptable information to match
 * @return ERROR_SUCCESS if successful, ERROR_NOT_FOUND if the info
 *         does not match, or the last error otherwise.
 */
DWORD
CheckCertificateInfoForPEFile(CertificateCheckInfo* info)
{
  HCERTSTORE certStore = NULL;
  HCRYPTMSG cryptMsg = NULL;
  PCCERT_CONTEXT certContext = NULL;
  PCMSG_SIGNER_INFO signerInfo = NULL;
  DWORD lastError = ERROR_SUCCESS;

  // Get the HCERTSTORE and HCRYPTMSG from the signed file.
  DWORD encoding, contentType, formatType;
  BOOL result = CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                                 info->filePath,
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

  if (!DoCertificateAttributesMatch(certContext, info)) {
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
  // ZeroMemory should be fine here, but the compiler converts that into a
  // call to memset, and we're avoiding the C runtime to keep file size down.
  SecureZeroMemory(&trustData, sizeof(trustData));
  trustData.cbStruct = sizeof(trustData);
  trustData.dwUIChoice = WTD_UI_NONE;
  trustData.dwUnionChoice = WTD_CHOICE_FILE;
  trustData.pFile = &fileToCheck;

  GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  // Check if the file is signed by something that is trusted.
  LONG ret = WinVerifyTrust(NULL, &policyGUID, &trustData);
  return ret;
}

/**
 * Synchronously verifies the trust and attributes of a signed PE file.
 * Meant to be invoked as a thread entry point from CheckPETrustAndInfoAsync.
 */
DWORD WINAPI
VerifyCertThreadProc(void* info)
{
  CertificateCheckInfo* certInfo = (CertificateCheckInfo*)info;

  if (VerifyCertificateTrustForFile(certInfo->filePath) == ERROR_SUCCESS) {
    gCheckTrustPassed = true;
  }

  if (CheckCertificateInfoForPEFile(certInfo) == ERROR_SUCCESS) {
    gCheckAttributesPassed = true;
  }

  LocalFree(info);
  SetEvent(gCheckEvent);
  return 0;
}

/**
 * Verifies the trust and the attributes of a signed PE file's certificate on
*  a separate thread. Returns immediately upon starting that thread.
 * Call GetStatus (repeatedly if necessary) to get the result of the checks.
 *
 * @param  stacktop  A pointer to the NSIS stack.
 *                   From the top down, the stack should contain:
 *                   1) the path to the file that will have its trust verified
 *                   2) the expected certificate subject common name
 *                   3) the expected certificate issuer common name
 */
extern "C" void __declspec(dllexport)
CheckPETrustAndInfoAsync(HWND, int, TCHAR*, stack_t **stacktop, extra_parameters* pX)
{
  pX->RegisterPluginCallback(gHInst, NSISPluginCallback);

  gCheckTrustPassed = false;
  gCheckAttributesPassed = false;
  gCheckThread = nullptr;

  CertificateCheckInfo* certInfo =
    (CertificateCheckInfo*)LocalAlloc(0, sizeof(CertificateCheckInfo));
  if (certInfo) {
    popstring(stacktop, certInfo->filePath, MAX_PATH);
    popstring(stacktop, certInfo->name, MAX_PATH);
    popstring(stacktop, certInfo->issuer, MAX_PATH);

    gCheckThread = CreateThread(nullptr, 0, VerifyCertThreadProc,
                                (void*)certInfo, 0, nullptr);
  }
  if (!gCheckThread) {
    LocalFree(certInfo);
    SetEvent(gCheckEvent);
  }
}

/**
 * Returns the result of a certificate check on the NSIS stack.
 *
 * If the check is not yet finished, will push "0" to the stack.
 * If the check is finished, the top of the stack will be "1", followed by:
 *   "1" if the certificate is trusted by the system, "0" if not. Then:
 *   "1" if the certificate attributes matched those provided, "0" if not.
 */
extern "C" void __declspec(dllexport)
GetStatus(HWND, int, TCHAR*, stack_t **stacktop, void*)
{
  if (WaitForSingleObject(gCheckEvent, 0) == WAIT_OBJECT_0) {
    pushstring(stacktop, gCheckAttributesPassed ? L"1" : L"0", 2);
    pushstring(stacktop, gCheckTrustPassed ? L"1" : L"0", 2);
    pushstring(stacktop, L"1", 2);
  } else {
    pushstring(stacktop, L"0", 2);
  }
}

BOOL WINAPI
DllMain(HINSTANCE hInst, DWORD fdwReason, LPVOID)
{
  if (fdwReason == DLL_PROCESS_ATTACH) {
    gHInst = hInst;
    gCheckEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  }
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
int popstring(stack_t **stacktop, LPTSTR str, int len)
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
void pushstring(stack_t **stacktop, LPCTSTR str, int len)
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
