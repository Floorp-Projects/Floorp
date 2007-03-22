/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <string.h>

extern HMODULE hInst;
static char szClassName[] = "LoadStatusWindowClass";

HWND ShowLoadStatus(char * aMessage)
{
  if (!aMessage)
    return NULL;

  if (!WinRegisterClass((HAB)0, szClassName, (PFNWP)WinDefWindowProc, 0, sizeof(ULONG))) {
    return NULL;
  }
  
  HWND hWnd = WinCreateWindow(HWND_DESKTOP, szClassName, "", WS_VISIBLE | WS_DISABLED, 
                              0, 0, 0, 0, HWND_DESKTOP, HWND_TOP, 255, (PVOID)NULL, NULL);
  if (!hWnd) {
    ERRORID error = WinGetLastError(0);
    return NULL;
  }
  
  HPS hPS = WinGetPS(hWnd);
  if (!hPS) {
    WinDestroyWindow(hWnd);
    return NULL;
  }

  POINTL ptls;
  GpiQueryTextBox(hPS, strlen(aMessage), aMessage, 1, &ptls);
  WinSetWindowPos(hWnd, HWND_TOP, 0, 0, ptls.x + 4, ptls.y + 2, SWP_SHOW );
  POINTL ptlStart;
  ptlStart.x = 2; ptlStart.y = 1;
  GpiCharStringAt(hPS, &ptlStart, strlen(aMessage), aMessage);
  WinReleasePS(hPS);

  return hWnd;
}

void DestroyLoadStatus(HWND ahWnd)
{
  if (ahWnd)
    WinDestroyWindow(ahWnd);

}
