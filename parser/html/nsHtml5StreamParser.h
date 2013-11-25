/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5StreamParser_h
#define nsHtml5StreamParser_h

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsICharsetDetectionObserver.h"
#include "nsHtml5MetaScanner.h"
#include "nsIUnicodeDecoder.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5OwningUTF16Buffer.h"
#include "nsIInputStream.h"
#include "mozilla/Mutex.h"
#include "nsHtml5AtomTable.h"
#include "nsHtml5Speculation.h"
#include "nsITimer.h"
#include "nsICharsetDetector.h"
#include "nsIThreadRetargetableStreamListener.h"

class nsHtml5Parser;

#define NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE 1024
#define NS_HTML5_STREAM_PARSER_SNIFFING_BUFFER_SIZE 1024

enum eParserMode {
  /**
   * Parse a document normally as HTML.
   */
  NORMAL,

  /**
   * View document as HTML source.
   */
  VIEW_SOURCE_HTML,

  /**
   * View document as XML source
   */
  VIEW_SOURCE_XML,

  /**
   * View document as plain text source
   */
  VIEW_SOURCE_PLAIN,

  /**
   * View document as plain text
   */
  PLAIN_TEXT,

  /**
   * Load as data (XHR)
   */
  LOAD_AS_DATA
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

enum eHtml5StreamState {
  STREAM_NOT_STARTED = 0,
  STREAM_BEING_READ = 1,
  STREAM_ENDED = 2
};

class nsHtml5StreamParser : public nsIStreamListener,
                            public nsIThreadRetargetableStreamListener,
                            public nsICharsetDetectionObserver {

  friend class nsHtml5RequestStopper;
  friend class nsHtml5DataAvailable;
  friend class nsHtml5StreamParserContinuation;
  friend class nsHtml5TimerKungFu;

  public:
    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsHtml5StreamParser, nsIStreamListener)

    static void InitializeStatics();

    nsHtml5StreamParser(nsHtml5TreeOpExecutor* aExecutor,
                        nsHtml5Parser* aOwner,
                        eParserMode aMode);
                        
    virtual ~nsHtml5StreamParser();

    // nsIRequestObserver methods:
    NS_DECL_NSIREQUESTOBSERVER
    // nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER
    // nsIThreadRetargetableStreamListener methods:
    NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
    
    // nsICharsetDetectionObserver
    /**
     * Chardet calls this to report the detection result
     */
    NS_IMETHOD Notify(const char* aCharset, nsDetectionConfident aConf);

    // EncodingDeclarationHandler
    // http://hg.mozilla.org/projects/htmlparser/file/tip/src/nu/validator/htmlparser/common/EncodingDeclarationHandler.java
    /**
     * Tree builder uses this to report a late <meta charset>
     */
    bool internalEncodingDeclaration(nsString* aEncoding);

    // Not from an external interface

    /**
     *  Call this method once you've created a parser, and want to instruct it
     *  about what charset to load
     *
     *  @param   aCharset the charset of a document
     *  @param   aCharsetSource the source of the charset
     */
    inline void SetDocumentCharset(const nsACString& aCharset, int32_t aSource) {
      NS_PRECONDITION(mStreamState == STREAM_NOT_STARTED,
                      "SetDocumentCharset called too late.");
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
      mCharset = aCharset;
      mCharsetSource = aSource;
    }
    
    inline void SetObserver(nsIRequestObserver* aObserver) {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
      mObserver = aObserver;
    }

    nsresult GetChannel(nsIChannel** aChannel);

    /**
     * The owner parser must call this after script execution
     * when no scripts are executing and the document.written 
     * buffer has been exhausted.
     */
    void ContinueAfterScripts(nsHtml5Tokenizer* aTokenizer, 
                              nsHtml5TreeBuilder* aTreeBuilder,
                              bool aLastWasCR);

    /**
     * Continues the stream parser if the charset switch failed.
     */
    void ContinueAfterFailedCharsetSwitch();

    void Terminate() {
      mozilla::MutexAutoLock autoLock(mTerminatedMutex);
      mTerminated = true;
    }
    
    void DropTimer();

    /**
     * Sets mCharset and mCharsetSource appropriately for the XML View Source
     * case if aEncoding names a supported rough ASCII superset and sets
     * the mCharset and mCharsetSource to the UTF-8 default otherwise.
     */
    void SetEncodingFromExpat(const PRUnichar* aEncoding);

    /**
     * Sets the URL for View Source title in case this parser ends up being
     * used for View Source. If aURL is a view-source: URL, takes the inner
     * URL. data: URLs are shown with an ellipsis instead of the actual data.
     */
    void SetViewSourceTitle(nsIURI* aURL);

