/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
