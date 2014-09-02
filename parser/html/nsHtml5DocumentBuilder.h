/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5DocumentBuilder_h
#define nsHtml5DocumentBuilder_h

#include "nsContentSink.h"
#include "nsHtml5DocumentMode.h"
#include "nsIDocument.h"
#include "nsIContent.h"

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

  inline void HoldElement(already_AddRefed<nsIContent> aContent)
  {
    *(mOwnedElements.AppendElement()) = aContent;
  }

  nsresult Init(nsIDocument* aDoc, nsIURI* aURI,
                nsISupports* aContainer, nsIChannel* aChannel);

  // Getters and setters for fields from nsContentSink
  nsIDocument* GetDocument()
  {
    return mDocument;
  }

  nsNodeInfoManager* GetNodeInfoManager()
  {
    return mNodeInfoManager;
  }

  /**
   * Marks this parser as broken and tells the stream parser (if any) to
   * terminate.
   *
   * @return aReason for convenience
   */
  virtual nsresult MarkAsBroken(nsresult aReason);

  /**
   * Checks if this parser is broken. Returns a non-NS_OK (i.e. non-0)
   * value if broken.
   */
  inline nsresult IsBroken()
  {
    return mBroken;
  }

  inline void BeginDocUpdate()
  {
    NS_PRECONDITION(mFlushState == eInFlush, "Tried to double-open update.");
    NS_PRECONDITION(mParser, "Started update without parser.");
    mFlushState = eInDocUpdate;
    mDocument->BeginUpdate(UPDATE_CONTENT_MODEL);
  }

  inline void EndDocUpdate()
  {
    NS_PRECONDITION(mFlushState != eNotifying, "mFlushState out of sync");
    if (mFlushState == eInDocUpdate) {
      mFlushState = eInFlush;
      mDocument->EndUpdate(UPDATE_CONTENT_MODEL);
    }
  }

  bool IsInDocUpdate()
  {
    return mFlushState == eInDocUpdate;
  }

  void SetDocumentCharsetAndSource(nsACString& aCharset, int32_t aCharsetSource);

  /**
   * Sets up style sheet load / parse
   */
  void UpdateStyleSheet(nsIContent* aElement);

  void SetDocumentMode(nsHtml5DocumentMode m);

  void SetNodeInfoManager(nsNodeInfoManager* aManager)
  {
    mNodeInfoManager = aManager;
  }

  // nsContentSink methods
  virtual void UpdateChildCounts();
  virtual nsresult FlushTags();

protected:

  explicit nsHtml5DocumentBuilder(bool aRunsToCompletion);
  virtual ~nsHtml5DocumentBuilder();

protected:
  nsAutoTArray<nsCOMPtr<nsIContent>, 32> mOwnedElements;
  /**
   * Non-NS_OK if this parser should refuse to process any more input.
   * For example, the parser needs to be marked as broken if it drops some
   * input due to a memory allocation failure. In such a case, the whole
   * parser needs to be marked as broken, because some input has been lost
   * and parsing more input could lead to a DOM where pieces of HTML source
   * that weren't supposed to become scripts become scripts.
   *
   * Since NS_OK is actually 0, zeroing operator new takes care of
   * initializing this.
   */
  nsresult                             mBroken;
  eHtml5FlushState                     mFlushState;
};

#endif // nsHtml5DocumentBuilder_h
