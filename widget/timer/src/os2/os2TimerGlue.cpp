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
 *   Pierre Phaneuf <pp@ludusdesign.com> 
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
 * 05/31/2000       IBM Corp.       Added to contain the majority of the
 *                                  differences btwn the windows and os/2
 *                                  timer code
 */

/* Most of this code came from nsTimerOS2.cpp at m13 time frame.  Moving to
 * seperate file so as to make the OS/2 code as physically similar to the
 * Windows code as possible.  This will make it much easier to compare the
 * two platforms' timer code to see what os/2 hasn't implemented as far
 * as fixes and features.  Code maintanance will be easier and this is a good
 * time to do it since the general mozilla timer code changed its structure
 * between m13 and m15      IBM-AKR
 */


#include "os2TimerGlue.h"

static long long timers = 0;

extern void CALLBACK FireTimeout(HWND aWindow, UINT aMessage, UINT aTimerID, 
                                 DWORD aTime);

PRBool PR_CALLBACK mgr_dtor( nsHashKey *aKey, void *aData, void */*aClosure*/)
{
   HWND hwnd = (HWND) aData;
   WinDestroyWindow( hwnd );
   return PR_TRUE;
}

MRESULT EXPENTRY fnwpTimer( HWND h, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mRC = 0;

   if( msg == WM_TIMER )
   {
      FireTimeout( h, msg, (UINT)mp1, 0 );
   }
   else
   {
      mRC = WinDefWindowProc( h, msg, mp1, mp2);
   }

   return mRC;
}

void EnsureWndClass()
{
   static BOOL bRegistered = FALSE;

   if( !bRegistered)
   {
      BOOL rc = WinRegisterClass( 0, TIMERCLASS, fnwpTimer, 0, 4);
      NS_ASSERTION(rc,"Can't register class");
      bRegistered = TRUE;
   }
}

HWND CreateWindow()
{
   HWND rethwnd = NULLHANDLE;

   if( FALSE == WinQueryQueueInfo( HMQ_CURRENT, 0, 0))
   {
      HAB hab = WinInitialize( 0);
      WinCreateMsgQueue( hab, 0);
   }
   // Register timer window class if we haven't done that yet.
   EnsureWndClass();

   // Create timer window
   rethwnd = WinCreateWindow( HWND_DESKTOP, TIMERCLASS, 0, 0, 0, 0, 0,
                                 0, HWND_DESKTOP, HWND_TOP, 0, 0, 0);
   NS_ASSERTION(rethwnd, "Couldn't create Timer window");
   return rethwnd;
}


os2TimerGlue::os2TimerGlue()
   : mMutex(0)
{
   DosCreateMutexSem( 0, &mMutex, 0, 0 /*unowned*/);
}

os2TimerGlue::~os2TimerGlue()
{
   mTable.Enumerate( &mgr_dtor);
   if( mMutex)
      DosCloseMutexSem( mMutex);
}

HWND os2TimerGlue::Get()
{
   Lock   lock(mMutex);
   TIDKey key;

   HWND hwnd = (HWND) mTable.Get( &key);

   if( hwnd == NULLHANDLE)
   {
      hwnd = CreateWindow();
      mTable.Put( &key, (void *)hwnd);
   }

   return hwnd;
}

UINT os2TimerGlue::SetTimer(HWND hWnd, UINT timerID, UINT aDelay)
{
   HWND timerHWND;
   timerHWND = Get();

   int i;
   // TODO - make this threadsafe
   for (i=0;i<64;i++) {
      if (!(timers & ((long long)1 << i))) {
         timers |= ((long long)1 << i);
         break;
      }
   }

   NS_ASSERTION(i < 64,"Too many timers");

   ULONG newTimerID = WinStartTimer(NULLHANDLE, timerHWND, i+0x1000, aDelay);
   return (UINT)newTimerID;
}

BOOL os2TimerGlue::KillTimer(HWND hWnd, UINT timerID)
{
   HWND timerHWND;
   timerHWND = Get();
   WinStopTimer(NULLHANDLE, timerHWND, timerID);

   timers ^= ((long long)1 << (timerID-0x1000));

   return TRUE;
}
