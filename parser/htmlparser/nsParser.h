/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * MODULE NOTES:
 *
 *  This class does two primary jobs:
 *    1) It iterates the tokens provided during the
 *       tokenization process, identifing where elements
 *       begin and end (doing validation and normalization).
 *    2) It controls and coordinates with an instance of
 *       the IContentSink interface, to coordinate the
 *       the production of the content model.
 *
 *  The basic operation of this class assumes that an HTML
 *  document is non-normalized. Therefore, we don't process
 *  the document in a normalized way. Don't bother to look
 *  for methods like: doHead() or doBody().
 *
 *  Instead, in order to be backward compatible, we must
 *  scan the set of tokens and perform this basic set of
 *  operations:
 *    1)  Determine the token type (easy, since the tokens know)
 *    2)  Determine the appropriate section of the HTML document
 *        each token belongs in (HTML,HEAD,BODY,FRAMESET).
 *    3)  Insert content into our document (via the sink) into
 *        the correct section.
 *    4)  In the case of tags that belong in the BODY, we must
 *        ensure that our underlying document state reflects
 *        the appropriate context for our tag.
 *
 *        For example,if we see a <TR>, we must ensure our
 *        document contains a table into which the row can
 *        be placed. This may result in "implicit containers"
 *        created to ensure a well-formed document.
 *
 */

#ifndef NS_PARSER__
#define NS_PARSER__

#include "nsIParser.h"
#include "nsDeque.h"
#include "CParserContext.h"
#include "nsHTMLTags.h"
#include "nsIContentSink.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWeakReference.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

class nsIDTD;
class nsIRunnable;

#ifdef _MSC_VER
#  pragma warning(disable : 4275)
#endif

