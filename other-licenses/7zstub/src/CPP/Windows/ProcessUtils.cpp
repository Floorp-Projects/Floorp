// ProcessUtils.cpp

#include "StdAfx.h"

#include "../Common/StringConvert.h"

#include "ProcessUtils.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {

#ifndef UNDER_CE
static UString GetQuotedString(const UString &s)
{
  UString s2 ('\"');
  s2 += s;
  s2 += '\"';
  return s2;
}
#endif

WRes CProcess::Create(LPCWSTR imageName, const UString &params, LPCWSTR curDir)
{
  Close();
  const UString params2 =
      #ifndef UNDER_CE
      GetQuotedString(imageName) + L' ' +
      #endif
      params;
  #ifdef UNDER_CE
  curDir = 0;
  #else
  imageName = 0;
  #endif
  PROCESS_INFORMATION pi;
  BOOL result;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    STARTUPINFOA si;
    si.cb = sizeof(si);
    si.lpReserved = 0;
    si.lpDesktop = 0;
    si.lpTitle = 0;
    si.dwFlags = 0;
    si.cbReserved2 = 0;
    si.lpReserved2 = 0;
    
    CSysString curDirA;
    if (curDir != 0)
      curDirA = GetSystemString(curDir);
    result = ::CreateProcessA(NULL, (LPSTR)(LPCSTR)GetSystemString(params2),
        NULL, NULL, FALSE, 0, NULL, ((curDir != 0) ? (LPCSTR)curDirA: 0), &si, &pi);
  }
  else
  #endif
  {
    STARTUPINFOW si;
    si.cb = sizeof(si);
    si.lpReserved = 0;
    si.lpDesktop = 0;
    si.lpTitle = 0;
    si.dwFlags = 0;
    si.cbReserved2 = 0;
    si.lpReserved2 = 0;
    
    result = CreateProcessW(imageName, (LPWSTR)(LPCWSTR)params2,
        NULL, NULL, FALSE, 0, NULL, (LPWSTR)curDir, &si, &pi);
  }
  if (result == 0)
    return ::GetLastError();
  ::CloseHandle(pi.hThread);
  _handle = pi.hProcess;
  return 0;
}

WRes MyCreateProcess(LPCWSTR imageName, const UString &params)
{
  CProcess process;
  return process.Create(imageName, params, 0);
}

}
