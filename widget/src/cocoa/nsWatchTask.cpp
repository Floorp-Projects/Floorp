/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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


#include "nsWatchTask.h"
#include <LowMem.h>
#include <Appearance.h>


// our global, as soon as the gfx code fragment loads, this will setup
// the VBL task automagically. 
nsWatchTask gWatchTask;


//
// GetTask
//
// A nice little getter to avoid exposing the global variable
nsWatchTask&
nsWatchTask :: GetTask ( )
{
  return gWatchTask;
}


nsWatchTask :: nsWatchTask ( )
  : mChecksum('mozz'), mSelf(this), mTicks(::TickCount()), mBusy(PR_FALSE), mSuspended(PR_FALSE),
     mInstallSucceeded(PR_FALSE), mAnimation(0)
{

}


nsWatchTask :: ~nsWatchTask ( ) 
{
#if !TARGET_CARBON
  if ( mInstallSucceeded )
    ::VRemove ( (QElemPtr)&mTask );
#endif
  InitCursor();
}


//
// Start
//
// Registers the VBL task and does other various init tasks to begin
// watching for time away from the event loop. It is ok to call other
// methods on this object w/out calling Start().
//
void
nsWatchTask :: Start ( )
{
#if !TARGET_CARBON
  // get the watch cursor and lock it high
  CursHandle watch = ::GetCursor ( watchCursor );
  if ( !watch )
    return;
  mWatchCursor = **watch;
  
  // setup the task
  mTask.qType = vType;
  mTask.vblAddr = NewVBLProc((VBLProcPtr)DoWatchTask);
  mTask.vblCount = kRepeatInterval;
  mTask.vblPhase = 0;
  
  // install it
  mInstallSucceeded = ::VInstall((QElemPtr)&mTask) == noErr;
#endif

} // Start


//
// DoWatchTask
//
// Called at vertical retrace. If we haven't been to the event loop for
// |kTicksToShowWatch| ticks, animate the cursor.
//
// Note: assumes that the VBLTask, mTask, is the first member variable, so
// that we can piggy-back off the pointer to get to the rest of our data.
//
// (Do we still need the check for LMGetCrsrBusy()? It's not in carbon...)
//
pascal void
nsWatchTask :: DoWatchTask ( nsWatchTask* inSelf )
{
  if ( inSelf->mChecksum == 'mozz' ) {
    if ( !inSelf->mSuspended  ) {
#if TARGET_CARBON
 	  PRBool busy = inSelf->mBusy;
#else
 	  PRBool busy = inSelf->mBusy && LMGetCrsrBusy();
#endif   
      if ( !busy ) {
        if ( ::TickCount() - inSelf->mTicks > kTicksToShowWatch ) {
          ::SetCursor ( &(inSelf->mWatchCursor) );
          inSelf->mBusy = PR_TRUE;
        }
      }
      else
        ::SetCursor ( &(inSelf->mWatchCursor) );
      
      // next frame in cursor animation    
      ++inSelf->mAnimation;
    }
    
#if !TARGET_CARBON
    // reset the task to fire again
    inSelf->mTask.vblCount = kRepeatInterval;
#endif    
  } // if valid checksum
  
} // DoWatchTask


//
// EventLoopReached
//
// Called every time we reach the event loop (or an event loop), this tickles
// our internal tick count to reset the time since our last visit to WNE and
// if we were busy, we're not any more.
//
void
nsWatchTask :: EventLoopReached ( )
{
  // reset the cursor if we were animating it
  if ( mBusy )
    ::InitCursor();
 
  mBusy = PR_FALSE;
  mTicks = ::TickCount();
  mAnimation = 0;

}

