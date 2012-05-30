/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5TreeOpStage.h"

nsHtml5TreeOpStage::nsHtml5TreeOpStage()
 : mMutex("nsHtml5TreeOpStage mutex")
{
}
    
nsHtml5TreeOpStage::~nsHtml5TreeOpStage()
{
}

void
nsHtml5TreeOpStage::MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue)
{
  mozilla::MutexAutoLock autoLock(mMutex);
  if (mOpQueue.IsEmpty()) {
    mOpQueue.SwapElements(aOpQueue);
  } else {
    mOpQueue.MoveElementsFrom(aOpQueue);
  }
}
    
void
nsHtml5TreeOpStage::MoveOpsAndSpeculativeLoadsTo(nsTArray<nsHtml5TreeOperation>& aOpQueue,
    nsTArray<nsHtml5SpeculativeLoad>& aSpeculativeLoadQueue)
{
  mozilla::MutexAutoLock autoLock(mMutex);
  if (aOpQueue.IsEmpty()) {
    mOpQueue.SwapElements(aOpQueue);
  } else {
    aOpQueue.MoveElementsFrom(mOpQueue);
  }
  if (aSpeculativeLoadQueue.IsEmpty()) {
    mSpeculativeLoadQueue.SwapElements(aSpeculativeLoadQueue);
  } else {
    aSpeculativeLoadQueue.MoveElementsFrom(mSpeculativeLoadQueue);
  }
}

void
nsHtml5TreeOpStage::MoveSpeculativeLoadsFrom(nsTArray<nsHtml5SpeculativeLoad>& aSpeculativeLoadQueue)
{
  mozilla::MutexAutoLock autoLock(mMutex);
  if (mSpeculativeLoadQueue.IsEmpty()) {
    mSpeculativeLoadQueue.SwapElements(aSpeculativeLoadQueue);
  } else {
    mSpeculativeLoadQueue.MoveElementsFrom(aSpeculativeLoadQueue);
  }
}

void
nsHtml5TreeOpStage::MoveSpeculativeLoadsTo(nsTArray<nsHtml5SpeculativeLoad>& aSpeculativeLoadQueue)
{
  mozilla::MutexAutoLock autoLock(mMutex);
  if (aSpeculativeLoadQueue.IsEmpty()) {
    mSpeculativeLoadQueue.SwapElements(aSpeculativeLoadQueue);
  } else {
    aSpeculativeLoadQueue.MoveElementsFrom(mSpeculativeLoadQueue);
  }
}

#ifdef DEBUG
void
nsHtml5TreeOpStage::AssertEmpty()
{
  mozilla::MutexAutoLock autoLock(mMutex);
  // This shouldn't really need the mutex
  NS_ASSERTION(mOpQueue.IsEmpty(), "The stage was supposed to be empty.");
}
#endif
