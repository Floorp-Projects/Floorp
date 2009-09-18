/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NS_HTML5_PARSER__
#define NS_HTML5_PARSER__

#include "nsAutoPtr.h"
#include "nsIParser.h"
#include "nsDeque.h"
#include "nsIURL.h"
#include "nsParserCIID.h"
#include "nsITokenizer.h"
#include "nsThreadUtils.h"
#include "nsIContentSink.h"
#include "nsIParserFilter.h"
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsCOMArray.h"
#include "nsContentSink.h"
#include "nsIHTMLDocument.h"
#include "nsIUnicharStreamListener.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsIInputStream.h"
#include "nsIUnicodeDecoder.h"
#include "nsICharsetDetectionObserver.h"
#include "nsDetectionConfident.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5MetaScanner.h"

#define NS_HTML5_PARSER_READ_BUFFER_SIZE 1024
#define NS_HTML5_PARSER_SNIFFING_BUFFER_SIZE 512

enum eHtml5ParserLifecycle {
  /**
   * The parser has told the tokenizer to start yet.
   */
  NOT_STARTED = 0,

  /**
   * The parser has started the tokenizer and the stream hasn't ended yet.
   */
  PARSING = 1,

  /**
   * The parser hasn't told the tokenizer to emit EOF yet, but the network
   * stream has been exhausted or document.close() called.
   */
  STREAM_ENDING = 2,

  /**
   * The parser has told the tokenizer to emit EOF.
   */
  TERMINATED = 3
};

enum eBomState {
  /**
   * BOM sniffing hasn't started.
   */
  BOM_SNIFFING_NOT_STARTED = 0,

  /**
   * BOM sniffing is ongoing, and the first byte of an UTF-16LE BOM has been
   * seen.
   */
  SEEN_UTF_16_LE_FIRST_BYTE = 1,

  /**
   * BOM sniffing is ongoing, and the first byte of an UTF-16BE BOM has been
   * seen.
   */
  SEEN_UTF_16_BE_FIRST_BYTE = 2,

  /**
   * BOM sniffing is ongoing, and the first byte of an UTF-8 BOM has been
   * seen.
   */
  SEEN_UTF_8_FIRST_BYTE = 3,

  /**
   * BOM sniffing is ongoing, and the first and second bytes of an UTF-8 BOM
   * have been seen.
   */
  SEEN_UTF_8_SECOND_BYTE = 4,

  /**
   * BOM sniffing was started but is now over for whatever reason.
   */
  BOM_SNIFFING_OVER = 5
};

