/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_HTML5_PARSER
#define NS_HTML5_PARSER

#include "mozilla/UniquePtr.h"
#include "nsIParser.h"
#include "nsDeque.h"
#include "nsIContentSink.h"
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsCOMArray.h"
#include "nsContentSink.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHtml5OwningUTF16Buffer.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5StreamParser.h"
#include "nsHtml5AtomTable.h"
#include "nsWeakReference.h"
#include "nsHtml5StreamListener.h"
#include "nsCharsetSource.h"

class nsHtml5Parser final : public nsIParser, public nsSupportsWeakReference {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsHtml5Parser, nsIParser)

  nsHtml5Parser();

  /* Start nsIParser */
  /**
   * No-op for backwards compat.
   */
  NS_IMETHOD_(void) SetContentSink(nsIContentSink* aSink) override;

  /**
   * Returns the tree op executor for backwards compat.
   */
  NS_IMETHOD_(nsIContentSink*) GetContentSink() override;

  /**
   * Always returns "view" for backwards compat.
   */
  NS_IMETHOD_(void) GetCommand(nsCString& aCommand) override;

  /**
   * No-op for backwards compat.
   */
  NS_IMETHOD_(void) SetCommand(const char* aCommand) override;

  /**
   * No-op for backwards compat.
   */
  NS_IMETHOD_(void) SetCommand(eParserCommands aParserCommand) override;

  /**
   *  Call this method once you've created a parser, and want to instruct it
   *  about what charset to load
   *
   *  @param   aEncoding the charset of a document
   *  @param   aCharsetSource the source of the charset
   */
  virtual void SetDocumentCharset(NotNull<const Encoding*> aEncoding,
                                  int32_t aSource,
                                  bool aForceAutoDetection) override;

  /**
   * Get the channel associated with this parser
   * @param aChannel out param that will contain the result
   * @return NS_OK if successful or NS_NOT_AVAILABLE if not
   */
  nsresult GetChannel(nsIChannel** aChannel);

  /**
   * Get the stream parser for this parser
   */
  virtual nsIStreamListener* GetStreamListener() override;

  /**
   * Don't call. For interface compat only.
   */
  NS_IMETHOD ContinueInterruptedParsing() override;

  /**
   * Blocks the parser.
   */
  NS_IMETHOD_(void) BlockParser() override;

  /**
   * Unblocks the parser.
   */
  NS_IMETHOD_(void) UnblockParser() override;

  /**
   * Asynchronously continues parsing.
   */
  NS_IMETHOD_(void) ContinueInterruptedParsingAsync() override;

  /**
   * Query whether the parser is enabled (i.e. not blocked) or not.
   */
  NS_IMETHOD_(bool) IsParserEnabled() override;

  /**
   * Query whether the parser is closed (i.e. document.closed() is called) or
   * not.
   */
  NS_IMETHOD_(bool) IsParserClosed() override;

  /**
   * Query whether the parser thinks it's done with parsing.
   */
  NS_IMETHOD_(bool) IsComplete() override;

  /**
   * Set up request observer.
   *
   * @param   aURL used for View Source title
   */
  NS_IMETHOD Parse(nsIURI* aURL) override;

  /**
   * document.write and document.close
   *
   * @param   aSourceBuffer the argument of document.write (empty for .close())
   * @param   aKey a key unique to the script element that caused this call
   * @param   aLastCall true if .close() false if .write()
   */
  nsresult Parse(const nsAString& aSourceBuffer, void* aKey, bool aLastCall);

  /**
   * Stops the parser prematurely
   */
  NS_IMETHOD Terminate() override;

  /**
   * True if the insertion point (per HTML5) is defined.
   */
  virtual bool IsInsertionPointDefined() override;

  /**
   * Call immediately before starting to evaluate a parser-inserted script or
   * in general when the spec says to increment the script nesting level.
   */
  void IncrementScriptNestingLevel() final;

  /**
   * Call immediately after having evaluated a parser-inserted script or
   * generally want to restore to the state before the last
   * IncrementScriptNestingLevel call.
   */
  void DecrementScriptNestingLevel() final;

  /**
   * True if this is an HTML5 parser whose script nesting level (in
   * the sense of
   * <https://html.spec.whatwg.org/multipage/parsing.html#script-nesting-level>)
   * is nonzero.
   */
  bool HasNonzeroScriptNestingLevel() const final;

  /**
   * Marks the HTML5 parser as not a script-created parser: Prepares the
   * parser to be able to read a stream.
   *
   * @param aCommand the parser command (Yeah, this is bad API design. Let's
   * make this better when retiring nsIParser)
   */
  void MarkAsNotScriptCreated(const char* aCommand);

  /**
   * True if this is a script-created HTML5 parser.
   */
  virtual bool IsScriptCreated() override;

  /* End nsIParser  */

  // Not from an external interface
  // Non-inherited methods

 public:
  /**
   * Initializes the parser to load from a channel.
   */
  virtual nsresult Initialize(mozilla::dom::Document* aDoc, nsIURI* aURI,
                              nsISupports* aContainer, nsIChannel* aChannel);

  inline nsHtml5Tokenizer* GetTokenizer() { return mTokenizer.get(); }

  void InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState,
                                     int32_t aLine);

  void DropStreamParser() {
    if (GetStreamParser()) {
      GetStreamParser()->DropTimer();
      mStreamListener->DropDelegate();
      mStreamListener = nullptr;
    }
  }

  void StartTokenizer(bool aScriptingEnabled);

  void ContinueAfterFailedCharsetSwitch();

  nsHtml5StreamParser* GetStreamParser() {
    if (!mStreamListener) {
      return nullptr;
    }
    return mStreamListener->GetDelegate();
  }

  void PermanentlyUndefineInsertionPoint() {
    mInsertionPointPermanentlyUndefined = true;
  }

  /**
   * Parse until pending data is exhausted or a script blocks the parser
   */
  nsresult ParseUntilBlocked();

  /**
   * Start our executor.  This is meant to be used from document.open() _only_
   * and does some work similar to what nsHtml5StreamParser::OnStartRequest does
   * for normal parses.
   */
  nsresult StartExecutor();

 private:
  virtual ~nsHtml5Parser();

  // State variables

  /**
   * Whether the last character tokenized was a carriage return (for CRLF)
   */
  bool mLastWasCR;

  /**
   * Whether the last character tokenized was a carriage return (for CRLF)
   * when preparsing document.write.
   */
  bool mDocWriteSpeculativeLastWasCR;

  /**
   * The parser is blocking on the load of an external script from a web
   * page, or any number of extension content scripts.
   */
  uint32_t mBlocked;

  /**
   * Whether the document.write() speculator is already active.
   */
  bool mDocWriteSpeculatorActive;

  /**
   * The number of IncrementScriptNestingLevel calls we've seen without a
   * matching DecrementScriptNestingLevel.
   */
  int32_t mScriptNestingLevel;

  /**
   * True if document.close() has been called.
   */
  bool mDocumentClosed;

  bool mInDocumentWrite;

  /**
   * This is set when the tokenizer has seen EOF. The purpose is to
   * keep the insertion point undefined between the time the
   * parser has reached the point where it can't accept more input
   * and the time the document's mParser is set to nullptr.
   * Scripts can run during this time period due to an update
   * batch ending and due to various end-of-parse events firing.
   * (Setting mParser on the document to nullptr at the point
   * where this flag gets set to true would break things that for
   * legacy reasons assume that mParser on the document stays
   * non-null though the end-of-parse events.)
   */
  bool mInsertionPointPermanentlyUndefined;

  // Portable parser objects
  /**
   * The first buffer in the pending UTF-16 buffer queue
   */
  RefPtr<nsHtml5OwningUTF16Buffer> mFirstBuffer;

  /**
   * The last buffer in the pending UTF-16 buffer queue. Always points
   * to a sentinel object with nullptr as its parser key.
   */
  nsHtml5OwningUTF16Buffer* mLastBuffer;  // weak ref;

  /**
   * The tree operation executor
   */
  RefPtr<nsHtml5TreeOpExecutor> mExecutor;

  /**
   * The HTML5 tree builder
   */
  const mozilla::UniquePtr<nsHtml5TreeBuilder> mTreeBuilder;

  /**
   * The HTML5 tokenizer
   */
  const mozilla::UniquePtr<nsHtml5Tokenizer> mTokenizer;

  /**
   * Another HTML5 tree builder for preloading document.written content.
   */
  mozilla::UniquePtr<nsHtml5TreeBuilder> mDocWriteSpeculativeTreeBuilder;

  /**
   * Another HTML5 tokenizer for preloading document.written content.
   */
  mozilla::UniquePtr<nsHtml5Tokenizer> mDocWriteSpeculativeTokenizer;

  /**
   * The stream listener holding the stream parser.
   */
  RefPtr<nsHtml5StreamListener> mStreamListener;

  /**
   *
   */
  int32_t mRootContextLineNumber;

  /**
   * Whether it's OK to transfer parsing back to the stream parser
   */
  bool mReturnToStreamParserPermitted;

  /**
   * The scoped atom table
   */
  nsHtml5AtomTable mAtomTable;
};
#endif
