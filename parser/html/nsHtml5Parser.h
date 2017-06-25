/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_HTML5_PARSER
#define NS_HTML5_PARSER

#include "nsAutoPtr.h"
#include "nsIParser.h"
#include "nsDeque.h"
#include "nsIURL.h"
#include "nsParserCIID.h"
#include "nsITokenizer.h"
#include "nsIContentSink.h"
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsCOMArray.h"
#include "nsContentSink.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIInputStream.h"
#include "nsDetectionConfident.h"
#include "nsHtml5OwningUTF16Buffer.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5StreamParser.h"
#include "nsHtml5AtomTable.h"
#include "nsWeakReference.h"
#include "nsHtml5StreamListener.h"

class nsHtml5Parser final : public nsIParser,
                            public nsSupportsWeakReference
{
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
     *  @param   aCharset the charset of a document
     *  @param   aCharsetSource the source of the charset
     */
    NS_IMETHOD_(void) SetDocumentCharset(const nsACString& aCharset, int32_t aSource) override;

    /**
     * Don't call. For interface compat only.
     */
    NS_IMETHOD_(void) GetDocumentCharset(nsACString& aCharset, int32_t& aSource) override
    {
      NS_NOTREACHED("No one should call this.");
    }

    /**
     * Get the channel associated with this parser
     * @param aChannel out param that will contain the result
     * @return NS_OK if successful or NS_NOT_AVAILABLE if not
     */
    NS_IMETHOD GetChannel(nsIChannel** aChannel) override;

    /**
     * Return |this| for backwards compat.
     */
    NS_IMETHOD GetDTD(nsIDTD** aDTD) override;

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
     * Query whether the parser thinks it's done with parsing.
     */
    NS_IMETHOD_(bool) IsComplete() override;

    /**
     * Set up request observer.
     *
     * @param   aURL used for View Source title
     * @param   aListener a listener to forward notifications to
     * @param   aKey the root context key (used for document.write)
     * @param   aMode ignored (for interface compat only)
     */
    NS_IMETHOD Parse(nsIURI* aURL,
                     nsIRequestObserver* aListener = nullptr,
                     void* aKey = 0,
                     nsDTDMode aMode = eDTDMode_autodetect) override;

    /**
     * document.write and document.close
     *
     * @param   aSourceBuffer the argument of document.write (empty for .close())
     * @param   aKey a key unique to the script element that caused this call
     * @param   aContentType "text/html" for HTML mode, else text/plain mode
     * @param   aLastCall true if .close() false if .write()
     * @param   aMode ignored (for interface compat only)
     */
    nsresult Parse(const nsAString& aSourceBuffer,
                   void* aKey,
                   const nsACString& aContentType,
                   bool aLastCall,
                   nsDTDMode aMode = eDTDMode_autodetect);

    /**
     * Stops the parser prematurely
     */
    NS_IMETHOD Terminate() override;

    /**
     * Don't call. For interface backwards compat only.
     */
    NS_IMETHOD ParseFragment(const nsAString& aSourceBuffer,
                             nsTArray<nsString>& aTagStack) override;

    /**
     * Don't call. For interface compat only.
     */
    NS_IMETHOD BuildModel() override;

    /**
     * Don't call. For interface compat only.
     */
    NS_IMETHOD CancelParsingEvents() override;

    /**
     * Don't call. For interface compat only.
     */
    virtual void Reset() override;

    /**
     * True if the insertion point (per HTML5) is defined.
     */
    virtual bool IsInsertionPointDefined() override;

    /**
     * Call immediately before starting to evaluate a parser-inserted script or
     * in general when the spec says to define an insertion point.
     */
    virtual void PushDefinedInsertionPoint() override;

    /**
     * Call immediately after having evaluated a parser-inserted script or
     * generally want to restore to the state before the last
     * PushDefinedInsertionPoint call.
     */
    virtual void PopDefinedInsertionPoint() override;

    /**
     * Marks the HTML5 parser as not a script-created parser: Prepares the 
     * parser to be able to read a stream.
     *
     * @param aCommand the parser command (Yeah, this is bad API design. Let's
     * make this better when retiring nsIParser)
     */
    virtual void MarkAsNotScriptCreated(const char* aCommand) override;

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
    virtual nsresult Initialize(nsIDocument* aDoc,
                        nsIURI* aURI,
                        nsISupports* aContainer,
                        nsIChannel* aChannel);

    inline nsHtml5Tokenizer* GetTokenizer() {
      return mTokenizer;
    }

    void InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState, int32_t aLine);

    void DropStreamParser()
    {
      if (GetStreamParser()) {
        GetStreamParser()->DropTimer();
        mStreamListener->DropDelegate();
        mStreamListener = nullptr;
      }
    }
    
    void StartTokenizer(bool aScriptingEnabled);
    
    void ContinueAfterFailedCharsetSwitch();

    nsHtml5StreamParser* GetStreamParser()
    {
      if (!mStreamListener) {
        return nullptr;
      }
      return mStreamListener->GetDelegate();
    }

    /**
     * Parse until pending data is exhausted or a script blocks the parser
     */
    nsresult ParseUntilBlocked();

  private:

    virtual ~nsHtml5Parser();

    // State variables

    /**
     * Whether the last character tokenized was a carriage return (for CRLF)
     */
    bool                          mLastWasCR;

    /**
     * Whether the last character tokenized was a carriage return (for CRLF)
     * when preparsing document.write.
     */
    bool                          mDocWriteSpeculativeLastWasCR;

    /**
     * The parser is blocking on the load of an external script from a web
     * page, or any number of extension content scripts.
     */
    uint32_t                      mBlocked;

    /**
     * Whether the document.write() speculator is already active.
     */
    bool                          mDocWriteSpeculatorActive;
    
    /**
     * The number of PushDefinedInsertionPoint calls we've seen without a
     * matching PopDefinedInsertionPoint.
     */
    int32_t                       mInsertionPointPushLevel;

    /**
     * True if document.close() has been called.
     */
    bool                          mDocumentClosed;

    bool                          mInDocumentWrite;

    // Portable parser objects
    /**
     * The first buffer in the pending UTF-16 buffer queue
     */
    RefPtr<nsHtml5OwningUTF16Buffer>  mFirstBuffer;

    /**
     * The last buffer in the pending UTF-16 buffer queue. Always points
     * to a sentinel object with nullptr as its parser key.
     */
    nsHtml5OwningUTF16Buffer* mLastBuffer; // weak ref;

    /**
     * The tree operation executor
     */
    RefPtr<nsHtml5TreeOpExecutor>     mExecutor;

    /**
     * The HTML5 tree builder
     */
    const nsAutoPtr<nsHtml5TreeBuilder> mTreeBuilder;

    /**
     * The HTML5 tokenizer
     */
    const nsAutoPtr<nsHtml5Tokenizer>   mTokenizer;

    /**
     * Another HTML5 tree builder for preloading document.written content.
     */
    nsAutoPtr<nsHtml5TreeBuilder> mDocWriteSpeculativeTreeBuilder;

    /**
     * Another HTML5 tokenizer for preloading document.written content.
     */
    nsAutoPtr<nsHtml5Tokenizer>   mDocWriteSpeculativeTokenizer;

    /**
     * The stream listener holding the stream parser.
     */
    RefPtr<nsHtml5StreamListener>     mStreamListener;

    /**
     *
     */
    int32_t                             mRootContextLineNumber;
    
    /**
     * Whether it's OK to transfer parsing back to the stream parser
     */
    bool                                mReturnToStreamParserPermitted;

    /**
     * The scoped atom table
     */
    nsHtml5AtomTable                    mAtomTable;

};
#endif
