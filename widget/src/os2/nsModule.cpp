/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 06/28/2000       IBM Corp.       Fixed querying of system keyboard
 * 
 */

#define INCL_DOSDEVIOCTL // for keyboard layout
#define INCL_DOSERRORS

#include "nsWidgetDefs.h"
#include "nsHashtable.h"
#include "resource.h"
#include <stdio.h>
#include <stdlib.h>
#include "nsIPref.h"
#include "nsIServiceManager.h"

// Module-level data & utility functions:
//       * unicode keycode & string conversion
//       * atom management

#include "nsDragService.h"
#include "nsAppShell.h"

#include <unidef.h>

#ifdef XP_OS2_VACPP
#define KBD_CTRL KBD_CONTROL
#endif

HMODULE gModuleHandle;

extern "C" {

/* _CRT_init is the C run-time environment initialization function.
 * It will return 0 to indicate success and -1 to indicate failure.
 */
int _CRT_init(void);

/* __ctordtorInit calls the C++ run-time constructors for static objects.
 */
void __ctordtorInit(void);

/* __ctordtorTerm calls the C++ run-time destructors for static objects.
 */
void __ctordtorTerm(void);
}

unsigned long _System _DLL_InitTerm(unsigned long mod_handle, unsigned long flag)
{
   if (flag == 0) {
      gModuleHandle = mod_handle;
      if ( _CRT_init() == -1 )
      {
        return(0UL);
      }
      __ctordtorInit();
   }
   else
   {
      gModuleHandle = 0;
      __ctordtorTerm();
   }
   return 1;
}

NS_IMPL_ISUPPORTS0(nsWidgetModuleData)

nsWidgetModuleData::nsWidgetModuleData()
{
   hModResources = NULLHANDLE;
}

// This is called when the first appshell is created.
void nsWidgetModuleData::Init( nsIAppShell *aPrimaevalAppShell)
{
   if( 0 != hModResources) return;

   char buffer[CCHMAXPATH];

   hModResources = gModuleHandle;

   szScreen.cx = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN);
   szScreen.cy = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN);

   // Work out if the system is DBCS
   COUNTRYCODE cc = { 0 };
   DosQueryDBCSEnv( CCHMAXPATH, &cc, buffer);
   bIsDBCS = buffer[0] || buffer[1];

   // keep a ref beyond the client app so we shut ourselves down properly.
   // don't do this for embedding where the appshell pointer is nsnull
   appshell = aPrimaevalAppShell;
   if (appshell != nsnull)
     NS_ADDREF(appshell);

#if 0
   mWindows = nsnull;
#endif

   for (int i=0;i<=16;i++ ) {
     hptrArray[i] = ::WinLoadPointer(HWND_DESKTOP, gModuleHandle, IDC_BASE+i);
   }
  nsresult rv;

  bIsTrackPoint = PR_FALSE;

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv) && prefs)
     prefs->GetBoolPref("os2.trackpoint", &bIsTrackPoint);
}

nsWidgetModuleData::~nsWidgetModuleData()
{
   NS_IF_RELEASE(dragService);

   for (int i=0;i<=16;i++ ) {
     WinDestroyPointer(hptrArray[i]);
   }

   // finally shut down the appshell.  No more PM.
   // (hope that gfxos2 has gone first!)
   // don't do this if appshell is nsnull for embedding
   if (appshell != nsnull)
     NS_IF_RELEASE(appshell);
}

const char *nsWidgetModuleData::DBCSstrchr( const char *string, int c )
{
   const char* p = string;
   do {
       if (*p == c)
           break;
       p  = WinNextChar(0,0,0,(char*)p);
   } while (*p); /* enddo */
   // Result is p or NULL
   return *p ? p : (char*)NULL;
}


const char *nsWidgetModuleData::DBCSstrrchr( const char *string, int c )
{
   int length = strlen(string);
   const char* p = string+length;
   do {
       if (*p == c)
           break;
       p  = WinPrevChar(0,0,0,(char*)string,(char*)p);
   } while (p > string); /* enddo */
   // Result is p or NULL
   return (*p == c) ? p : (char*)NULL;
}

nsWidgetModuleData *gWidgetModuleData = nsnull;

