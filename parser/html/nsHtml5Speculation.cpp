/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5Speculation.h"

using namespace mozilla;

nsHtml5Speculation::nsHtml5Speculation(nsHtml5OwningUTF16Buffer* aBuffer,
                                       int32_t aStart,
                                       int32_t aStartLineNumber,
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
  mOpQueue.AppendElements(std::move(aOpQueue));
}

void
nsHtml5Speculation::FlushToSink(nsAHtml5TreeOpSink* aSink)
{
  aSink->MoveOpsFrom(mOpQueue);
}
