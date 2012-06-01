/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5TreeOpStage_h___
#define nsHtml5TreeOpStage_h___

#include "mozilla/Mutex.h"
#include "nsHtml5TreeOperation.h"
#include "nsTArray.h"
#include "nsAHtml5TreeOpSink.h"
#include "nsHtml5SpeculativeLoad.h"

class nsHtml5TreeOpStage : public nsAHtml5TreeOpSink {
  public:
  
    nsHtml5TreeOpStage();
    
    virtual ~nsHtml5TreeOpStage();
  
    /**
     * Flush the operations from the tree operations from the argument
     * queue unconditionally.
     */
    virtual void MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue);
    
    /**
     * Retrieve the staged operations and speculative loads into the arguments.
     */
    void MoveOpsAndSpeculativeLoadsTo(nsTArray<nsHtml5TreeOperation>& aOpQueue,
        nsTArray<nsHtml5SpeculativeLoad>& aSpeculativeLoadQueue);

    /**
     * Move the speculative loads from the argument into the staging queue.
     */
    void MoveSpeculativeLoadsFrom(nsTArray<nsHtml5SpeculativeLoad>& aSpeculativeLoadQueue);

    /**
     * Retrieve the staged speculative loads into the argument.
     */
    void MoveSpeculativeLoadsTo(nsTArray<nsHtml5SpeculativeLoad>& aSpeculativeLoadQueue);

#ifdef DEBUG
    void AssertEmpty();
#endif

  private:
    nsTArray<nsHtml5TreeOperation> mOpQueue;
    nsTArray<nsHtml5SpeculativeLoad> mSpeculativeLoadQueue;
    mozilla::Mutex mMutex;
    
};

#endif /* nsHtml5TreeOpStage_h___ */
