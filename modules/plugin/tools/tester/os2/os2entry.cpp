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

/*********************************************************/
/*                                                       */
/*            Main entry point for Windows               */
/*                                                       */
/*********************************************************/
#include <os2.h>

#include "xp.h"
#include "guiprefs.h"
#include "loadstatus.h"
#include "os2utils.h"

HMODULE hInst = NULL;
HWND hWndLoadStatus = NULL;

//Compiler run-time functions
extern "C" {
int  _CRT_init( void );
void _CRT_term( void );
void __ctordtorInit( void );
void __ctordtorTerm( void );
}

extern "C" unsigned long _System _DLL_InitTerm(unsigned long hModule, unsigned long ulFlag)
{
 APIRET rc; 

 switch (ulFlag) {
   case 0:
    // Init: Prime compiler run-time and construct static C++ objects.
     if ( _CRT_init() == -1 ) {
       return 0UL;
     } else {
       __ctordtorInit();
       hInst = hModule;
       char szFileName[_MAX_PATH];
       GetINIFileName(hInst, szFileName, sizeof(szFileName));
       char sz[256];
       XP_GetPrivateProfileString(SECTION_PREFERENCES, KEY_LOADSTATUS_WINDOW, ENTRY_NO, sz, sizeof(sz), szFileName);
       if (stricmp(sz, ENTRY_YES) == 0)
         hWndLoadStatus = ShowLoadStatus("Tester dll is loaded");
     }
     break;
     
   case 1 :
     __ctordtorTerm();
     _CRT_term();
     hInst = NULLHANDLE;
     if (hWndLoadStatus)
       DestroyLoadStatus(hWndLoadStatus);

     break;
     
   default  :
     return 0UL;
 }

 return 1;
}


