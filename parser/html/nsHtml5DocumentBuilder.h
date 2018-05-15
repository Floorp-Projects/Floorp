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

enum eHtml5FlushState
{
  eNotFlushing = 0, // not flushing
  eInFlush = 1,     // the Flush() method is on the call stack
  eInDocUpdate = 2, // inside an update batch on the document
};

class nsHtml5DocumentBuilder : public nsContentSink
{
  using Encoding = mozilla::Encoding;
  template<typename T>
  using NotNull = mozilla::NotNull<T>;

public:
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHtml5DocumentBuilder,
                                           nsContentSink)

  NS_DECL_ISUPPORTS_INHERITED

  inline void HoldElement(already_AddRefed<nsIContent> aContent)
  {
    *(mOwnedElements.AppendElement()) = aContent;
  }

  nsresult Init(nsIDocument* aDoc,
                nsIURI* aURI,
                nsISupports* aContainer,
                nsIChannel* aChannel);

  // Getters and setters for fields from nsContentSink
  nsIDocument* GetDocument() { return mDocument; }

  nsNodeInfoManager* GetNodeInfoManager() { return mNodeInfoManager; }

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
  inline nsresult IsBroken() { return mBroken; }

  inline bool IsComplete() { return !mParser; }

  inline void BeginDocUpdate()
  {
    MOZ_RELEASE_ASSERT(IsInFlush(), "Tried to double-open doc update.");
    MOZ_RELEASE_ASSERT(mParser, "Started doc update without parser.");
    mFlushState = eInDocUpdate;
    mDocument->BeginUpdate();
  }

  inline void EndDocUpdate()
  {
    MOZ_RELEASE_ASSERT(IsInDocUpdate(),
                       "Tried to end doc update without one open.");
    mFlushState = eInFlush;
    mDocument->EndUpdate();
  }

  inline void BeginFlush()
  {
    MOZ_RELEASE_ASSERT(mFlushState == eNotFlushing,
                       "Tried to start a flush when already flushing.");
    MOZ_RELEASE_ASSERT(mParser, "Started a flush without parser.");
    mFlushState = eInFlush;
  }

  inline void EndFlush()
  {
    MOZ_RELEASE_ASSERT(IsInFlush(), "Tried to end flush when not flushing.");
    mFlushState = eNotFlushing;
  }

  inline bool IsInDocUpdate() { return mFlushState == eInDocUpdate; }

  inline bool IsInFlush() { return mFlushState == eInFlush; }

  void SetDocumentCharsetAndSource(NotNull<const Encoding*> aEncoding,
                                   int32_t aCharsetSource);

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
  virtual void UpdateChildCounts() override;
  virtual nsresult FlushTags() override;

protected:
  explicit nsHtml5DocumentBuilder(bool aRunsToCompletion);
  virtual ~nsHtml5DocumentBuilder();

protected:
  AutoTArray<nsCOMPtr<nsIContent>, 32> mOwnedElements;
  /**
   * Non-NS_OK if this parser should refuse to process any more input.
   * For example, the parser needs to be marked as broken if it drops some
   * input due to a memory allocation failure. In such a case, the whole
   * parser needs to be marked as broken, because some input has been lost
   * and parsing more input could lead to a DOM where pieces of HTML source
   * that weren't supposed to become scripts become scripts.
   */
  nsresult mBroken;
  eHtml5FlushState mFlushState;
};

#endif // nsHtml5DocumentBuilder_h
