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

/*--------------------------------------------------------
   POPFIND.C -- Popup Editor Search and Replace Functions
   (c) Charles Petzold, 1992
  --------------------------------------------------------*/

#include <windows.h>
#include <commdlg.h>
#include <string.h>
#define MAX_STRING_LEN   256

static char szFindText [MAX_STRING_LEN] ;
static char szReplText [MAX_STRING_LEN] ;

HWND PopFindFindDlg (HWND hwnd)
     {
     static FINDREPLACE fr ;       // must be static for modeless dialog!!!

     fr.lStructSize      = sizeof (FINDREPLACE) ;
     fr.hwndOwner        = hwnd ;
     fr.hInstance        = NULL ;
     fr.Flags            = FR_HIDEUPDOWN | FR_HIDEMATCHCASE | FR_HIDEWHOLEWORD ;
     fr.lpstrFindWhat    = szFindText ;
     fr.lpstrReplaceWith = NULL ;
     fr.wFindWhatLen     = sizeof (szFindText) ;
     fr.wReplaceWithLen  = 0 ;
     fr.lCustData        = 0 ;
     fr.lpfnHook         = NULL ;
     fr.lpTemplateName   = NULL ;

     return FindText (&fr) ;
     }

HWND PopFindReplaceDlg (HWND hwnd)
     {
     static FINDREPLACE fr ;       // must be static for modeless dialog!!!

     fr.lStructSize      = sizeof (FINDREPLACE) ;
     fr.hwndOwner        = hwnd ;
     fr.hInstance        = NULL ;
     fr.Flags            = FR_HIDEUPDOWN | FR_HIDEMATCHCASE | FR_HIDEWHOLEWORD ;
     fr.lpstrFindWhat    = szFindText ;
     fr.lpstrReplaceWith = szReplText ;
     fr.wFindWhatLen     = sizeof (szFindText) ;
     fr.wReplaceWithLen  = sizeof (szReplText) ;
     fr.lCustData        = 0 ;
     fr.lpfnHook         = NULL ;
     fr.lpTemplateName   = NULL ;

     return ReplaceText (&fr) ;
     }

BOOL PopFindFindText (HWND hwndEdit, int *piSearchOffset, LPFINDREPLACE lpfr)
     {
     int         iPos ;
     LOCALHANDLE hLocal ;
     LPSTR       lpstrDoc, lpstrPos ;

               // Get a pointer to the edit document

     hLocal   = (HWND) SendMessage (hwndEdit, EM_GETHANDLE, 0, 0L) ;
     lpstrDoc = (LPSTR) LocalLock (hLocal) ;

               // Search the document for the find string

     lpstrPos = _fstrstr (lpstrDoc + *piSearchOffset, lpfr->lpstrFindWhat) ;
     LocalUnlock (hLocal) ;

               // Return an error code if the string cannot be found

     if (lpstrPos == NULL)
          return FALSE ;

               // Find the position in the document and the new start offset

     iPos = lpstrPos - lpstrDoc ;
     *piSearchOffset = iPos + _fstrlen (lpfr->lpstrFindWhat) ;

               // Select the found text

     SendMessage (hwndEdit, EM_SETSEL, 0,
                  MAKELONG (iPos, *piSearchOffset)) ;

     return TRUE ;
     }

BOOL PopFindNextText (HWND hwndEdit, int *piSearchOffset)
     {
     FINDREPLACE fr ;

     fr.lpstrFindWhat = szFindText ;

     return PopFindFindText (hwndEdit, piSearchOffset, &fr) ;
     }

BOOL PopFindReplaceText (HWND hwndEdit, int *piSearchOffset, LPFINDREPLACE lpfr)
     {
               // Find the text

     if (!PopFindFindText (hwndEdit, piSearchOffset, lpfr))
          return FALSE ;

               // Replace it

     SendMessage (hwndEdit, EM_REPLACESEL, 0, (long) lpfr->lpstrReplaceWith) ;

     return TRUE ;
     }

BOOL PopFindValidFind (void)
     {
     return *szFindText != '\0' ;
     }
