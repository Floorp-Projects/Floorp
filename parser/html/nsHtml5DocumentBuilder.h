/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5DocumentBuilder_h
#define nsHtml5DocumentBuilder_h

#include "nsHtml5PendingNotification.h"

typedef nsIContent* nsIContentPtr;

enum eHtml5FlushState {
  eNotFlushing = 0,  // not flushing
  eInFlush = 1,      // the Flush() method is on the call stack
  eInDocUpdate = 2,  // inside an update batch on the document
  eNotifying = 3     // flushing pending append notifications
};

class nsHtml5DocumentBuilder : public nsContentSink
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHtml5DocumentBuilder,
                                           nsContentSink)

  NS_DECL_ISUPPORTS_INHERITED

  inline void HoldElement(nsIContent* aContent) {
    mOwnedElements.AppendElement(aContent);
  }

  inline bool HaveNotified(nsIContent* aNode) {
    NS_PRECONDITION(aNode, "HaveNotified called with null argument.");
    const nsHtml5PendingNotification* start = mPendingNotifications.Elements();
    const nsHtml5PendingNotification* end = start + mPendingNotifications.Length();
    for (;;) {
      nsIContent* parent = aNode->GetParent();
      if (!parent) {
        return true;
      }
      for (nsHtml5PendingNotification* iter = (nsHtml5PendingNotification*)start; iter < end; ++iter) {
        if (iter->Contains(parent)) {
          return iter->HaveNotifiedIndex(parent->IndexOf(aNode));
        }
      }
      aNode = parent;
    }
  }

  void PostPendingAppendNotification(nsIContent* aParent, nsIContent* aChild) {
    bool newParent = true;
    const nsIContentPtr* first = mElementsSeenInThisAppendBatch.Elements();
    const nsIContentPtr* last = first + mElementsSeenInThisAppendBatch.Length() - 1;
    for (const nsIContentPtr* iter = last; iter >= first; --iter) {
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
      sAppendBatchSlotsExamined++;
#endif
      if (*iter == aParent) {
        newParent = false;
        break;
      }
    }
    if (aChild->IsElement()) {
      mElementsSeenInThisAppendBatch.AppendElement(aChild);
    }
    mElementsSeenInThisAppendBatch.AppendElement(aParent);
    if (newParent) {
      mPendingNotifications.AppendElement(aParent);
    }
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
    sAppendBatchExaminations++;
#endif
  }

  void FlushPendingAppendNotifications() {
    NS_PRECONDITION(mFlushState == eInDocUpdate, "Notifications flushed outside update");
    mFlushState = eNotifying;
    const nsHtml5PendingNotification* start = mPendingNotifications.Elements();
    const nsHtml5PendingNotification* end = start + mPendingNotifications.Length();
    for (nsHtml5PendingNotification* iter = (nsHtml5PendingNotification*)start; iter < end; ++iter) {
      iter->Fire();
    }
    mPendingNotifications.Clear();
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
    if (mElementsSeenInThisAppendBatch.Length() > sAppendBatchMaxSize) {
      sAppendBatchMaxSize = mElementsSeenInThisAppendBatch.Length();
    }
#endif
    mElementsSeenInThisAppendBatch.Clear();
    NS_ASSERTION(mFlushState == eNotifying, "mFlushState out of sync");
    mFlushState = eInDocUpdate;
  }

  void DropHeldElements();

  // Getters and setters for fields from nsContentSink
  nsIDocument* GetDocument() {
    return mDocument;
  }
  nsNodeInfoManager* GetNodeInfoManager() {
    return mNodeInfoManager;
  }

  virtual bool BelongsToStringParser() = 0;

protected:
  inline void SetAppendBatchCapacity(uint32_t aCapacity)
  {
    mElementsSeenInThisAppendBatch.SetCapacity(aCapacity);
  }

private:
  nsTArray<nsHtml5PendingNotification> mPendingNotifications;
  nsTArray<nsCOMPtr<nsIContent> >      mOwnedElements;
  nsTArray<nsIContentPtr>              mElementsSeenInThisAppendBatch;
protected:
  eHtml5FlushState                     mFlushState;
};

#endif // nsHtml5DocumentBuilder_h
