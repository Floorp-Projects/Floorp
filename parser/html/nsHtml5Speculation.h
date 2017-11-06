/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5Speculation_h
#define nsHtml5Speculation_h

#include "nsHtml5OwningUTF16Buffer.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5TreeOperation.h"
#include "nsAHtml5TreeOpSink.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

class nsHtml5Speculation final : public nsAHtml5TreeOpSink
{
  public:
    nsHtml5Speculation(nsHtml5OwningUTF16Buffer* aBuffer,
                       int32_t aStart, 
                       int32_t aStartLineNumber, 
                       nsAHtml5TreeBuilderState* aSnapshot);
    
    ~nsHtml5Speculation();

    nsHtml5OwningUTF16Buffer* GetBuffer()
    {
      return mBuffer;
    }
    
    int32_t GetStart()
    {
      return mStart;
    }

    int32_t GetStartLineNumber()
    {
      return mStartLineNumber;
    }
    
    nsAHtml5TreeBuilderState* GetSnapshot()
    {
      return mSnapshot;
    }

    /**
     * Flush the operations from the tree operations from the argument
     * queue unconditionally.
     */
    virtual void MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue) override;

    void FlushToSink(nsAHtml5TreeOpSink* aSink);

  private:
    /**
     * The first buffer in the pending UTF-16 buffer queue
     */
    RefPtr<nsHtml5OwningUTF16Buffer>  mBuffer;
    
    /**
     * The start index of this speculation in the first buffer
     */
    int32_t                             mStart;

    /**
     * The current line number at the start of the speculation
     */
    int32_t                             mStartLineNumber;
    
    nsAutoPtr<nsAHtml5TreeBuilderState> mSnapshot;

    nsTArray<nsHtml5TreeOperation>      mOpQueue;
};

#endif // nsHtml5Speculation_h
