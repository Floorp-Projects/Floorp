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
 * The Original Code is mozilla.org
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Mike Hommey <mh@glandium.org>
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

#include <stdio.h>
#include "nsWindowsDllInterceptor.h"

static bool patched_func_called = false;

static BOOL (WINAPI *orig_GetVersionExA)(LPOSVERSIONINFO);

static BOOL WINAPI
patched_GetVersionExA(LPOSVERSIONINFO lpVersionInfo)
{
  patched_func_called = true;
  return orig_GetVersionExA(lpVersionInfo);
}

bool osvi_equal(OSVERSIONINFO &info0, OSVERSIONINFO &info1)
{
  return (info0.dwMajorVersion == info1.dwMajorVersion &&
          info0.dwMinorVersion == info1.dwMinorVersion &&
          info0.dwBuildNumber == info1.dwBuildNumber &&
          info0.dwPlatformId == info1.dwPlatformId &&
          !strncmp(info0.szCSDVersion, info1.szCSDVersion, sizeof(info0.szCSDVersion)));
}

int main()
{
  OSVERSIONINFO info0, info1;
  ZeroMemory(&info0, sizeof(OSVERSIONINFO));
  ZeroMemory(&info1, sizeof(OSVERSIONINFO));
  info0.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  info1.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

  GetVersionExA(&info0);

  {
    WindowsDllInterceptor Kernel32Intercept;
    Kernel32Intercept.Init("kernel32.dll");
    if (Kernel32Intercept.AddHook("GetVersionExA", reinterpret_cast<intptr_t>(patched_GetVersionExA), (void**) &orig_GetVersionExA)) {
      printf("TEST-PASS | WindowsDllInterceptor | Hook added\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to add hook\n");
      return 1;
    }

    GetVersionExA(&info1);

    if (patched_func_called) {
      printf("TEST-PASS | WindowsDllInterceptor | Hook called\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Hook was not called\n");
      return 1;
    }

    if (osvi_equal(info0, info1)) {
      printf("TEST-PASS | WindowsDllInterceptor | Hook works properly\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Hook didn't return the right information\n");
      return 1;
    }
  }

  patched_func_called = false;

  GetVersionExA(&info1);

  if (!patched_func_called) {
    printf("TEST-PASS | WindowsDllInterceptor | Hook was not called after unregistration\n");
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Hook was still called after unregistration\n");
    return 1;
  }

  if (osvi_equal(info0, info1)) {
    printf("TEST-PASS | WindowsDllInterceptor | Original function worked properly\n");
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Original function didn't return the right information\n");
    return 1;
  }

  printf("TEST-PASS | WindowsDllInterceptor | all checks passed\n");
  return 0;
}
