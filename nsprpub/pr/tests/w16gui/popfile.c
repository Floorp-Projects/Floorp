/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

/*------------------------------------------
   POPFILE.C -- Popup Editor File Functions
   (c) Charles Petzold, 1992
  ------------------------------------------*/

#include <windows.h>
#include <commdlg.h>
#include <stdlib.h>

static OPENFILENAME ofn ;

void PopFileInitialize (HWND hwnd)
     {
     static char *szFilter[] = { "Text Files (*.TXT)",  "*.txt",
                                 "ASCII Files (*.ASC)", "*.asc",
                                 "All Files (*.*)",     "*.*",
                                 "" } ;

     ofn.lStructSize       = sizeof (OPENFILENAME) ;
     ofn.hwndOwner         = hwnd ;
     ofn.hInstance         = NULL ;
     ofn.lpstrFilter       = szFilter [0] ;
     ofn.lpstrCustomFilter = NULL ;
     ofn.nMaxCustFilter    = 0 ;
     ofn.nFilterIndex      = 0 ;
     ofn.lpstrFile         = NULL ;          // Set in Open and Close functions
     ofn.nMaxFile          = _MAX_PATH ;
     ofn.lpstrFileTitle    = NULL ;          // Set in Open and Close functions
     ofn.nMaxFileTitle     = _MAX_FNAME + _MAX_EXT ;
     ofn.lpstrInitialDir   = NULL ;
     ofn.lpstrTitle        = NULL ;
     ofn.Flags             = 0 ;             // Set in Open and Close functions
     ofn.nFileOffset       = 0 ;
     ofn.nFileExtension    = 0 ;
     ofn.lpstrDefExt       = "txt" ;
     ofn.lCustData         = 0L ;
     ofn.lpfnHook          = NULL ;
     ofn.lpTemplateName    = NULL ;
     }

BOOL PopFileOpenDlg (HWND hwnd, LPSTR lpstrFileName, LPSTR lpstrTitleName)
     {
     ofn.hwndOwner         = hwnd ;
     ofn.lpstrFile         = lpstrFileName ;
     ofn.lpstrFileTitle    = lpstrTitleName ;
     ofn.Flags             = OFN_CREATEPROMPT ;

     return GetOpenFileName (&ofn) ;
     }

BOOL PopFileSaveDlg (HWND hwnd, LPSTR lpstrFileName, LPSTR lpstrTitleName)
     {
     ofn.hwndOwner         = hwnd ;
     ofn.lpstrFile         = lpstrFileName ;
     ofn.lpstrFileTitle    = lpstrTitleName ;
     ofn.Flags             = OFN_OVERWRITEPROMPT ;

     return GetSaveFileName (&ofn) ;
     }

static long PopFileLength (int hFile)
     {
     long lCurrentPos = _llseek (hFile, 0L, 1) ;
     long lFileLength = _llseek (hFile, 0L, 2) ;
     
     _llseek (hFile, lCurrentPos, 0) ;

     return lFileLength ;
     }

BOOL PopFileRead (HWND hwndEdit, LPSTR lpstrFileName)
     {
     long   lLength ;
     HANDLE hBuffer ;
     int    hFile ;
     LPSTR  lpstrBuffer ;

     if (-1 == (hFile = _lopen (lpstrFileName, OF_READ | OF_SHARE_DENY_WRITE)))
          return FALSE ;

     if ((lLength = PopFileLength (hFile)) >= 32000)
          {
          _lclose (hFile) ;
          return FALSE ;
          }

     if (NULL == (hBuffer = GlobalAlloc (GHND, lLength + 1)))
          {
          _lclose (hFile) ;
          return FALSE ;
          }

     lpstrBuffer = GlobalLock (hBuffer) ;
     _lread (hFile, lpstrBuffer, (WORD) lLength) ;
     _lclose (hFile) ;
     lpstrBuffer [(WORD) lLength] = '\0' ;

     SetWindowText (hwndEdit, lpstrBuffer) ;
     GlobalUnlock (hBuffer) ;
     GlobalFree (hBuffer) ;

     return TRUE ;
     }

BOOL PopFileWrite (HWND hwndEdit, LPSTR lpstrFileName)
     {
     HANDLE hBuffer ;
     int    hFile ;
     LPSTR  lpstrBuffer ;
     WORD   wLength ;

     if (-1 == (hFile = _lopen (lpstrFileName, OF_WRITE | OF_SHARE_EXCLUSIVE)))
          if (-1 == (hFile = _lcreat (lpstrFileName, 0)))
               return FALSE ;

     wLength = GetWindowTextLength (hwndEdit) ;
     hBuffer = (HANDLE) SendMessage (hwndEdit, EM_GETHANDLE, 0, 0L) ;
     lpstrBuffer = (LPSTR) LocalLock (hBuffer) ;

     if (wLength != _lwrite (hFile, lpstrBuffer, wLength))
          {
          _lclose (hFile) ;
          return FALSE ;
          }

     _lclose (hFile) ;
     LocalUnlock (hBuffer) ;

     return TRUE ;
     }
