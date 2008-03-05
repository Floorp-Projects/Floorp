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
 * The Original Code is Mozilla Communicator client code.
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

#include <windows.h>
#include <windowsx.h>

#include "plugin.h"

// open the registry, create if necessary
HKEY openRegistry()      
{
  HKEY phkResult;

  if(RegCreateKeyW(HKEY_CURRENT_USER, REGISTRY_PLACE, &phkResult) != ERROR_SUCCESS)
    MessageBoxW(0, L"Error creating Default Plugin registry key", L"Default Plugin", MB_OK);

  return phkResult;
}

// return TRUE if we've never seen this MIME type before
BOOL IsNewMimeType(LPSTR mime)   
{
  HKEY hkey = openRegistry();
  DWORD dwType, keysize = 512;
  wchar_t keybuf[512];
  wchar_t wideMime[64];

  MultiByteToWideChar(CP_ACP, 0, 
                      mime,
                      strlen(mime) + 1, 
                      wideMime, 
                      64);
  
  if(RegQueryValueExW(hkey, wideMime, 0, &dwType, (LPBYTE) &keybuf, &keysize) == ERROR_SUCCESS)
  {
    // key exists, must have already been here...
    return FALSE;
  }
  else 
  {
    if(RegSetValueExW(hkey, wideMime, 0,  REG_SZ, (LPBYTE) L"(none)", 7) != ERROR_SUCCESS)
      MessageBoxW(0, L"Error adding MIME type value", L"Default Plugin", MB_OK);

    return TRUE;
  }
}

// string length in pixels for the specific window (selected font)
static int getWindowStringLength(HWND hWnd, wchar_t* lpsz)
{
  SIZE sz;
  HDC hDC = GetDC(hWnd);
  HFONT hWindowFont = GetWindowFont(hWnd);
  HFONT hFontOld = SelectFont(hDC, hWindowFont);
  GetTextExtentPoint32W(hDC, lpsz, wcslen(lpsz), &sz);
  POINT pt;
  pt.x = sz.cx;
  pt.y = sz.cy;
  LPtoDP(hDC, &pt, 1);
  SelectFont(hDC, hFontOld);
  ReleaseDC(hWnd, hDC);
  return (int)pt.x;
}

/*******************************************************************/
/*                                                                 */
/* void SetDlgItemTextWrapped(HWND hWnd, int iID, wchar_t* szText) */
/*                                                                 */
/* helper to wrap long lines in a static control, which do not     */
/* wrap automatically if they do not have space characters         */
/*                                                                 */
/*******************************************************************/
void SetDlgItemTextWrapped(HWND hWnd, int iID, wchar_t* szText)
{
  HWND hWndStatic = GetDlgItem(hWnd, iID);
  if((szText == NULL) || (wcslen(szText) == 0))
  {
    SetDlgItemTextW(hWnd, iID, L"");
    return;
  }

  RECT rc;
  GetClientRect(hWndStatic, &rc);

  int iStaticLength = rc.right - rc.left;
  int iStringLength = getWindowStringLength(hWndStatic, szText);

  if(iStringLength <= iStaticLength)
  {
    SetDlgItemTextW(hWnd, iID, szText);
    return;
  }

  int iBreaks = iStringLength/iStaticLength;
  if(iBreaks <= 0)
    return;

  wchar_t * pBuf = new wchar_t[iStringLength + iBreaks + 1];
  if(pBuf == NULL)
    return;

  wcscpy(pBuf, L"");

  int iStart = 0;
  int iLines = 0;
  for(int i = 0; i < iStringLength; i++)
  {
    wchar_t* sz = &szText[iStart];
    int iIndex = i - iStart;
    wchar_t ch = sz[iIndex + 1];

    sz[iIndex + 1] = '\0';

    int iLength = getWindowStringLength(hWndStatic, sz);

    if(iLength < iStaticLength)
    {
      sz[iIndex + 1] = ch;
      if(iLines == iBreaks)
      {
        wcscat(pBuf, sz);
        break;
      }
      continue;
    }

    sz[iIndex + 1] = ch;  // restore zeroed element
    i--;                  // go one step back

    ch = sz[iIndex];
    sz[iIndex] = '\0';    // terminate string one char shorter

    wcscat(pBuf, sz);     // append the string
    wcscat(pBuf, L" ");   // append space character for successful wrapping

    iStart += wcslen(sz); // shift new start position
    sz[iIndex] = ch;      // restore zeroed element
    iLines++;             // count lines
  }

  SetDlgItemTextW(hWnd, iID, pBuf);

  delete [] pBuf;
}
