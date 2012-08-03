/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include "../../../../toolkit/mozapps/update/common/pathhash.h"

#pragma comment(lib, "advapi32.lib") 

typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[MAX_PATH];
} stack_t;

int popstring(stack_t **stacktop, LPTSTR str, int len);
void pushstring(stack_t **stacktop, LPCTSTR str, int len);

/**
 * Determines if the specified service exists or not
 *
 * @param  serviceName The name of the service to check
 * @param  exists      Whether or not the service exists 
 * @return TRUE if there were no errors
 */
static BOOL
IsServiceInstalled(LPCWSTR serviceName, BOOL &exists)
{
  exists = FALSE;

  // Get a handle to the local computer SCM database with full access rights.
  SC_HANDLE serviceManager = OpenSCManager(NULL, NULL, 
                                           SC_MANAGER_ENUMERATE_SERVICE);
  if (!serviceManager) {
    return FALSE;
  }

  SC_HANDLE serviceHandle = OpenServiceW(serviceManager, 
                                         serviceName, 
                                         SERVICE_QUERY_CONFIG);
  if (!serviceHandle && GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST) {
    CloseServiceHandle(serviceManager);
    return FALSE;
  }
 
  if (serviceHandle) {
    CloseServiceHandle(serviceHandle);
    exists = TRUE;
  } 

  CloseServiceHandle(serviceManager);
  return TRUE;
}

/**
 * Determines if the specified service is installed or not
 * 
 * @param  stacktop  A pointer to the top of the stack
 * @param  variables A pointer to the NSIS variables
 * @return 0 if the service does not exist
 *         1 if the service does exist
 *         -1 if there was an error.
 */
extern "C" void __declspec(dllexport)
IsInstalled(HWND hwndParent, int string_size, 
            TCHAR *variables, stack_t **stacktop, void *extra)
{
  TCHAR tmp[MAX_PATH] = { L'\0' };
  WCHAR serviceName[MAX_PATH] = { '\0' };
  popstring(stacktop, tmp, MAX_PATH);

#if !defined(UNICODE)
    MultiByteToWideChar(CP_ACP, 0, tmp, -1, serviceName, MAX_PATH);
#else
    wcscpy(serviceName, tmp);
#endif

  BOOL serviceInstalled;
  if (!IsServiceInstalled(serviceName, serviceInstalled)) {
    pushstring(stacktop, TEXT("-1"), 3);
  } else {
    pushstring(stacktop, serviceInstalled ? TEXT("1") : TEXT("0"), 2);
  }
}

/**
 * Stops the specified service.
 * 
 * @param  serviceName The name of the service to stop
 * @return TRUE if the operation was successful 
 */
static BOOL
StopService(LPCWSTR serviceName)
{
  // Get a handle to the local computer SCM database with full access rights.
  SC_HANDLE serviceManager = OpenSCManager(NULL, NULL, 
                                           SC_MANAGER_ENUMERATE_SERVICE);
  if (!serviceManager) {
    return FALSE;
  }

  SC_HANDLE serviceHandle = OpenServiceW(serviceManager, 
                                         serviceName, 
                                         SERVICE_STOP);
  if (!serviceHandle) {
    CloseServiceHandle(serviceManager);
    return FALSE;
  }

  //Stop the service so it deletes faster and so the uninstaller
  // can actually delete its EXE.
  DWORD totalWaitTime = 0;
  SERVICE_STATUS status;
  static const int maxWaitTime = 1000 * 60; // Never wait more than a minute
  BOOL stopped = FALSE;
  if (ControlService(serviceHandle, SERVICE_CONTROL_STOP, &status)) {
    do {
      Sleep(status.dwWaitHint);
      // + 10 milliseconds to make sure we always approach maxWaitTime
      totalWaitTime += (status.dwWaitHint + 10);
      if (status.dwCurrentState == SERVICE_STOPPED) {
        stopped = true;
        break;
      } else if (totalWaitTime > maxWaitTime) {
        break;
      }
    } while (QueryServiceStatus(serviceHandle, &status));
  }

  CloseServiceHandle(serviceHandle);
  CloseServiceHandle(serviceManager);
  return stopped;
}

/**
 * Stops the specified service
 * 
 * @param  stacktop  A pointer to the top of the stack
 * @param  variables A pointer to the NSIS variables 
 * @return 1 if the service was stopped, 0 on error
 */
extern "C" void __declspec(dllexport)
Stop(HWND hwndParent, int string_size, 
     TCHAR *variables, stack_t **stacktop, void *extra)
{
  TCHAR tmp[MAX_PATH] = { L'\0' };
  WCHAR serviceName[MAX_PATH] = { '\0' };

  popstring(stacktop, tmp, MAX_PATH);

#if !defined(UNICODE)
    MultiByteToWideChar(CP_ACP, 0, tmp, -1, serviceName, MAX_PATH);
#else
    wcscpy(serviceName, tmp);
#endif

  if (StopService(serviceName)) {
    pushstring(stacktop, TEXT("1"), 2);
  } else {
    pushstring(stacktop, TEXT("0"), 2);
  }
}

/**
 * Determines a unique registry path from a file or directory path
 * 
 * @param  stacktop  A pointer to the top of the stack
 * @param  variables A pointer to the NSIS variables 
 * @return The unique registry path or an empty string on error
 */
extern "C" void __declspec(dllexport)
PathToUniqueRegistryPath(HWND hwndParent, int string_size, 
                         TCHAR *variables, stack_t **stacktop, 
                         void *extra)
{
  TCHAR tmp[MAX_PATH] = { L'\0' };
  WCHAR installBasePath[MAX_PATH] = { '\0' };
  popstring(stacktop, tmp, MAX_PATH);

#if !defined(UNICODE)
    MultiByteToWideChar(CP_ACP, 0, tmp, -1, installBasePath, MAX_PATH);
#else
    wcscpy(installBasePath, tmp);
#endif

  WCHAR registryPath[MAX_PATH + 1] = { '\0' };
  if (CalculateRegistryPathFromFilePath(installBasePath, registryPath)) {
    pushstring(stacktop, registryPath, wcslen(registryPath) + 1);
  } else {
    pushstring(stacktop, TEXT(""), 1);
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
