/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * We want to impersonate the user when writing to user-controlled directories
 * from the maintenance service (running as Local System), for that we need an
 * impersonation token for the user. However, there is no direct connection
 * between the maintenance service and the user that started it.
 *
 * The approach here is to look for the updater process that the user ran,
 * which will still be hanging around while it waits for the service to stop.
 * We know the path of the updater that the service will run, and what
 * arguments we will call it with, and because the original updater should
 * have the same executable and command line we can look for a process matching
 * that description.
 *
 * That process's token is duplicated to make the impersonation token used
 * by the maintenance service and updater.
 */

#include <windows.h>
#include <stdio.h>
#include <oleauto.h>
#include <sddl.h>
#include <wbemidl.h>
#include <shellapi.h>

#ifndef __MINGW32__
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "wbemuuid.lib")
#endif

#include "usertoken.h"
#include "updatecommon.h"
#include "RefPtr.h"
#include "nsWindowsHelpers.h"
#include "mozilla/UniquePtr.h"

using mozilla::MakeUnique;
using mozilla::UniquePtr;

// Automatically manage initializing and uninitializing COM
class CoInitScope {
 private:
  HRESULT hr;

 public:
  CoInitScope() {
    hr = ::CoInitializeEx(0, COINIT_APARTMENTTHREADED);

    if (SUCCEEDED(hr)) {
      hr = ::CoInitializeSecurity(
          nullptr, -1, nullptr,
          nullptr,                      // not relevant for clients, or reserved
          RPC_C_AUTHN_LEVEL_DEFAULT,    // default authentication
          RPC_C_IMP_LEVEL_IMPERSONATE,  // default impersonation (server
                                        // impersonates)
          nullptr,                      // no authentication services
          EOAC_NONE,                    // no additional capabilities
          nullptr                       // reserved
      );

      if (!SUCCEEDED(hr)) {
        ::CoUninitialize();
      }
    }
  }

  MOZ_IMPLICIT operator bool() const { return SUCCEEDED(hr); }

  HRESULT GetHRESULT() const { return hr; }

  ~CoInitScope() {
    if (SUCCEEDED(hr)) {
      ::CoUninitialize();
    }
  }

 private:
  CoInitScope(const CoInitScope&) = delete;
  CoInitScope& operator=(const CoInitScope&) = delete;
  CoInitScope(CoInitScope&&) = delete;
  CoInitScope& operator=(CoInitScope&&) = delete;
};

// Container for a VARIANT, clears it when going out of scope.
class AutoVariant {
 private:
  VARIANT v;

 public:
  AutoVariant() { ::VariantInit(&v); }

  ~AutoVariant() { ::VariantClear(&v); }

  VARIANT& get() { return v; }

 private:
  AutoVariant(const AutoVariant&) = delete;
  AutoVariant& operator=(const AutoVariant&) = delete;
  AutoVariant(AutoVariant&&) = delete;
  AutoVariant& operator=(AutoVariant&&) = delete;
};

// Deleter for UniquePtr to free strings allocated with SysAllocString.
struct SysFreeStringDeleter {
  void operator()(void* aPtr) { SysFreeString((BSTR)aPtr); }
};

typedef UniquePtr<WCHAR, SysFreeStringDeleter> UniqueBstr;

static inline UniqueBstr UniqueBstrFromWSTR(LPCWSTR wstr) {
  UniqueBstr bstr(SysAllocString(wstr));
  return bstr;
}

/*
 * Quote and escape a string for use in a WQL query.
 */