class nsHtml5Parser : public nsIParser,
                      public nsIStreamListener,
                      public nsICharsetDetectionObserver,
                      public nsIContentSink,
                      public nsContentSink {
  public:
    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHtml5Parser, nsContentSink)

    nsHtml5Parser();
    virtual ~nsHtml5Parser();

    /* Start nsIParser */
    /**
     * No-op for backwards compat.
     */
    NS_IMETHOD_(void) SetContentSink(nsIContentSink* aSink);

    /**
     * Returns |this| for backwards compat.
     */
    NS_IMETHOD_(nsIContentSink*) GetContentSink(void);

    /**
     * Always returns "view" for backwards compat.
     */
    NS_IMETHOD_(void) GetCommand(nsCString& aCommand);

    /**
     * No-op for backwards compat.
     */
    NS_IMETHOD_(void) SetCommand(const char* aCommand);

    /**
     * No-op for backwards compat.
     */
    NS_IMETHOD_(void) SetCommand(eParserCommands aParserCommand);

    /**
     *  Call this method once you've created a parser, and want to instruct it
     *  about what charset to load
     *
     *  @param   aCharset the charset of a document
     *  @param   aCharsetSource the source of the charset
     */
    NS_IMETHOD_(void) SetDocumentCharset(const nsACString& aCharset, PRInt32 aSource);

    /**
     * Getter for backwards compat.
     */
    NS_IMETHOD_(void) GetDocumentCharset(nsACString& aCharset, PRInt32& aSource)
    {
         aCharset = mCharset;
         aSource = mCharsetSource;
    }

    /**
     * No-op for backwards compat.
     */
    NS_IMETHOD_(void) SetParserFilter(nsIParserFilter* aFilter);

    /**
     * Get the channel associated with this parser
     * @param aChannel out param that will contain the result
     * @return NS_OK if successful or NS_NOT_AVAILABLE if not
     */
    NS_IMETHOD GetChannel(nsIChannel** aChannel);

    /**
     * Return |this| for backwards compat.
     */
    NS_IMETHOD GetDTD(nsIDTD** aDTD);

    /**
     * Unblocks parser and calls ContinueInterruptedParsing()
     */
    NS_IMETHOD        ContinueParsing();

    /**
     * If scripts are not executing, maybe flushes tree builder and parses
     * until suspension.
     */
    NS_IMETHOD        ContinueInterruptedParsing();

    /**
     * Don't call. For interface backwards compat only.
     */
    NS_IMETHOD_(void) BlockParser();

    /**
     * Unblocks the parser.
     */
    NS_IMETHOD_(void) UnblockParser();

    /**
     * Query whether the parser is enabled or not.
     */
    NS_IMETHOD_(PRBool) IsParserEnabled();

    /**
     * Query whether the parser thinks it's done with parsing.
     */
    NS_IMETHOD_(PRBool) IsComplete();

    /**
     * Set up request observer.
     *
     * @param   aURL ignored (for interface compat only)
     * @param   aListener a listener to forward notifications to
     * @param   aKey the root context key (used for document.write)
     * @param   aMode ignored (for interface compat only)
     */
    NS_IMETHOD Parse(nsIURI* aURL,
                     nsIRequestObserver* aListener = nsnull,
                     void* aKey = 0,
                     nsDTDMode aMode = eDTDMode_autodetect);

    /**
     * document.write and document.close
     *
     * @param   aSourceBuffer the argument of document.write (empty for .close())
     * @param   aKey a key unique to the script element that caused this call
     * @param   aContentType ignored (for interface compat only)
     * @param   aLastCall true if .close() false if .write()
     * @param   aMode ignored (for interface compat only)
     */
    NS_IMETHOD Parse(const nsAString& aSourceBuffer,
                     void* aKey,
                     const nsACString& aContentType,
                     PRBool aLastCall,
                     nsDTDMode aMode = eDTDMode_autodetect);

    /**
     * Gets the key passed to initial Parse()
     */
    NS_IMETHOD_(void *) GetRootContextKey();

    /**
     * Stops the parser prematurely
     */
    NS_IMETHOD        Terminate(void);

    /**
     * Don't call. For interface backwards compat only.
     */
    NS_IMETHOD ParseFragment(const nsAString& aSourceBuffer,
                             void* aKey,
                             nsTArray<nsString>& aTagStack,
                             PRBool aXMLMode,
                             const nsACString& aContentType,
                             nsDTDMode aMode = eDTDMode_autodetect);

    /**
     * Invoke the fragment parsing algorithm (innerHTML).
     *
     * @param aSourceBuffer the string being set as innerHTML
     * @param aTargetNode the target container (must QI to nsIContent)
     * @param aContextLocalName local name of context node
     * @param aContextNamespace namespace of context node
     * @param aQuirks true to make <table> not close <p>
     */
    NS_IMETHOD ParseFragment(const nsAString& aSourceBuffer,
                             nsISupports* aTargetNode,
                             nsIAtom* aContextLocalName,
                             PRInt32 aContextNamespace,
                             PRBool aQuirks);

    /**
     * Calls ParseUntilSuspend()
     */
    NS_IMETHOD BuildModel(void);

    /**
     *  Removes continue parsing events
     */
    NS_IMETHODIMP CancelParsingEvents();

    /**
     * Sets the state to initial values
     */
    virtual void Reset();
    
    /**
     * True in fragment mode and during synchronous document.write
     */
    virtual PRBool CanInterrupt();
    
    /* End nsIParser  */
     //*********************************************
      // These methods are callback methods used by
      // net lib to let us know about our inputstream.
      //*********************************************
    // nsIRequestObserver methods:
    NS_DECL_NSIREQUESTOBSERVER
    // nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER

    /**
     *  Fired when the continue parse event is triggered.
     */
    void HandleParserContinueEvent(class nsHtml5ParserContinueEvent *);

    // EncodingDeclarationHandler
    /**
     * Tree builder uses this to report a late <meta charset>
     */
    void internalEncodingDeclaration(nsString* aEncoding);

    // DocumentModeHandler
    /**
     * Tree builder uses this to report quirkiness of the document
     */
    void documentMode(nsHtml5DocumentMode m);

    // nsICharsetDetectionObserver
    /**
     * Chardet calls this to report the detection result
     */
    NS_IMETHOD Notify(const char* aCharset, nsDetectionConfident aConf);

    // nsIContentSink

    /**
     * Unimplemented. For interface compat only.
     */
    NS_IMETHOD WillParse();

    /**
     * Unimplemented. For interface compat only.
     */
    NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode);

    /**
     * Emits EOF.
     */
    NS_IMETHOD DidBuildModel();

    /**
     * Forwards to nsContentSink
     */
    NS_IMETHOD WillInterrupt();

    /**
     * Unimplemented. For interface compat only.
     */
    NS_IMETHOD WillResume();

    /**
     * Unimplemented. For interface compat only.
     */
    NS_IMETHOD SetParser(nsIParser* aParser);

    /**
     * No-op for backwards compat.
     */
    virtual void FlushPendingNotifications(mozFlushType aType);

    /**
     * Sets mCharset
     */
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset);

    /**
     * Returns the document.
     */
    virtual nsISupports *GetTarget();
    
    // Not from an external interface
  public:
    // nsContentSink methods
    virtual nsresult Initialize(nsIDocument* aDoc,
                        nsIURI* aURI,
                        nsISupports* aContainer,
                        nsIChannel* aChannel);
    virtual nsresult ProcessBASETag(nsIContent* aContent);
    virtual void UpdateChildCounts();
    virtual nsresult FlushTags();
    virtual void PostEvaluateScript(nsIScriptElement *aElement);
    using nsContentSink::Notify;
    // Non-inherited methods

    /**
     * <meta charset> scan failed. Try chardet if applicable. After this, the
     * the parser will have some encoding even if a last resolt fallback.
     *
     * @param aFromSegment The current network buffer or null if the sniffing
     *                     buffer is being flushed due to network stream ending.
     * @param aCount       The number of bytes in aFromSegment (ignored if
     *                     aFromSegment is null)
     * @param aWriteCount  Return value for how many bytes got read from the
     *                     buffer.
     * @param aCountToSniffingLimit The number of unfilled slots in
     *                              mSniffingBuffer
     */
    nsresult FinalizeSniffing(const PRUint8* aFromSegment,
                              PRUint32 aCount,
                              PRUint32* aWriteCount,
                              PRUint32 aCountToSniffingLimit);

    /**
     * Set up the Unicode decoder and write the sniffing buffer into it
     * followed by the current network buffer.
     *
     * @param aFromSegment The current network buffer or null if the sniffing
     *                     buffer is being flushed due to network stream ending.
     * @param aCount       The number of bytes in aFromSegment (ignored if
     *                     aFromSegment is null)
     * @param aWriteCount  Return value for how many bytes got read from the
     *                     buffer.
     */
    nsresult SetupDecodingAndWriteSniffingBufferAndCurrentSegment(const PRUint8* aFromSegment,
                                                                  PRUint32 aCount,
                                                                  PRUint32* aWriteCount);

    /**
     * Write the sniffing buffer into the Unicode decoder followed by the
     * current network buffer.
     *
     * @param aFromSegment The current network buffer or null if the sniffing
     *                     buffer is being flushed due to network stream ending.
     * @param aCount       The number of bytes in aFromSegment (ignored if
     *                     aFromSegment is null)
     * @param aWriteCount  Return value for how many bytes got read from the
     *                     buffer.
     */
    nsresult WriteSniffingBufferAndCurrentSegment(const PRUint8* aFromSegment,
                                                  PRUint32 aCount,
                                                  PRUint32* aWriteCount);

    /**
     * Initialize the Unicode decoder, mark the BOM as the source and
     * drop the sniffer.
     *
     * @param aCharsetName The charset name to report to the outside (UTF-16
     *                     or UTF-8)
     * @param aDecoderCharsetName The actual name for the decoder's charset
     *                            (UTF-16BE, UTF-16LE or UTF-8; the BOM has
     *                            been swallowed)
     */
    nsresult SetupDecodingFromBom(const char* aCharsetName,
                                  const char* aDecoderCharsetName);

    /**
     * True when there is a Unicode decoder already
     */
    PRBool HasDecoder() {
      return !!mUnicodeDecoder;
    }

    /**
     * Push bytes from network when there is no Unicode decoder yet
     */
    nsresult SniffStreamBytes(const PRUint8* aFromSegment,
                              PRUint32 aCount,
                              PRUint32* aWriteCount);

    /**
     * Push bytes from network when there is a Unicode decoder already
     */
    nsresult WriteStreamBytes(const PRUint8* aFromSegment,
                              PRUint32 aCount,
                              PRUint32* aWriteCount);

    /**
     * Request event loop spin as soon as the tokenizer returns
     */
    void Suspend();
    
    /**
     * Request execution of the script element when the tokenizer returns
     */
    void SetScriptElement(nsIContent* aScript);

    /**
     * Sets up style sheet load / parse
     */
    void UpdateStyleSheet(nsIContent* aElement);

    // Getters and setters for fields from nsContentSink
    nsIDocument* GetDocument() {
      return mDocument;
    }
    nsNodeInfoManager* GetNodeInfoManager() {
      return mNodeInfoManager;
    }
    nsIDocShell* GetDocShell() {
      return mDocShell;
    }

  private:
    /**
     * Runs mScriptElement
     */
    void ExecuteScript();

    /**
     * Posts a continue event if there isn't one already
     */
    void MaybePostContinueEvent();

    /**
     * Renavigates to the document with a different charset
     */
    nsresult PerformCharsetSwitch();

    /**
     * Parse until pending data is exhausted or tree builder suspends
     */
    void ParseUntilSuspend();

  private:
    // State variables
    /**
     * Call to PerformCharsetSwitch() needed
     */
    PRBool                        mNeedsCharsetSwitch;

    /**
     * Whether the last character tokenized was a carriage return (for CRLF)
     */
    PRBool                        mLastWasCR;

    /**
     * The parser is in the fragment mode
     */
    PRBool                        mFragmentMode;

    /**
     * The parser is blocking on a script
     */
    PRBool                        mBlocked;

    /**
     * The event loop will spin ASAP
     */
    PRBool                        mSuspending;
    
    /**
     * Whether EOF needs to be suppressed
     */
    PRBool                        mSuppressEOF;

    /**
     * The current point on parser life cycle
     */
    eHtml5ParserLifecycle         mLifeCycle;

    // script execution
    /**
     * Script to run ASAP
     */
    nsCOMPtr<nsIContent>          mScriptElement;

    /**
     * 
     */
    PRBool                        mUninterruptibleDocWrite;

    // Gecko integration
    void*                         mRootContextKey;
    nsCOMPtr<nsIRequest>          mRequest;
    nsCOMPtr<nsIRequestObserver>  mObserver;
    nsIRunnable*                  mContinueEvent;  // weak ref

    // encoding-related stuff
    /**
     * The source (confidence) of the character encoding in use
     */
    PRInt32                       mCharsetSource;

    /**
     * The character encoding in use
     */
    nsCString                     mCharset;

    /**
     * The character encoding to which to switch in a late <meta> renavigation
     */
    nsCString                     mPendingCharset;

    /**
     * The Unicode decoder
     */
    nsCOMPtr<nsIUnicodeDecoder>   mUnicodeDecoder;

    /**
     * The buffer for sniffing the character encoding
     */
    nsAutoArrayPtr<PRUint8>       mSniffingBuffer;

    /**
     * The number of meaningful bytes in mSniffingBuffer
     */
    PRUint32                      mSniffingLength;

    /**
     * BOM sniffing state
     */
    eBomState                     mBomState;

    /**
     * <meta> prescan implementation
     */
    nsAutoPtr<nsHtml5MetaScanner> mMetaScanner;

    // Portable parser objects
    /**
     * The first buffer in the pending UTF-16 buffer queue
     */
    nsHtml5UTF16Buffer*           mFirstBuffer; // manually managed strong ref

    /**
     * The last buffer in the pending UTF-16 buffer queue
     */
    nsHtml5UTF16Buffer*           mLastBuffer; // weak ref; always points to
                      // a buffer of the size NS_HTML5_PARSER_READ_BUFFER_SIZE

    /**
     * The HTML5 tree builder
     */
    nsAutoPtr<nsHtml5TreeBuilder> mTreeBuilder;

    /**
     * The HTML5 tokenizer
     */
    nsAutoPtr<nsHtml5Tokenizer>   mTokenizer;

#ifdef DEBUG
    /**
     * For asserting stream life cycle
     */
    eStreamState                  mStreamListenerState;
#endif

#ifdef GATHER_DOCWRITE_STATISTICS
    nsHtml5StateSnapshot*         mSnapshot;
    static PRUint32               sUnsafeDocWrites;
    static PRUint32               sTokenSafeDocWrites;
    static PRUint32               sTreeSafeDocWrites;
#endif
};
#endif
