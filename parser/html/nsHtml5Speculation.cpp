/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5Speculation.h"

using namespace mozilla;

nsHtml5Speculation::nsHtml5Speculation(nsHtml5OwningUTF16Buffer* aBuffer,
                                       int32_t aStart, int32_t aStartLineNumber,
                                       int32_t aStartColumnNumber,
                                       nsAHtml5TreeBuilderState* aSnapshot)
    : mBuffer(aBuffer),
      mStart(aStart),
      mStartLineNumber(aStartLineNumber),
      mStartColumnNumber(aStartColumnNumber),
      mSnapshot(aSnapshot) {
  MOZ_COUNT_CTOR(nsHtml5Speculation);
}

nsHtml5Speculation::~nsHtml5Speculation() {
  MOZ_COUNT_DTOR(nsHtml5Speculation);
}

[[nodiscard]] bool nsHtml5Speculation::MoveOpsFrom(
    nsTArray<nsHtml5TreeOperation>& aOpQueue) {
  return !!mOpQueue.AppendElements(std::move(aOpQueue), mozilla::fallible_t());
}

[[nodiscard]] bool nsHtml5Speculation::FlushToSink(nsAHtml5TreeOpSink* aSink) {
  return aSink->MoveOpsFrom(mOpQueue);
}
