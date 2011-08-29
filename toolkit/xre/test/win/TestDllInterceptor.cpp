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

struct payload {
  UINT64 a;
  UINT64 b;
  UINT64 c;

  bool operator==(const payload &other) const {
    return (a == other.a &&
            b == other.b &&
            c == other.c);
  }
};

extern "C" __declspec(dllexport,noinline) payload rotatePayload(payload p) {
  UINT64 tmp = p.a;
  p.a = p.b;
  p.b = p.c;
  p.c = tmp;
  return p;
}

static bool patched_func_called = false;

static payload (*orig_rotatePayload)(payload);

static payload
patched_rotatePayload(payload p)
{
  patched_func_called = true;
  return orig_rotatePayload(p);
}

bool TestHook(const char *dll, const char *func)
{
  void *orig_func;
  WindowsDllInterceptor TestIntercept;
  TestIntercept.Init(dll);
  if (TestIntercept.AddHook(func, 0, &orig_func)) {
    printf("TEST-PASS | WindowsDllInterceptor | Could hook %s from %s\n", func, dll);
    return true;
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to hook %s from %s\n", func, dll);
    return false;
  }
}

int main()
{
  payload initial = { 0x12345678, 0xfc4e9d31, 0x87654321 };
  payload p0, p1;
  ZeroMemory(&p0, sizeof(p0));
  ZeroMemory(&p1, sizeof(p1));

  p0 = rotatePayload(initial);

  {
    WindowsDllInterceptor ExeIntercept;
    ExeIntercept.Init("TestDllInterceptor.exe");
    if (ExeIntercept.AddHook("rotatePayload", reinterpret_cast<intptr_t>(patched_rotatePayload), (void**) &orig_rotatePayload)) {
      printf("TEST-PASS | WindowsDllInterceptor | Hook added\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to add hook\n");
      return 1;
    }

    p1 = rotatePayload(initial);

    if (patched_func_called) {
      printf("TEST-PASS | WindowsDllInterceptor | Hook called\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Hook was not called\n");
      return 1;
    }

    if (p0 == p1) {
      printf("TEST-PASS | WindowsDllInterceptor | Hook works properly\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Hook didn't return the right information\n");
      return 1;
    }
  }

  patched_func_called = false;
  ZeroMemory(&p1, sizeof(p1));

  p1 = rotatePayload(initial);

  if (!patched_func_called) {
    printf("TEST-PASS | WindowsDllInterceptor | Hook was not called after unregistration\n");
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Hook was still called after unregistration\n");
    return 1;
  }

  if (p0 == p1) {
    printf("TEST-PASS | WindowsDllInterceptor | Original function worked properly\n");
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Original function didn't return the right information\n");
    return 1;
  }

  if (TestHook("user32.dll", "GetWindowInfo") &&
#ifdef _WIN64
      TestHook("user32.dll", "SetWindowLongPtrA") &&
      TestHook("user32.dll", "SetWindowLongPtrW") &&
#else
      TestHook("user32.dll", "SetWindowLongA") &&
      TestHook("user32.dll", "SetWindowLongW") &&
#endif
      TestHook("user32.dll", "TrackPopupMenu") &&
      TestHook("ntdll.dll", "LdrLoadDll")) {
    printf("TEST-PASS | WindowsDllInterceptor | all checks passed\n");
    return 0;
  }

  return 1;
}
