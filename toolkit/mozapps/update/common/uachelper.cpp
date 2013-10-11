/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include "uachelper.h"
#include "updatelogging.h"

typedef BOOL (WINAPI *LPWTSQueryUserToken)(ULONG, PHANDLE);

// See the MSDN documentation with title: Privilege Constants
// At the time of this writing, this documentation is located at: 
// http://msdn.microsoft.com/en-us/library/windows/desktop/bb530716%28v=vs.85%29.aspx
LPCTSTR UACHelper::PrivsToDisable[] = { 
  SE_ASSIGNPRIMARYTOKEN_NAME,
  SE_AUDIT_NAME,
  SE_BACKUP_NAME,
  // CreateProcess will succeed but the app will fail to launch on some WinXP 
  // machines if SE_CHANGE_NOTIFY_NAME is disabled.  In particular this happens
  // for limited user accounts on those machines.  The define is kept here as a
  // reminder that it should never be re-added.  
  // This permission is for directory watching but also from MSDN: "This
  // privilege also causes the system to skip all traversal access checks."
  // SE_CHANGE_NOTIFY_NAME,
  SE_CREATE_GLOBAL_NAME,
  SE_CREATE_PAGEFILE_NAME,
  SE_CREATE_PERMANENT_NAME,
  SE_CREATE_SYMBOLIC_LINK_NAME,
  SE_CREATE_TOKEN_NAME,
  SE_DEBUG_NAME,
  SE_ENABLE_DELEGATION_NAME,
  SE_IMPERSONATE_NAME,
  SE_INC_BASE_PRIORITY_NAME,
  SE_INCREASE_QUOTA_NAME,
  SE_INC_WORKING_SET_NAME,
  SE_LOAD_DRIVER_NAME,
  SE_LOCK_MEMORY_NAME,
  SE_MACHINE_ACCOUNT_NAME,
  SE_MANAGE_VOLUME_NAME,
  SE_PROF_SINGLE_PROCESS_NAME,
  SE_RELABEL_NAME,
  SE_REMOTE_SHUTDOWN_NAME,
  SE_RESTORE_NAME,
  SE_SECURITY_NAME,
  SE_SHUTDOWN_NAME,
  SE_SYNC_AGENT_NAME,
  SE_SYSTEM_ENVIRONMENT_NAME,
  SE_SYSTEM_PROFILE_NAME,
  SE_SYSTEMTIME_NAME,
  SE_TAKE_OWNERSHIP_NAME,
  SE_TCB_NAME,
  SE_TIME_ZONE_NAME,
  SE_TRUSTED_CREDMAN_ACCESS_NAME,
  SE_UNDOCK_NAME,
  SE_UNSOLICITED_INPUT_NAME
};

/**
 * Determines if the OS is vista or later
 *
 * @return TRUE if the OS is vista or later.
 */
BOOL
UACHelper::IsVistaOrLater()
{
  // Check if we are running Vista or later.
  OSVERSIONINFO osInfo;
  osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  return GetVersionEx(&osInfo) && osInfo.dwMajorVersion >= 6;
}

/**
 * Opens a user token for the given session ID
 *
 * @param  sessionID  The session ID for the token to obtain
 * @return A handle to the token to obtain which will be primary if enough
 *         permissions exist.  Caller should close the handle.
 */
HANDLE
UACHelper::OpenUserToken(DWORD sessionID)
{
  HMODULE module = LoadLibraryW(L"wtsapi32.dll");
  HANDLE token = nullptr;
  LPWTSQueryUserToken wtsQueryUserToken = 
    (LPWTSQueryUserToken)GetProcAddress(module, "WTSQueryUserToken");
  if (wtsQueryUserToken) {
    wtsQueryUserToken(sessionID, &token);
  }
  FreeLibrary(module);
  return token;
}

/**
 * Opens a linked token for the specified token.
 *
 * @param  token The token to get the linked token from
 * @return A linked token or nullptr if one does not exist.
 *         Caller should close the handle.
 */
