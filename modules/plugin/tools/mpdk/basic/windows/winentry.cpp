/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

/*
 * Mainly for debug purposes and in case we need a handle 
 * to application Windows instance
 */

/**********************************************************************
*
* winentry.cpp
*
* Windows entry point. Mainly for debug purposes.
*
***********************************************************************/

#include <windows.h>

#include "dbg.h"

HINSTANCE hInst = NULL;

BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
#ifdef DEBUG
  char szReason[80];

  switch (dwReason)
  {
    case DLL_PROCESS_ATTACH:
      strcpy(szReason, "DLL_PROCESS_ATTACH");
      break;
    case DLL_THREAD_ATTACH:
      strcpy(szReason, "DLL_THREAD_ATTACH");
      break;
    case DLL_PROCESS_DETACH:
      strcpy(szReason, "DLL_PROCESS_DETACH");
      break;
    case DLL_THREAD_DETACH:
      strcpy(szReason, "DLL_THREAD_DETACH");
      break;
  }

  dbgOut2("DllMain -- %s", szReason);
#endif

  hInst = hDLL;

  return TRUE;
}
