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
#include "nsIURL.h"
#include "CParserContext.h"
#include "nsParserCIID.h"
#include "nsITokenizer.h"
#include "nsHTMLTags.h"
#include "nsIContentSink.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWeakReference.h"

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
   *  @return	 nada
   */
  virtual void SetDocumentCharset(NotNull<const Encoding*> aCharset,
                                  int32_t aSource) override;

  NotNull<const Encoding*> GetDocumentCharset(int32_t& aSource) {
    aSource = mCharsetSource;
    return mCharset;
  }

  /**
   * Cause parser to parse input from given URL
   * @update	gess5/11/98
   * @param   aURL is a descriptor for source document
   * @param   aListener is a listener to forward notifications to
   * @return  TRUE if all went well -- FALSE otherwise
   */
  NS_IMETHOD Parse(nsIURI* aURL, nsIRequestObserver* aListener = nullptr,
                   void* aKey = 0,
                   nsDTDMode aMode = eDTDMode_autodetect) override;

  /**
   * This method needs documentation
   */
  NS_IMETHOD ParseFragment(const nsAString& aSourceBuffer,
                           nsTArray<nsString>& aTagStack) override;

  /**
   * This method gets called when the tokens have been consumed, and it's time
   * to build the model via the content sink.
   * @update	gess5/11/98
   * @return  YES if model building went well -- NO otherwise.
   */
  NS_IMETHOD BuildModel(void) override;

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
   *  This rather arcane method (hack) is used as a signal between the
   *  DTD and the parser. It allows the DTD to tell the parser that content
   *  that comes through (parser::parser(string)) but not consumed should
   *  propagate into the next string based parse call.
   *
   *  @update  gess 9/1/98
   *  @param   aState determines whether we propagate unused string content.
   *  @return  current state
   */
  void SetUnusedInput(nsString& aBuffer);

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

  void PushContext(CParserContext& aContext);
  CParserContext* PopContext();
  CParserContext* PeekContext() { return mParserContext; }

  /**
   * Get the channel associated with this parser
   * @update harishd,gagan 07/17/01
   * @param aChannel out param that will contain the result
   * @return NS_OK if successful
   */
  NS_IMETHOD GetChannel(nsIChannel** aChannel) override;

  /**
   * Get the DTD associated with this parser
   * @update vidur 9/29/99
   * @param aDTD out param that will contain the result
   * @return NS_OK if successful, NS_ERROR_FAILURE for runtime error
   */
  NS_IMETHOD GetDTD(nsIDTD** aDTD) override;

  /**
   * Get the nsIStreamListener for this parser
   */
  virtual nsIStreamListener* GetStreamListener() override;

  void SetSinkCharset(NotNull<const Encoding*> aCharset);

  /**
   *  Removes continue parsing events
   *  @update  kmcclusk 5/18/98
   */

  NS_IMETHOD CancelParsingEvents() override;

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
   * No-op.
   */
  virtual void MarkAsNotScriptCreated(const char* aCommand) override;

  /**
   * Always false.
   */
  virtual bool IsScriptCreated() override;

  /**
   *  Set to parser state to indicate whether parsing tokens can be interrupted
   *  @param aCanInterrupt true if parser can be interrupted, false if it can
   *                       not be interrupted.
   *  @update  kmcclusk 5/18/98
   */
  void SetCanInterrupt(bool aCanInterrupt);

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

  virtual void Reset() override {
    Cleanup();
    Initialize();
  }

  bool IsScriptExecuting() { return mSink && mSink->IsScriptExecuting(); }

  bool IsOkToProcessNetworkData() {
    return !IsScriptExecuting() && !mProcessingNetworkData;
  }

 protected:
  void Initialize(bool aConstructor = false);
  void Cleanup();

  /**
   *
   * @update	gess5/18/98
   * @param
   * @return
   */
  nsresult WillBuildModel(nsString& aFilename);

  /**
   *
   * @update	gess5/18/98
   * @param
   * @return
   */
  nsresult DidBuildModel(nsresult anErrorCode);

 private:
  /*******************************************
    These are the tokenization methods...
   *******************************************/

  /**
   *  Part of the code sandwich, this gets called right before
   *  the tokenization process begins. The main reason for
   *  this call is to allow the delegate to do initialization.
   *
   *  @update  gess 3/25/98
   *  @param
   *  @return  TRUE if it's ok to proceed
   */
  bool WillTokenize(bool aIsFinalChunk = false);

  /**
   *  This is the primary control routine. It iteratively
   *  consumes tokens until an error occurs or you run out
   *  of data.
   *
   *  @update  gess 3/25/98
   *  @return  error code
   */
  nsresult Tokenize(bool aIsFinalChunk = false);

  /**
   * Pushes XML fragment parsing data to expat without an input stream.
   */
  nsresult Parse(const nsAString& aSourceBuffer, void* aKey, bool aLastCall);

 protected:
  //*********************************************
  // And now, some data members...
  //*********************************************

  CParserContext* mParserContext;
  nsCOMPtr<nsIDTD> mDTD;
  nsCOMPtr<nsIRequestObserver> mObserver;
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

nsresult nsParserInitialize();

#endif
