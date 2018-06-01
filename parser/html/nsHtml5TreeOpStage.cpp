/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5TreeOpStage.h"

using namespace mozilla;

nsHtml5TreeOpStage::nsHtml5TreeOpStage()
  : mMutex("nsHtml5TreeOpStage mutex")
{
}

nsHtml5TreeOpStage::~nsHtml5TreeOpStage() {}

void
nsHtml5TreeOpStage::MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue)
{
  mozilla::MutexAutoLock autoLock(mMutex);
  mOpQueue.AppendElements(std::move(aOpQueue));
}

void
nsHtml5TreeOpStage::MoveOpsAndSpeculativeLoadsTo(
  nsTArray<nsHtml5TreeOperation>& aOpQueue,
  nsTArray<nsHtml5SpeculativeLoad>& aSpeculativeLoadQueue)
{
  mozilla::MutexAutoLock autoLock(mMutex);
  aOpQueue.AppendElements(std::move(mOpQueue));
  aSpeculativeLoadQueue.AppendElements(std::move(mSpeculativeLoadQueue));
}

void
nsHtml5TreeOpStage::MoveSpeculativeLoadsFrom(
  nsTArray<nsHtml5SpeculativeLoad>& aSpeculativeLoadQueue)
{
  mozilla::MutexAutoLock autoLock(mMutex);
  mSpeculativeLoadQueue.AppendElements(std::move(aSpeculativeLoadQueue));
}

void
nsHtml5TreeOpStage::MoveSpeculativeLoadsTo(
  nsTArray<nsHtml5SpeculativeLoad>& aSpeculativeLoadQueue)
{
  mozilla::MutexAutoLock autoLock(mMutex);
  aSpeculativeLoadQueue.AppendElements(std::move(mSpeculativeLoadQueue));
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
