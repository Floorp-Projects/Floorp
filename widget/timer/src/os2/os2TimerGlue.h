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

#ifndef __os2TimerGlue_h
#define __os2TimerGlue_h

#include "nsHashtable.h"
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINHOOKS       /* to get HMQ_CURRENT */
#define INCL_WINTIMER
#include <os2.h>

extern TID QueryCurrentTID(void);

#define TIMERCLASS    ("WarpzillaTimerClass")
#define CALLBACK      _System
typedef ULONG         DWORD;
typedef BOOL          bool;
#define true          TRUE
#define false         FALSE

#define HEARTBEATTIMEOUT 50
#define HEARTBEATTIMERID 0x7ffe

struct TIDKey : public nsVoidKey
{
   TIDKey() : nsVoidKey((void*)QueryCurrentTID())
   {}
};

struct Lock
{
   HMTX mHmtx;
   Lock( HMTX hmtx) : mHmtx(hmtx)
   {
      WinRequestMutexSem( mHmtx, SEM_INDEFINITE_WAIT);
   }
  ~Lock()
   {
      DosReleaseMutexSem( mHmtx);
   }
};

// Data structure to hold and create nsTimerManagers.
// This is (meant to be) thread-safe.
class os2TimerGlue
{
   nsHashtable mTable;
   HMTX        mMutex;

 public:
   os2TimerGlue();
   ~os2TimerGlue();
   HWND   Get();
   UINT   SetTimer(HWND hWnd, UINT timerID, UINT aDelay);
   BOOL   KillTimer(HWND hWnd, UINT timerID);

};
#endif /* __os2TimerGlue_h */
