/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CityHash.h"
#include "cityhash/city.h"
#include <tchar.h>

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

void GetCityHash64(HWND hwndParent, int string_size, char *variables, stack_t **stacktop)
{
  TCHAR hashString[MAX_STRLEN];
  TCHAR hexResult[18] = { _T('\0') };

  g_stacktop = stacktop;
  g_variables = variables;

  memset(hashString, 0, sizeof(hashString));
  memset(hexResult, 0, sizeof(hexResult));

  if (!popString(hashString)) {
    pushString(L"error");
    return;
  }
  uint64 result = CityHash64((const char*)&hashString[0], wcslen(hashString)*sizeof(TCHAR));
  // If the hash happens to work out to less than 16 hash digits it will just
  // use less of the buffer.
  swprintf(hexResult, L"%I64X", result);
  pushString(hexResult);
}

}