  private:

#ifdef DEBUG
    bool IsParserThread() {
      bool ret;
      mThread->IsOnCurrentThread(&ret);
      return ret;
    }
#endif

    void MarkAsBroken();

    /**
     * Marks the stream parser as interrupted. If you ever add calls to this
     * method, be sure to review Uninterrupt usage very, very carefully to
     * avoid having a previous in-flight runnable cancel your Interrupt()
     * call on the other thread too soon.
     */
    void Interrupt() {
      mozilla::MutexAutoLock autoLock(mTerminatedMutex);
      mInterrupted = true;
    }

    void Uninterrupt() {
      NS_ASSERTION(IsParserThread(), "Wrong thread!");
      mTokenizerMutex.AssertCurrentThreadOwns();
      // Not acquiring mTerminatedMutex because mTokenizerMutex is already
      // held at this point and is already stronger.
      mInterrupted = false;      
    }

    /**
     * Flushes the tree ops from the tree builder and disarms the flush
     * timer.
     */
    void FlushTreeOpsAndDisarmTimer();

    void ParseAvailableData();
    
    void DoStopRequest();
    
    void DoDataAvailable(const uint8_t* aBuffer, uint32_t aLength);

    static NS_METHOD CopySegmentsToParser(nsIInputStream *aInStream,
                                          void *aClosure,
                                          const char *aFromSegment,
                                          uint32_t aToOffset,
                                          uint32_t aCount,
                                          uint32_t *aWriteCount);

    bool IsTerminatedOrInterrupted() {
      mozilla::MutexAutoLock autoLock(mTerminatedMutex);
      return mTerminated || mInterrupted;
    }

    bool IsTerminated() {
      mozilla::MutexAutoLock autoLock(mTerminatedMutex);
      return mTerminated;
    }

    /**
     * True when there is a Unicode decoder already
     */
    inline bool HasDecoder() {
      return !!mUnicodeDecoder;
    }

    /**
     * Push bytes from network when there is no Unicode decoder yet
     */
    nsresult SniffStreamBytes(const uint8_t* aFromSegment,
                              uint32_t aCount,
                              uint32_t* aWriteCount);

    /**
     * Push bytes from network when there is a Unicode decoder already
     */
    nsresult WriteStreamBytes(const uint8_t* aFromSegment,
                              uint32_t aCount,
                              uint32_t* aWriteCount);

    /**
     * Check whether every other byte in the sniffing buffer is zero.
     */
    void SniffBOMlessUTF16BasicLatin(const uint8_t* aFromSegment,
                                     uint32_t aCountToSniffingLimit);

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
    nsresult FinalizeSniffing(const uint8_t* aFromSegment,
                              uint32_t aCount,
                              uint32_t* aWriteCount,
                              uint32_t aCountToSniffingLimit);

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
    nsresult SetupDecodingAndWriteSniffingBufferAndCurrentSegment(const uint8_t* aFromSegment,
                                                                  uint32_t aCount,
                                                                  uint32_t* aWriteCount);

    /**
     * Initialize the Unicode decoder, mark the BOM as the source and
     * drop the sniffer.
     *
     * @param aDecoderCharsetName The name for the decoder's charset
     *                            (UTF-16BE, UTF-16LE or UTF-8; the BOM has
     *                            been swallowed)
     */
    nsresult SetupDecodingFromBom(const char* aDecoderCharsetName);

    /**
     * Become confident or resolve and encoding name to its preferred form.
     * @param aEncoding the value of an internal encoding decl. Acts as an
     *                  out param, too, when the method returns true.
     * @return true if the parser needs to start using the new value of
     *         aEncoding and false if the parser became confident or if
     *         the encoding name did not specify a usable encoding
     */
    bool PreferredForInternalEncodingDecl(nsACString& aEncoding);

    /**
     * Callback for mFlushTimer.
     */
    static void TimerCallback(nsITimer* aTimer, void* aClosure);

    /**
     * Parser thread entry point for (maybe) flushing the ops and posting
     * a flush runnable back on the main thread.
     */
    void TimerFlush();

    nsCOMPtr<nsIRequest>          mRequest;
    nsCOMPtr<nsIRequestObserver>  mObserver;

    /**
     * The document title to use if this turns out to be a View Source parser.
     */
    nsCString                     mViewSourceTitle;

    /**
     * The Unicode decoder
     */
    nsCOMPtr<nsIUnicodeDecoder>   mUnicodeDecoder;

