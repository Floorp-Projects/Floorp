/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John
 * Fairhurst.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All
 * Rights Reserved.
 *
 * Contributor(s): John Fairhurst <john_fairhurst@iname.com>
 *
 */
 
/* Quick test program to develop/demonstrate the directory picker */

#define INCL_DOS
#define INCL_WIN
#include <os2.h>
#include <stdio.h>

#include "nsDirPicker.h"

int main()
{
   HAB       hab;
   HMQ       hmq;
   DIRPICKER dirpicker = { "", 0, TRUE, 0 };
   HMODULE   hModRes;
   APIRET    rc;
   PTIB      ptib;
   PPIB      ppib;

   /* need to do this 'cos we don't link against NSPR */
   DosGetInfoBlocks( &ptib, &ppib);
   ppib->pib_ultype = 3;

   hab = WinInitialize(0);
   hmq = WinCreateMsgQueue( hab, 0);

   rc = DosLoadModule( 0, 0, "WDGTRES", &hModRes);

   if( !rc)
   {
   
      FS_PickDirectory( HWND_DESKTOP, HWND_DESKTOP, hModRes, &dirpicker);
   
      printf( "%s\n", dirpicker.szFullFile);
   
      DosFreeModule( hModRes);
   }
   else
   {
      printf( "Unable to load WDGTRES dll -- check running from $(DIST)/bin\n");
   }

   WinDestroyMsgQueue( hmq);
   WinTerminate( hab);

   return 0;
}