static UniquePtr<WCHAR[]> QuoteWQLString(LPCWSTR src) {
  // The only special characters in WQL strings are \ and quote.
  // Quote can be ' or ", using ' unconditionally here.
  const WCHAR kQuote = L'\'';
  const WCHAR kBs = L'\\';

  // Compute the length of the quoted and escaped string.
  size_t quotedLength = 3;  // Always start quote, end quote and terminator
  for (size_t i = 0;; i++) {
    WCHAR c = src[i];

    if (c == kQuote || c == kBs) {
      quotedLength++;
    } else if (c == L'\0') {
      break;
    }

    quotedLength++;
  }

  // Allocate string.
  UniquePtr<WCHAR[]> quoted = MakeUnique<WCHAR[]>(quotedLength);
  if (!quoted) {
    return nullptr;
  }

  // Fill in the quoted string.
  quoted[0] = kQuote;
  for (size_t i = 0, quotedIdx = 1;; i++) {
    WCHAR c = src[i];

    if (c == kQuote || c == kBs) {
      quoted[quotedIdx++] = kBs;
    } else if (c == L'\0') {
      quoted[quotedIdx++] = kQuote;
      quoted[quotedIdx++] = L'\0';
      break;
    }

    quoted[quotedIdx++] = c;
  }

  return quoted;
}

/*
 * Compare the creation time of a process with a CIM DateTime BSTR from WMI.
 *
 * This comparison is complicated by two issues:
 * 1) DateTime has microsecond resolution, but process creation time
 *    is reported in a FILETIME with 100 nanosecond resolution.
 * 2) DateTime's default BSTR representation is a local time, but
 *    the process creation FILETIME is in GMT (UTC+0).
 *
 * Combined this prevents a straightforward comparison of FILETIMEs or
 * DateTime BSTRs.
 * This function converts to UTC+0 DateTime BSTRs for comparison.
 *
 * @return true if the process's creation time matches, false if
 *         there was no match or some error
 */
static bool CheckCreationTime(const CoInitScope& coInited, HANDLE hProc,
                              BSTR checkCreationDateLocal) {
  if (!coInited) {
    return false;
  }

  // Convert the WMI creation date to UTC+0 so it can be compared.
  UniqueBstr checkCreationDate;
  {
    RefPtr<ISWbemDateTime> pCheckDateTime;
    if (FAILED(CoCreateInstance(CLSID_SWbemDateTime, nullptr,
                                CLSCTX_INPROC_SERVER, IID_ISWbemDateTime,
                                getter_AddRefs(pCheckDateTime)))) {
      return false;
    }

    // local DateTime to FILETIME
    UniqueBstr checkCreationFILETIME;
    {
      BSTR checkCreationFILETIMERaw = nullptr;
      if (FAILED(pCheckDateTime->put_Value(checkCreationDateLocal)) ||
          FAILED(pCheckDateTime->GetFileTime(VARIANT_FALSE,
                                             &checkCreationFILETIMERaw))) {
        return false;
      }
      checkCreationFILETIME.reset(checkCreationFILETIMERaw);
    }

    // FILETIME to UTC+0 DateTime
    BSTR checkCreationDateRaw;
    if (FAILED(pCheckDateTime->SetFileTime(checkCreationFILETIME.get(),
                                           VARIANT_FALSE)) ||
        FAILED(pCheckDateTime->get_Value(&checkCreationDateRaw))) {
      return false;
    }
    checkCreationDate.reset(checkCreationDateRaw);
  }

  // Get the process creation time and convert to DateTime for comparison.
  UniqueBstr procCreationDate;
  {
    FILETIME procCreationTime, _exitTime, _kernelTime, _userTime;
    if (!GetProcessTimes(hProc, &procCreationTime, &_exitTime, &_kernelTime,
                         &_userTime)) {
      LOG_WARN(("GetProcessTimes failed. (%lu)", GetLastError()));
      return false;
    }

    // Print the creation time into a BSTR.
    UniqueBstr createFILETIME;
    {
      ULARGE_INTEGER timeInt;
      timeInt.LowPart = procCreationTime.dwLowDateTime;
      timeInt.HighPart = procCreationTime.dwHighDateTime;

      static const WCHAR timeFmt[] = L"%llu";
      const size_t timeBufLen = _scwprintf(timeFmt, timeInt.QuadPart) + 1;
      UniquePtr<WCHAR[]> timeBuf = MakeUnique<WCHAR[]>(timeBufLen);
      swprintf_s(timeBuf.get(), timeBufLen, timeFmt, timeInt.QuadPart);

      createFILETIME = UniqueBstrFromWSTR(timeBuf.get());
      if (!createFILETIME) {
        return false;
      }
    }

    RefPtr<ISWbemDateTime> pProcDateTime;
    if (FAILED(CoCreateInstance(CLSID_SWbemDateTime, nullptr,
                                CLSCTX_INPROC_SERVER, IID_ISWbemDateTime,
                                getter_AddRefs(pProcDateTime)))) {
      return false;
    }

    // FILETIME to UTC+0 DateTime
    BSTR procCreationDateRaw;
    if (FAILED(
            pProcDateTime->SetFileTime(createFILETIME.get(), VARIANT_FALSE)) ||
        FAILED(pProcDateTime->get_Value(&procCreationDateRaw))) {
      return false;
    }
    procCreationDate.reset(procCreationDateRaw);
  }

  return !wcscmp(checkCreationDate.get(), procCreationDate.get());
}

