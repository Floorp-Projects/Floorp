// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0.If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "exdll.h"

unsigned int g_stringsize;
stack_t** g_stacktop;
int(__stdcall* g_executeCodeSegment)(int pos, HWND hwndProgress);
HWND g_hwndParent;

int popstring(TCHAR* str) {
  stack_t* th;
  if (!g_stacktop || !*g_stacktop) return 1;
  th = (*g_stacktop);
  lstrcpy(str, th->text);
  *g_stacktop = th->next;
  GlobalFree((HGLOBAL)th);
  return 0;
}

void pushstring(const TCHAR* str) {
  stack_t* th;
  if (!g_stacktop) return;
  th = (stack_t*)GlobalAlloc(GPTR,
                             sizeof(stack_t) + (g_stringsize * sizeof(TCHAR)));
  lstrcpyn(th->text, str, g_stringsize);
  th->next = *g_stacktop;
  *g_stacktop = th;
}