HANDLE
UACHelper::OpenLinkedToken(HANDLE token)
{
  // Magic below...
  // UAC creates 2 tokens.  One is the restricted token which we have.
  // the other is the UAC elevated one. Since we are running as a service
  // as the system account we have access to both.
  TOKEN_LINKED_TOKEN tlt;
  HANDLE hNewLinkedToken = nullptr;
  DWORD len;
  if (GetTokenInformation(token, (TOKEN_INFORMATION_CLASS)TokenLinkedToken, 
                          &tlt, sizeof(TOKEN_LINKED_TOKEN), &len)) {
    token = tlt.LinkedToken;
    hNewLinkedToken = token;
  }
  return hNewLinkedToken;
}


/**
 * Enables or disables a privilege for the specified token.
 *
 * @param  token  The token to adjust the privilege on.
 * @param  priv   The privilege to adjust.
 * @param  enable Whether to enable or disable it
 * @return TRUE if the token was adjusted to the specified value.
 */
BOOL 
UACHelper::SetPrivilege(HANDLE token, LPCTSTR priv, BOOL enable)
{
  LUID luidOfPriv;
  if (!LookupPrivilegeValue(nullptr, priv, &luidOfPriv)) {
    return FALSE; 
  }

  TOKEN_PRIVILEGES tokenPriv;
  tokenPriv.PrivilegeCount = 1;
  tokenPriv.Privileges[0].Luid = luidOfPriv;
  tokenPriv.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

  SetLastError(ERROR_SUCCESS);
  if (!AdjustTokenPrivileges(token, false, &tokenPriv,
                             sizeof(tokenPriv), nullptr, nullptr)) {
    return FALSE; 
  } 

  return GetLastError() == ERROR_SUCCESS;
}

/**
 * For each privilege that is specified, an attempt will be made to 
 * drop the privilege. 
 * 
 * @param  token         The token to adjust the privilege on. 
 *         Pass nullptr for current token.
 * @param  unneededPrivs An array of unneeded privileges.
 * @param  count         The size of the array
 * @return TRUE if there were no errors
 */
BOOL
UACHelper::DisableUnneededPrivileges(HANDLE token, 
                                     LPCTSTR *unneededPrivs, 
                                     size_t count)
{
  HANDLE obtainedToken = nullptr;
  if (!token) {
    // Note: This handle is a pseudo-handle and need not be closed
    HANDLE process = GetCurrentProcess();
    if (!OpenProcessToken(process, TOKEN_ALL_ACCESS_P, &obtainedToken)) {
      LOG_WARN(("Could not obtain token for current process, no "
                "privileges changed. (%d)", GetLastError()));
      return FALSE;
    }
    token = obtainedToken;
  }

  BOOL result = TRUE;
  for (size_t i = 0; i < count; i++) {
    if (SetPrivilege(token, unneededPrivs[i], FALSE)) {
      LOG(("Disabled unneeded token privilege: %s.",
           unneededPrivs[i]));
    } else {
      LOG(("Could not disable token privilege value: %s. (%d)",
           unneededPrivs[i], GetLastError()));
      result = FALSE;
    }
  }

  if (obtainedToken) {
    CloseHandle(obtainedToken);
  }
  return result;
}

/**
 * Disables privileges for the specified token.
 * The privileges to disable are in PrivsToDisable.
 * In the future there could be new privs and we are not sure if we should
 * explicitly disable these or not. 
 * 
 * @param  token The token to drop the privilege on.
 *         Pass nullptr for current token.
 * @return TRUE if there were no errors
 */
BOOL
UACHelper::DisablePrivileges(HANDLE token)
{
  static const size_t PrivsToDisableSize = 
    sizeof(UACHelper::PrivsToDisable) / sizeof(UACHelper::PrivsToDisable[0]);

  return DisableUnneededPrivileges(token, UACHelper::PrivsToDisable, 
                                   PrivsToDisableSize);
}