class nsParser final : public nsIParser,
                       public nsIStreamListener,
                       public nsSupportsWeakReference {
  /**
   * Destructor
   * @update  gess5/11/98
   */
  virtual ~nsParser();

 public:
  /**
   * Called on module init
   */
  static nsresult Init();

  /**
   * Called on module shutdown
   */
  static void Shutdown();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsParser, nsIParser)

  /**
   * default constructor
   * @update	gess5/11/98
   */
  nsParser();

  /**
   * Select given content sink into parser for parser output
   * @update	gess5/11/98
   * @param   aSink is the new sink to be used by parser
   * @return  old sink, or nullptr
   */
  NS_IMETHOD_(void) SetContentSink(nsIContentSink* aSink) override;

  /**
   * retrive the sink set into the parser
   * @update	gess5/11/98
   * @param   aSink is the new sink to be used by parser
   * @return  old sink, or nullptr
   */
  NS_IMETHOD_(nsIContentSink*) GetContentSink(void) override;

  /**
   *  Call this method once you've created a parser, and want to instruct it
   *  about the command which caused the parser to be constructed. For example,
   *  this allows us to select a DTD which can do, say, view-source.
   *
   *  @update  gess 3/25/98
   *  @param   aCommand -- ptrs to string that contains command
   *  @return	 nada
   */
  NS_IMETHOD_(void) GetCommand(nsCString& aCommand) override;
  NS_IMETHOD_(void) SetCommand(const char* aCommand) override;
  NS_IMETHOD_(void) SetCommand(eParserCommands aParserCommand) override;

  /**
   *  Call this method once you've created a parser, and want to instruct it
   *  about what charset to load
   *
   *  @update  ftang 4/23/99
   *  @param   aCharset- the charset of a document
   *  @param   aCharsetSource- the source of the charset
   *  @param   aChannelHadCharset- ignored
   *  @return	 nada
   */
  virtual void SetDocumentCharset(NotNull<const Encoding*> aCharset,
                                  int32_t aSource,
                                  bool aForceAutoDetection) override;

  NotNull<const Encoding*> GetDocumentCharset(int32_t& aSource) {
    aSource = mCharsetSource;
    return mCharset;
  }

  /**
   * Cause parser to parse input from given URL
   */
  NS_IMETHOD Parse(nsIURI* aURL) override;

  /**
   * This method gets called when you want to parse a fragment of XML surrounded
   * by the context |aTagStack|. It requires that the parser have been given a
   * fragment content sink.
   *
   * @param aSourceBuffer The XML that hasn't been parsed yet.
   * @param aTagStack The context of the source buffer.
   */
  nsresult ParseFragment(const nsAString& aSourceBuffer,
                         nsTArray<nsString>& aTagStack);

  NS_IMETHOD ContinueInterruptedParsing() override;
  NS_IMETHOD_(void) BlockParser() override;
  NS_IMETHOD_(void) UnblockParser() override;
  NS_IMETHOD_(void) ContinueInterruptedParsingAsync() override;
  NS_IMETHOD Terminate(void) override;

  /**
   * Call this to query whether the parser is enabled or not.
   *
   *  @update  vidur 4/12/99
   *  @return  current state
   */
  NS_IMETHOD_(bool) IsParserEnabled() override;

  /**
   * Call this to query whether the parser thinks it's done with parsing.
   *
   *  @update  rickg 5/12/01
   *  @return  complete state
   */
  NS_IMETHOD_(bool) IsComplete() override;

  /**
   * This method gets called (automatically) during incremental parsing
   * @update	gess5/11/98
   * @return  TRUE if all went well, otherwise FALSE
   */
  virtual nsresult ResumeParse(bool allowIteration = true,
                               bool aIsFinalChunk = false,
                               bool aCanInterrupt = true);

  //*********************************************
  // These methods are callback methods used by
  // net lib to let us know about our inputstream.
  //*********************************************
  // nsIRequestObserver methods:
  NS_DECL_NSIREQUESTOBSERVER

  // nsIStreamListener methods:
  NS_DECL_NSISTREAMLISTENER

  /**
   * Get the nsIStreamListener for this parser
   */
  virtual nsIStreamListener* GetStreamListener() override;

  void SetSinkCharset(NotNull<const Encoding*> aCharset);

  /**
   * Return true.
   */
  virtual bool IsInsertionPointDefined() override;

  /**
   * No-op.
   */
  void IncrementScriptNestingLevel() final;

  /**
   * No-op.
   */
  void DecrementScriptNestingLevel() final;

  bool HasNonzeroScriptNestingLevel() const final;

  /**
   * Always false.
   */
  virtual bool IsScriptCreated() override;

  /**
   * This is called when the final chunk has been
   * passed to the parser and the content sink has
   * interrupted token processing. It schedules
   * a ParserContinue PL_Event which will ask the parser
   * to HandleParserContinueEvent when it is handled.
   * @update	kmcclusk6/1/2001
   */
  nsresult PostContinueEvent();

  /**
   *  Fired when the continue parse event is triggered.
   *  @update  kmcclusk 5/18/98
   */
  void HandleParserContinueEvent(class nsParserContinueEvent*);

  void Reset() {
    Cleanup();
    mUnusedInput.Truncate();
    Initialize();
  }

  bool IsScriptExecuting() { return mSink && mSink->IsScriptExecuting(); }

  bool IsOkToProcessNetworkData() {
    return !IsScriptExecuting() && !mProcessingNetworkData;
  }

  // Returns Nothing() if we haven't determined yet what the parser is being
  // used for. Else returns whether this parser is used for parsing XML.
  mozilla::Maybe<bool> IsForParsingXML() {
    if (!mParserContext || mParserContext->mDTDMode == eDTDMode_autodetect) {
      return mozilla::Nothing();
    }

    return mozilla::Some(mParserContext->mDocType == eXML);
  }

 protected:
  void Initialize();
  void Cleanup();

  /**
   *
   * @update	gess5/18/98
   * @param
   * @return
   */
  nsresult WillBuildModel();

  /**
   * Called when parsing is done.
   */
  void DidBuildModel();

 private:
  /**
   * Pushes XML fragment parsing data to expat without an input stream.
   */
  nsresult Parse(const nsAString& aSourceBuffer, bool aLastCall);

 protected:
  //*********************************************
  // And now, some data members...
  //*********************************************

  mozilla::UniquePtr<CParserContext> mParserContext;
  nsCOMPtr<nsIDTD> mDTD;
  nsCOMPtr<nsIContentSink> mSink;
  nsIRunnable* mContinueEvent;  // weak ref

  eParserCommands mCommand;
  nsresult mInternalState;
  nsresult mStreamStatus;
  int32_t mCharsetSource;

  uint16_t mFlags;
  uint32_t mBlocked;

  nsString mUnusedInput;
  NotNull<const Encoding*> mCharset;
  nsCString mCommandStr;

  bool mProcessingNetworkData;
  bool mIsAboutBlank;
};

#endif
