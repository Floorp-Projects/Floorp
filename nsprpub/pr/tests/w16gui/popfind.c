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
