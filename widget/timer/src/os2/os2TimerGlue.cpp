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

TID QueryCurrentTID()
{
   PTIB pTib = 0;
   PPIB pPib = 0;
   DosGetInfoBlocks( &pTib, &pPib);
   return pTib->tib_ptib2->tib2_ultid;
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
   // Need a unique number for the timerID.  No way to get a unique timer ID
   //  without passing in a null hwnd.  Since there are already tests in place
   //  to ensure only one OS timer per nsTimer, this should be safe.  Could
   //  really do away with the AddTimer/GetTimer stuff with this, but want to
   //  keep this looking like Windows so will leave it in.  The only problem
   //  is that we have to keep this id below 0x7fff.  This is the easy way
   //  and likely to be safe.  If this doesn't work, we'll need to keep track
   //  of what ID's we've used and what is still open.  Hopefully these id's
   //  will be unique enough.   IBM-AKR
 
   ULONG ulTimerID = ((ULONG)timerID & TID_USERMAX);
 
   // Doing this fixer stuff since most values will be double word aligned.
   //    This will help make it a little more unique by potentially giving us
   //    3 times as many values.  IBM-AKR
   ULONG fixer = ((ULONG)timerID & 0x00018000) >> 15;
   ulTimerID = (ulTimerID | fixer);
 
   ULONG newTimerID = WinStartTimer(NULLHANDLE, timerHWND, ulTimerID, aDelay);
   return (UINT)newTimerID;
}

BOOL os2TimerGlue::KillTimer(HWND hWnd, UINT timerID)
{
   HWND timerHWND;
   timerHWND = Get();
   WinStopTimer(NULLHANDLE, timerHWND, timerID);

   return TRUE;
}
