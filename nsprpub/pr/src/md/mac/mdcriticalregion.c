/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: NULL; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *                George Warner, Apple Computer Inc.
 *                Simon Fraser  <sfraser@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "mdcriticalregion.h"

/*
  This code is a replacement for MPEnterCriticalRegion/MPLeaveCriticalRegion,
  which is broken on Mac OS 10.0.x builds, but fixed in 10.1. This code works
  everywhere.
*/


typedef struct MDCriticalRegionData_struct {
  MPTaskID        mMPTaskID;          /* Who's in the critical region? */
  UInt32          mDepthCount;        /* How deep? */
  MPSemaphoreID   mMPSemaphoreID;     /* ready semaphore */
} MDCriticalRegionData, *MDCriticalRegionDataPtr;


OSStatus
MD_CriticalRegionCreate(MDCriticalRegionID * outCriticalRegionID)
{
  MDCriticalRegionDataPtr newCriticalRegionPtr;
  MPSemaphoreID           mpSemaphoreID;
  OSStatus                err = noErr;

  if (outCriticalRegionID == NULL)
    return paramErr;

  *outCriticalRegionID = NULL;

  newCriticalRegionPtr = (MDCriticalRegionDataPtr)MPAllocateAligned(sizeof(MDCriticalRegionData),
                        kMPAllocateDefaultAligned, kMPAllocateClearMask);
  if (newCriticalRegionPtr == NULL)
    return memFullErr;

  // Note: this semaphore is pre-fired (ready!)
  err = MPCreateBinarySemaphore(&mpSemaphoreID);
  if (err == noErr)
  {
    newCriticalRegionPtr->mMPTaskID = kInvalidID;
    newCriticalRegionPtr->mDepthCount = 0;
    newCriticalRegionPtr->mMPSemaphoreID = mpSemaphoreID;

    *outCriticalRegionID = (MDCriticalRegionID)newCriticalRegionPtr;
  }
  else
  {
    MPFree((LogicalAddress)newCriticalRegionPtr);
  }

  return err;
}

OSStatus
MD_CriticalRegionDelete(MDCriticalRegionID inCriticalRegionID)
{
  MDCriticalRegionDataPtr criticalRegion = (MDCriticalRegionDataPtr)inCriticalRegionID;
  OSStatus                err = noErr;

  if (criticalRegion == NULL)
    return paramErr;

  if ((criticalRegion->mMPTaskID != kInvalidID) && (criticalRegion->mDepthCount > 0))
    return kMPInsufficientResourcesErr;

  if (criticalRegion->mMPSemaphoreID != kInvalidID)
    err = MPDeleteSemaphore(criticalRegion->mMPSemaphoreID);
  if (noErr != err) return err;

  criticalRegion->mMPSemaphoreID = kInvalidID;
  MPFree((LogicalAddress) criticalRegion);

  return noErr;
}

OSStatus
MD_CriticalRegionEnter(MDCriticalRegionID inCriticalRegionID, Duration inTimeout)
{
  MDCriticalRegionDataPtr criticalRegion = (MDCriticalRegionDataPtr)inCriticalRegionID;
  MPTaskID                currentTaskID = MPCurrentTaskID();
  OSStatus                err = noErr;

  if (criticalRegion == NULL)
    return paramErr;

  // if I'm inside the critical region...
  if (currentTaskID == criticalRegion->mMPTaskID)
  {
    // bump my depth
    criticalRegion->mDepthCount++;
    // and continue
    return noErr;
  }

  // wait for the ready semaphore
  err = MPWaitOnSemaphore(criticalRegion->mMPSemaphoreID, inTimeout);
  // we didn't get it. return the error
  if (noErr != err) return err;

  // we got it!
  criticalRegion->mMPTaskID = currentTaskID;
  criticalRegion->mDepthCount = 1;

  return noErr;
}

OSStatus
MD_CriticalRegionExit(MDCriticalRegionID inCriticalRegionID)
{
  MDCriticalRegionDataPtr   criticalRegion = (MDCriticalRegionDataPtr)inCriticalRegionID;
  MPTaskID                  currentTaskID = MPCurrentTaskID();
  OSStatus                  err = noErr;

  // if we don't own the critical region...
  if (currentTaskID != criticalRegion->mMPTaskID)
    return kMPInsufficientResourcesErr;

  // if we aren't at a depth...
  if (criticalRegion->mDepthCount == 0)
    return kMPInsufficientResourcesErr;

  // un-bump my depth
  criticalRegion->mDepthCount--;

  // if we just bottomed out...
  if (criticalRegion->mDepthCount == 0)
  {
    // release ownership of the structure
    criticalRegion->mMPTaskID = kInvalidID;
    // and signal the ready semaphore
    err = MPSignalSemaphore(criticalRegion->mMPSemaphoreID);
  }
  return err;
}

