/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <windows.h>
#include <windowsx.h>

#include "plugin.h"

// open the registry, create if necessary
HKEY openRegistry()      
{
  HKEY phkResult;

  if(RegCreateKey(HKEY_CURRENT_USER, REGISTRY_PLACE, &phkResult) != ERROR_SUCCESS)
    MessageBox(0, "Error creating Default Plugin registry key", "Default Plugin", MB_OK);

  return phkResult;
}

// return TRUE if we've never seen this MIME type before
BOOL IsNewMimeType(LPSTR mime)   
{
  HKEY hkey = openRegistry();
  DWORD dwType, keysize = 512;
  char keybuf[512];

  if(RegQueryValueEx(hkey, mime, 0, &dwType, (LPBYTE) &keybuf, &keysize) == ERROR_SUCCESS)
  {
    // key exists, must have already been here...
    return FALSE;
  }
  else 
  {
    if(RegSetValueEx(hkey, mime, 0,  REG_SZ, (LPBYTE) "(none)", 7) != ERROR_SUCCESS)
      MessageBox(0, "Error adding MIME type value", "Default Plugin", MB_OK);

    return TRUE;
  }
}

// string length in pixels for the specific window (selected font)
static int getWindowStringLength(HWND hWnd, LPSTR lpsz)
{
  SIZE sz;
  HDC hDC = GetDC(hWnd);
  HFONT hWindowFont = GetWindowFont(hWnd);
  HFONT hFontOld = SelectFont(hDC, hWindowFont);
  GetTextExtentPoint32(hDC, lpsz, lstrlen(lpsz), &sz);
  POINT pt;
  pt.x = sz.cx;
  pt.y = sz.cy;
  LPtoDP(hDC, &pt, 1);
  SelectFont(hDC, hFontOld);
  ReleaseDC(hWnd, hDC);
  return (int)pt.x;
}

/****************************************************************/
/*                                                              */
/* void SetDlgItemTextWrapped(HWND hWnd, int iID, LPSTR szText) */
/*                                                              */
/* helper to wrap long lines in a static control, which do not  */
/* wrap automatically if they do not have space characters      */
/*                                                              */
/****************************************************************/
void SetDlgItemTextWrapped(HWND hWnd, int iID, LPSTR szText)
{
  HWND hWndStatic = GetDlgItem(hWnd, iID);
  if((szText == NULL) || (lstrlen(szText) == 0))
  {
    SetDlgItemText(hWnd, iID, "");
    return;
  }

  RECT rc;
  GetClientRect(hWndStatic, &rc);

  int iStaticLength = rc.right - rc.left;
  int iStringLength = getWindowStringLength(hWndStatic, szText);

  if(iStringLength <= iStaticLength)
  {
    SetDlgItemText(hWnd, iID, szText);
    return;
  }

  int iBreaks = iStringLength/iStaticLength;
  if(iBreaks <= 0)
    return;

  char * pBuf = new char[iStringLength + iBreaks + 1];
  if(pBuf == NULL)
    return;

  lstrcpy(pBuf, "");

  int iStart = 0;
  int iLines = 0;
  for(int i = 0; i < iStringLength; i++)
  {
    char * sz = &szText[iStart];
    int iIndex = i - iStart;
    char ch = sz[iIndex + 1];

    sz[iIndex + 1] = '\0';

    int iLength = getWindowStringLength(hWndStatic, sz);

    if(iLength < iStaticLength)
    {
      sz[iIndex + 1] = ch;
      if(iLines == iBreaks)
      {
        lstrcat(pBuf, sz);
        break;
      }
      continue;
    }

    sz[iIndex + 1] = ch;  // restore zeroed element
    i--;                  // go one step back

    ch = sz[iIndex];
    sz[iIndex] = '\0';    // terminate string one char shorter

    lstrcat(pBuf, sz);    // append the string
    lstrcat(pBuf, " ");   // append space character for successful wrapping

    iStart += lstrlen(sz);// shift new start position
    sz[iIndex] = ch;      // restore zeroed element
    iLines++;             // count lines
  }

  SetDlgItemText(hWnd, iID, pBuf);

  delete [] pBuf;
}
