/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <windows.h>
#include <windowsx.h>

extern HINSTANCE hInst;
static char szClassName[] = "LoadStatusWindowClass";
HBRUSH hBrushBackground = NULL;

HWND ShowLoadStatus(char * aMessage)
{
  if (!aMessage)
    return NULL;

  LOGBRUSH lb;
  lb.lbStyle = BS_SOLID;
  lb.lbColor = RGB(255,255,0);
  lb.lbHatch = NULL;
  hBrushBackground = CreateBrushIndirect(&lb);

  WNDCLASS wc;
  wc.style         = 0; 
  wc.lpfnWndProc   = DefWindowProc; 
  wc.cbClsExtra    = 0; 
  wc.cbWndExtra    = 0; 
  wc.hInstance     = hInst; 
  wc.hIcon         = NULL; 
  wc.hCursor       = NULL; 
  wc.hbrBackground = hBrushBackground ? hBrushBackground : (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszMenuName  = NULL; 
  wc.lpszClassName = szClassName;

  if(!RegisterClass(&wc)) {
    DeleteBrush(hBrushBackground);
    return NULL;
  }
  
  HWND hWnd = CreateWindowEx(WS_EX_TOOLWINDOW, szClassName, "", 
                             WS_POPUP | WS_VISIBLE | WS_DISABLED,
                             0,0,0,0, NULL, NULL, hInst, NULL);
  if (!hWnd) {
    UnregisterClass(szClassName, hInst);
    DeleteBrush(hBrushBackground);
    return NULL;
  }

  HDC hDC = GetDC(hWnd);
  if (!hDC) {
    DestroyWindow(hWnd);
    UnregisterClass(szClassName, hInst);
    DeleteBrush(hBrushBackground);
    return NULL;
  }

  HFONT hFont = GetStockFont(DEFAULT_GUI_FONT);
  HFONT hFontOld = SelectFont(hDC, hFont);
  SIZE size;
  GetTextExtentPoint32(hDC, aMessage, strlen(aMessage), &size);
  SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, size.cx + 4, size.cy + 2, SWP_SHOWWINDOW);
  SetBkMode(hDC, TRANSPARENT);
  TextOut(hDC, 2, 1, aMessage, strlen(aMessage));
  ReleaseDC(hWnd, hDC);

  return hWnd;
}

void DestroyLoadStatus(HWND ahWnd)
{
  if (ahWnd)
    DestroyWindow(ahWnd);

  UnregisterClass(szClassName, hInst);
  DeleteBrush(hBrushBackground);
}
