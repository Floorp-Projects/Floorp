/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#include "CityHash.h"
#include "cityhash/city.h"

#define MAX_STRLEN 1024

typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[MAX_STRLEN];
} stack_t;

stack_t **g_stacktop;
char *g_variables;

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
  switch (ul_reason_for_call)
  {
  case DLL_PROCESS_ATTACH:
  case DLL_THREAD_ATTACH:
  case DLL_THREAD_DETACH:
  case DLL_PROCESS_DETACH:
    break;
  }
  return TRUE;
}

bool popString(TCHAR *result)
{
  stack_t *th;
  if (!g_stacktop || !*g_stacktop) return false;
  th = (*g_stacktop);
  lstrcpyn(result, th->text, MAX_STRLEN);
  *g_stacktop = th->next;
  GlobalFree((HGLOBAL)th);
  return true;
}

void pushString(const TCHAR *str)
{
  stack_t *th;
  int strLen = wcslen(str)+1;
  if (!g_stacktop) return;
  th = (stack_t*)GlobalAlloc(GPTR, sizeof(stack_t) + (MAX_STRLEN*sizeof(TCHAR)));
  lstrcpyn(th->text, str, strLen);
  th->next = *g_stacktop;
  *g_stacktop = th;
}

extern "C"
{

CITYHASH_API
void GetCityHash64(HWND hwndParent, int string_size, char *variables, stack_t **stacktop)
{
  TCHAR hashString[MAX_STRLEN];
  TCHAR hexResult[18];

  g_stacktop = stacktop;
  g_variables = variables;

  memset(hashString, 0, sizeof(hashString));
  memset(hexResult, 0, sizeof(hexResult));

  if (!popString(hashString)) {
    pushString(L"error");
    return;
  }
  uint64 result = CityHash64((const char*)&hashString[0], wcslen(hashString)*sizeof(TCHAR));
  swprintf_s(hexResult, 17, L"%16I64X", result);
  pushString(hexResult);
}

}