    /**
     * The buffer for sniffing the character encoding
     */
    nsAutoArrayPtr<uint8_t>       mSniffingBuffer;

    /**
     * The number of meaningful bytes in mSniffingBuffer
     */
    uint32_t                      mSniffingLength;

    /**
     * BOM sniffing state
     */
    eBomState                     mBomState;

    /**
     * <meta> prescan implementation
     */
    nsAutoPtr<nsHtml5MetaScanner> mMetaScanner;

    // encoding-related stuff
    /**
     * The source (confidence) of the character encoding in use
     */
    int32_t                       mCharsetSource;

    /**
     * The character encoding in use
     */
    nsCString                     mCharset;

    /**
     * Whether reparse is forbidden
     */
    bool                          mReparseForbidden;

    // Portable parser objects
    /**
     * The first buffer in the pending UTF-16 buffer queue
     */
    nsRefPtr<nsHtml5OwningUTF16Buffer> mFirstBuffer;

    /**
     * The last buffer in the pending UTF-16 buffer queue
     */
    nsHtml5OwningUTF16Buffer*     mLastBuffer; // weak ref; always points to
                      // a buffer of the size NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE

    /**
     * The tree operation executor
     */
    nsHtml5TreeOpExecutor*        mExecutor;

    /**
     * The HTML5 tree builder
     */
    nsAutoPtr<nsHtml5TreeBuilder> mTreeBuilder;

    /**
     * The HTML5 tokenizer
     */
    nsAutoPtr<nsHtml5Tokenizer>   mTokenizer;

    /**
     * Makes sure the main thread can't mess the tokenizer state while it's
     * tokenizing. This mutex also protects the current speculation.
     */
    mozilla::Mutex                mTokenizerMutex;

    /**
     * The scoped atom table
     */
    nsHtml5AtomTable              mAtomTable;

    /**
     * The owner parser.
     */
    nsRefPtr<nsHtml5Parser>       mOwner;

    /**
     * Whether the last character tokenized was a carriage return (for CRLF)
     */
    bool                          mLastWasCR;

    /**
     * For tracking stream life cycle
     */
    eHtml5StreamState             mStreamState;
    
    /**
     * Whether we are speculating.
     */
    bool                          mSpeculating;

    /**
     * Whether the tokenizer has reached EOF. (Reset when stream rewinded.)
     */
    bool                          mAtEOF;

    /**
     * The speculations. The mutex protects the nsTArray itself.
     * To access the queue of current speculation, mTokenizerMutex must be 
     * obtained.
     * The current speculation is the last element
     */
    nsTArray<nsAutoPtr<nsHtml5Speculation> >  mSpeculations;
    mozilla::Mutex                            mSpeculationMutex;

    /**
     * True to terminate early; protected by mTerminatedMutex
     */
    bool                          mTerminated;
    bool                          mInterrupted;
    mozilla::Mutex                mTerminatedMutex;
    
    /**
     * The thread this stream parser runs on.
     */
    nsCOMPtr<nsIThread>           mThread;
    
    nsCOMPtr<nsIRunnable>         mExecutorFlusher;
    
    nsCOMPtr<nsIRunnable>         mLoadFlusher;

    /**
     * The chardet instance if chardet is enabled.
     */
    nsCOMPtr<nsICharsetDetector>  mChardet;

    /**
     * If false, don't push data to chardet.
     */
    bool                          mFeedChardet;

    /**
     * Whether the initial charset source was kCharsetFromParentFrame
     */
    bool                          mInitialEncodingWasFromParentFrame;

    /**
     * Timer for flushing tree ops once in a while when not speculating.
     */
    nsCOMPtr<nsITimer>            mFlushTimer;

    /**
     * Keeps track whether mFlushTimer has been armed. Unfortunately,
     * nsITimer doesn't enable querying this from the timer itself.
     */
    bool                          mFlushTimerArmed;

    /**
     * False initially and true after the timer has fired at least once.
     */
    bool                          mFlushTimerEverFired;

    /**
     * Whether the parser is doing a normal parse, view source or plain text.
     */
    eParserMode                   mMode;

    /**
     * The pref html5.flushtimer.initialdelay: Time in milliseconds between
     * the time a network buffer is seen and the timer firing when the
     * timer hasn't fired previously in this parse.
     */
    static int32_t                sTimerInitialDelay;

    /**
     * The pref html5.flushtimer.subsequentdelay: Time in milliseconds between
     * the time a network buffer is seen and the timer firing when the
     * timer has already fired previously in this parse.
     */
    static int32_t                sTimerSubsequentDelay;
};

#endif // nsHtml5StreamParser_h