/*
 * Get an impersonation token for the process which was run with the given
 * image and command line arguments. The first argument is ignored as it
 * may differ from the original command line.
 *
 * Fails if there is more than one matching process.
 *
 * @return nullptr on failure. Caller is responsible for closing the handle,
 *         though practically it will be left open until the process ends.
 */
HANDLE
GetUserProcessToken(LPCWSTR updaterPath, int updaterArgc,
                    LPCWSTR updaterArgv[]) {
  CoInitScope coInited;
  if (!coInited) {
    LOG_WARN(("Failed to initialize COM or COM security. (hr = %ld)",
              coInited.GetHRESULT()));
    return nullptr;
  }

  nsAutoHandle rv;
  DWORD matchedPid = 0;

  HRESULT hr;

  // Create SIDs for membership check.
  UniquePtr<void, LocalFreeDeleter> systemSid;
  {
    PSID sid;
    if (!ConvertStringSidToSidA("SY", &sid)) {
      return nullptr;
    }
    systemSid.reset(sid);
  }
  UniquePtr<void, LocalFreeDeleter> usersSid;
  {
    PSID sid;
    if (!ConvertStringSidToSidA("BU", &sid)) {
      return nullptr;
    }
    usersSid.reset(sid);
  }

  // Create Bstrs for column names.
  UniqueBstr processIdName = UniqueBstrFromWSTR(L"ProcessId");
  UniqueBstr creationDateName = UniqueBstrFromWSTR(L"CreationDate");
  UniqueBstr commandLineName = UniqueBstrFromWSTR(L"CommandLine");
  if (!processIdName || !creationDateName || !commandLineName) {
    return nullptr;
  }

  RefPtr<IEnumWbemClassObject> pQueryRows;
  {
    // Build the query BSTR.
    static const WCHAR queryFormat[] =
        L"SELECT ProcessId, CreationDate, CommandLine "
        "FROM Win32_Process "
        "WHERE ExecutablePath=%ls";
    UniqueBstr queryBstr;
    {
      UniquePtr<WCHAR[]> quotedExePath = QuoteWQLString(updaterPath);
      if (!quotedExePath) {
        return nullptr;
      }
      const size_t queryBufLen =
          _scwprintf(queryFormat, quotedExePath.get()) + 1;

      UniquePtr<WCHAR[]> query = MakeUnique<WCHAR[]>(queryBufLen);
      if (!query) {
        return nullptr;
      }

      swprintf_s(query.get(), queryBufLen, queryFormat, quotedExePath.get());
      queryBstr = UniqueBstrFromWSTR(query.get());
      if (!queryBstr) {
        return nullptr;
      }
    }

    // Connect to the server.
    RefPtr<IWbemLocator> pLocator;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, getter_AddRefs(pLocator));
    if (FAILED(hr)) {
      LOG_WARN(("Failed to create WbemLocator. (hr = %ld)", hr));
      return nullptr;
    }

    RefPtr<IWbemServices> pServices;
    UniqueBstr resourceBstr(SysAllocString(L"ROOT\\CimV2"));
    if (!resourceBstr) {
      return nullptr;
    }
    hr = pLocator->ConnectServer(
        resourceBstr.get(), nullptr,
        nullptr,  // use current security context (no user, pw)
        nullptr,  // use current locale
        WBEM_FLAG_CONNECT_USE_MAX_WAIT,  // use timeout
        nullptr,                         // use current NTLM domain
        nullptr,  // no context for dynamic class providers
        getter_AddRefs(pServices));
    if (FAILED(hr)) {
      LOG_WARN(("Failed to connect to WMI server. (hr = %ld)", hr));
      return nullptr;
    }

    // Set authentication information for the IWbemServices proxy.
    if (FAILED(CoSetProxyBlanket(
            pServices,
            RPC_C_AUTHN_WINNT,       // WINNT authentication
            RPC_C_AUTHZ_NONE,        // no authorization (reqd for AUTHN_WINNT)
            nullptr,                 // server principal (no change)
            RPC_C_AUTHN_LEVEL_CALL,  // authenticate at start of RPC call
            RPC_C_IMP_LEVEL_IMPERSONATE,  // allow the proxy to impersonate us
            nullptr,                      // identity (use process token)
            EOAC_NONE))) {                // no capability flags
      LOG_WARN(("Failed to set proxy blanket. (hr = %ld)", hr));
      return nullptr;
    }

    // Perform the query.

    UniqueBstr wqlBstr(SysAllocString(L"WQL"));
    hr =
        pServices->ExecQuery(wqlBstr.get(), queryBstr.get(),
                             0,        // no flags
                             nullptr,  // no context for dynamic class providers
                             getter_AddRefs(pQueryRows));
    if (FAILED(hr)) {
      LOG_WARN(("Command line query failed. (hr = %ld)", hr));
      return nullptr;
    }
  }

  // Iterate over the results.
  HRESULT hIterRes;

  RefPtr<IWbemClassObject> pRow;
  DWORD uReturned = 0;
  while (SUCCEEDED(hIterRes = pQueryRows->Next(
                       WBEM_INFINITE, 1, getter_AddRefs(pRow), &uReturned))) {
    if (hIterRes == WBEM_S_FALSE && uReturned == 0) {
      // 0 rows, end of results.
      break;
    } else if (hIterRes != WBEM_S_NO_ERROR || uReturned != 1) {
      // Query error, abort.
      LOG_WARN(("Get next row failed. (hr = %ld)", hIterRes));
      return nullptr;
    }

    AutoVariant commandLine;
    if (FAILED(pRow->Get(commandLineName.get(), 0, &commandLine.get(), nullptr,
                         nullptr))) {
      return nullptr;
    }
    if (V_VT(&commandLine.get()) != VT_BSTR) {
      // Abort on unexpected type.
      return nullptr;
    }

    // Check the command line arguments.
    {
      int resultArgc = 0;
      UniquePtr<LPWSTR, LocalFreeDeleter> resultArgv(
          CommandLineToArgvW(commandLine.get().bstrVal, &resultArgc));

      if (!resultArgv || resultArgc != updaterArgc) {
        continue;
      }

      bool matched = true;
      // Deliberately skipping argv[0]
      for (int i = 1; matched && i < resultArgc; i++) {
        if (wcsicmp(resultArgv.get()[i], updaterArgv[i])) {
          matched = false;
        }
      }

      if (!matched) {
        continue;
      }
    }

    // Command line arguments match!

    // Get ProcessId.
    DWORD pid;
    {
      AutoVariant processId;
      CIMTYPE processIdCimType;
      if (FAILED(pRow->Get(processIdName.get(), 0, &processId.get(),
                           &processIdCimType, nullptr))) {
        return nullptr;
      }
      // For some reason processId is VT_I4 (signed) but also
      // CIM_UINT32 (unsigned).
      if (V_VT(&processId.get()) != VT_I4 || processIdCimType != CIM_UINT32) {
        LOG_WARN(("Unexpected variant type %d or CIM type %d for processId",
                  V_VT(&processId.get()), processIdCimType));
        return nullptr;
      }
      pid = static_cast<DWORD>(processId.get().lVal);
    }

    // Get CreationDate.
    AutoVariant creationDate;
    CIMTYPE creationDateCimType;
    if (FAILED(pRow->Get(creationDateName.get(), 0, &creationDate.get(),
                         &creationDateCimType, nullptr))) {
      return nullptr;
    }
    if (V_VT(&creationDate.get()) != VT_BSTR ||
        creationDateCimType != CIM_DATETIME) {
      LOG_WARN(("Unexpected variant type %d or CIM type %d for creationDate",
                V_VT(&creationDate.get()), creationDateCimType));
      return nullptr;
    }

    // Get a handle to the process.
    nsAutoHandle hProc(OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid));
    if (!hProc) {
      // Couldn't open the process, it may have gone away.
      continue;
    }

    // Verify process creation time (as pids can be reused).
    if (!CheckCreationTime(coInited, hProc, creationDate.get().bstrVal)) {
      LOG_WARN(("Failed to verify process creation time, skipping."));
      continue;
    }

    // Get impersonation token.
    nsAutoHandle hDupToken;
    {
      HANDLE hRawProcToken = nullptr;
      if (!OpenProcessToken(hProc,
                            TOKEN_DUPLICATE | TOKEN_IMPERSONATE | TOKEN_QUERY,
                            (PVOID*)&hRawProcToken)) {
        LOG_WARN(("Failed to open impersonation token for pid (%lu). (%d)", pid,
                  GetLastError()));
        continue;
      }

      // Duplicate for impersonation and checking, inheritable by child.
      SECURITY_ATTRIBUTES sa;
      sa.nLength = sizeof(sa);
      sa.bInheritHandle = TRUE;
      sa.lpSecurityDescriptor = nullptr;

      HANDLE hRawDupToken = nullptr;
      BOOL result = DuplicateTokenEx(
          hRawProcToken, TOKEN_DUPLICATE | TOKEN_IMPERSONATE | TOKEN_QUERY, &sa,
          SecurityImpersonation, TokenImpersonation, &hRawDupToken);
      CloseHandle(hRawProcToken);
      if (!result) {
        LOG_WARN(("Failed to duplicate impersonation token for pid (%lu). (%d)",
                  pid, GetLastError()));
        continue;
      }

      hDupToken.own(hRawDupToken);
    }

    // Check memberships. This is done to avoid impersonating an updater
    // already running as System, which will never occur normally (an
    // updater running as System won't need to invoke the maintenance service).
    // Also restrict to Builtin User processes as they are the only users
    // granted access to the maintenance service (Local Service is in BU).
    {
      BOOL isMember = FALSE;
      if (!CheckTokenMembership(hDupToken.get(), systemSid.get(), &isMember) ||
          isMember) {
        // Reject if check failed or it is a System process.
        continue;
      }
      if (!CheckTokenMembership(hDupToken.get(), usersSid.get(), &isMember) ||
          !isMember) {
        // Reject if check failed or it is not a Builtin User process.
        continue;
      }
    }

    // By this point, the process seems like a match.

    if (rv) {
      // Multiple results, abort.
      // We have no way to decide between multiple matches.
      LOG_WARN(
          ("Matched multiple possible updater processes "
           "(pids %lu and %lu), can't choose one to impersonate.",
           pid, matchedPid));
      return nullptr;
    }

    // Found a result!
    rv.swap(hDupToken);
    matchedPid = pid;
  }  // end of while loop over query result rows

  if (!rv) {
    LOG_WARN(("Found no matching updater process to impersonate."));
  } else {
    LOG(("Successfully matched pid %lu and got impersonation token.",
         matchedPid));
  }

  return rv.disown();
}
