/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5Speculation_h__
#define nsHtml5Speculation_h__

#include "nsHtml5OwningUTF16Buffer.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5TreeOperation.h"
#include "nsAHtml5TreeOpSink.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"

class nsHtml5Speculation : public nsAHtml5TreeOpSink
{
  public:
    nsHtml5Speculation(nsHtml5OwningUTF16Buffer* aBuffer,
                       PRInt32 aStart, 
                       PRInt32 aStartLineNumber, 
                       nsAHtml5TreeBuilderState* aSnapshot);
    
    ~nsHtml5Speculation();

    nsHtml5OwningUTF16Buffer* GetBuffer() {
      return mBuffer;
    }
    
    PRInt32 GetStart() {
      return mStart;
    }

    PRInt32 GetStartLineNumber() {
      return mStartLineNumber;
    }
    
    nsAHtml5TreeBuilderState* GetSnapshot() {
      return mSnapshot;
    }

    /**
     * Flush the operations from the tree operations from the argument
     * queue unconditionally.
     */
    virtual void MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue);
    
    void FlushToSink(nsAHtml5TreeOpSink* aSink);

  private:
    /**
     * The first buffer in the pending UTF-16 buffer queue
     */
    nsRefPtr<nsHtml5OwningUTF16Buffer>  mBuffer;
    
    /**
     * The start index of this speculation in the first buffer
     */
    PRInt32                             mStart;

    /**
     * The current line number at the start of the speculation
     */
    PRInt32                             mStartLineNumber;
    
    nsAutoPtr<nsAHtml5TreeBuilderState> mSnapshot;

    nsTArray<nsHtml5TreeOperation>      mOpQueue;
};

#endif // nsHtml5Speculation_h__
