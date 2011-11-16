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
#include "nsCycleCollectionParticipant.h"
#include "nsIInputStream.h"
#include "nsDetectionConfident.h"
#include "nsHtml5OwningUTF16Buffer.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5StreamParser.h"
#include "nsHtml5AtomTable.h"
#include "nsWeakReference.h"

class nsHtml5Parser : public nsIParser,
                      public nsSupportsWeakReference
{
  public:
    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsHtml5Parser, nsIParser)

    nsHtml5Parser();
    virtual ~nsHtml5Parser();

    /* Start nsIParser */
    /**
     * No-op for backwards compat.
     */
    NS_IMETHOD_(void) SetContentSink(nsIContentSink* aSink);

    /**
     * Returns the tree op executor for backwards compat.
     */
    NS_IMETHOD_(nsIContentSink*) GetContentSink();

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
     * Don't call. For interface compat only.
     */
    NS_IMETHOD_(void) GetDocumentCharset(nsACString& aCharset, PRInt32& aSource)
    {
      NS_NOTREACHED("No one should call this.");
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
     * Get the stream parser for this parser
     */
    virtual nsIStreamListener* GetStreamListener();

    /**
     * Don't call. For interface compat only.
     */
    NS_IMETHOD ContinueInterruptedParsing();

    /**
     * Blocks the parser.
     */
    NS_IMETHOD_(void) BlockParser();

    /**
     * Unblocks the parser.
     */
    NS_IMETHOD_(void) UnblockParser();

    /**
     * Query whether the parser is enabled (i.e. not blocked) or not.
     */
    NS_IMETHOD_(bool) IsParserEnabled();

    /**
     * Query whether the parser thinks it's done with parsing.
     */
    NS_IMETHOD_(bool) IsComplete();

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
     * @param   aContentType "text/html" for HTML mode, else text/plain mode
     * @param   aLastCall true if .close() false if .write()
     * @param   aMode ignored (for interface compat only)
     */
    NS_IMETHOD Parse(const nsAString& aSourceBuffer,
                     void* aKey,
                     const nsACString& aContentType,
                     bool aLastCall,
                     nsDTDMode aMode = eDTDMode_autodetect);

    /**
     * Gets the key passed to initial Parse()
     */
    NS_IMETHOD_(void *) GetRootContextKey();

    /**
     * Stops the parser prematurely
     */
    NS_IMETHOD Terminate();

    /**
     * Don't call. For interface backwards compat only.
     */
    NS_IMETHOD ParseFragment(const nsAString& aSourceBuffer,
                             nsTArray<nsString>& aTagStack);

    /**
     * Don't call. For interface compat only.
     */
    NS_IMETHOD BuildModel();

    /**
     * Don't call. For interface compat only.
     */
    NS_IMETHODIMP CancelParsingEvents();

    /**
     * Sets the state to initial values
     */
    virtual void Reset();
    
    /**
     * True in fragment mode and during synchronous document.write
     */
    virtual bool CanInterrupt();

    /**
     * True if the insertion point (per HTML5) is defined.
     */
    virtual bool IsInsertionPointDefined();

    /**
     * Call immediately before starting to evaluate a parser-inserted script.
     */
    virtual void BeginEvaluatingParserInsertedScript();

    /**
     * Call immediately after having evaluated a parser-inserted script.
     */
    virtual void EndEvaluatingParserInsertedScript();

    /**
     * Marks the HTML5 parser as not a script-created parser: Prepares the 
     * parser to be able to read a stream.
     *
     * @param aCommand the parser command (Yeah, this is bad API design. Let's
     * make this better when retiring nsIParser)
     */
    virtual void MarkAsNotScriptCreated(const char* aCommand);

    /**
     * True if this is a script-created HTML5 parser.
     */
    virtual bool IsScriptCreated();

    /* End nsIParser  */

    /**
     * Invoke the fragment parsing algorithm (innerHTML).
     *
     * @param aSourceBuffer the string being set as innerHTML
     * @param aTargetNode the target container
     * @param aContextLocalName local name of context node
     * @param aContextNamespace namespace of context node
     * @param aQuirks true to make <table> not close <p>
     * @param aPreventScriptExecution true to prevent scripts from executing;
     * don't set to false when parsing into a target node that has been bound
     * to tree.
     */
    nsresult ParseHtml5Fragment(const nsAString& aSourceBuffer,
                                nsIContent* aTargetNode,
                                nsIAtom* aContextLocalName,
                                PRInt32 aContextNamespace,
                                bool aQuirks,
                                bool aPreventScriptExecution);

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

    void InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState, PRInt32 aLine);

    void DropStreamParser() {
      if (mStreamParser) {
        mStreamParser->DropTimer();
        mStreamParser = nsnull;
      }
    }
    
    void StartTokenizer(bool aScriptingEnabled);
    
    void ContinueAfterFailedCharsetSwitch();

    nsHtml5StreamParser* GetStreamParser() {
      return mStreamParser;
    }

    /**
     * Parse until pending data is exhausted or a script blocks the parser
     */
    void ParseUntilBlocked();

  private:

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
     * The parser is in the fragment mode
     */
    bool                          mFragmentMode;

    /**
     * The parser is blocking on a script
     */
    bool                          mBlocked;

    /**
     * Whether the document.write() speculator is already active.
     */
    bool                          mDocWriteSpeculatorActive;
    
    /**
     * The number of parser-inserted script currently being evaluated.
     */
    PRInt32                       mParserInsertedScriptsBeingEvaluated;

    /**
     * True if document.close() has been called.
     */
    bool                          mDocumentClosed;

    bool                          mInDocumentWrite;

    // Gecko integration
    void*                         mRootContextKey;

    // Portable parser objects
    /**
     * The first buffer in the pending UTF-16 buffer queue
     */
    nsRefPtr<nsHtml5OwningUTF16Buffer>  mFirstBuffer;

    /**
     * The last buffer in the pending UTF-16 buffer queue. Always points
     * to a sentinel object with nsnull as its parser key.
     */
    nsHtml5OwningUTF16Buffer* mLastBuffer; // weak ref;

    /**
     * The tree operation executor
     */
    nsRefPtr<nsHtml5TreeOpExecutor>     mExecutor;

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
     * The stream parser.
     */
    nsRefPtr<nsHtml5StreamParser>       mStreamParser;

    /**
     *
     */
    PRInt32                             mRootContextLineNumber;
    
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
