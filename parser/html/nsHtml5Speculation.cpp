/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5Speculation.h"

nsHtml5Speculation::nsHtml5Speculation(nsHtml5OwningUTF16Buffer* aBuffer,
                                       PRInt32 aStart, 
                                       PRInt32 aStartLineNumber, 
                                       nsAHtml5TreeBuilderState* aSnapshot)
  : mBuffer(aBuffer)
  , mStart(aStart)
  , mStartLineNumber(aStartLineNumber)
  , mSnapshot(aSnapshot)
{
  MOZ_COUNT_CTOR(nsHtml5Speculation);
}

nsHtml5Speculation::~nsHtml5Speculation()
{
  MOZ_COUNT_DTOR(nsHtml5Speculation);
}

void
nsHtml5Speculation::MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue)
{
  if (mOpQueue.IsEmpty()) {
    mOpQueue.SwapElements(aOpQueue);
    return;
  }
  mOpQueue.MoveElementsFrom(aOpQueue);
}

void
nsHtml5Speculation::FlushToSink(nsAHtml5TreeOpSink* aSink)
{
  aSink->MoveOpsFrom(mOpQueue);
}
