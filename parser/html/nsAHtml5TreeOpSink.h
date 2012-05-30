/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAHtml5TreeOpSink_h___
#define nsAHtml5TreeOpSink_h___

/**
 * The purpose of this interface is to connect a tree op executor 
 * (main-thread case), a tree op stage (non-speculative off-the-main-thread
 * case) or a speculation (speculative case).
 */
class nsAHtml5TreeOpSink {
  public:
  
    /**
     * Flush the operations from the tree operations from the argument
     * queue into this sink unconditionally.
     */
    virtual void MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue) = 0;
    
};

#endif /* nsAHtml5TreeOpSink_h___ */
