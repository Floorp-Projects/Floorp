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
#include "nsIInputStream.h"
#include "nsDetectionConfident.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5StreamParser.h"

class nsHtml5Parser : public nsIParser {
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
    NS_IMETHOD GetStreamListener(nsIStreamListener** aListener);

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
     * Blocks the parser.
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

    /**
     *  Fired when the continue parse event is triggered.
     */
    void HandleParserContinueEvent(class nsHtml5ParserContinueEvent *);

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

    /**
     * Request event loop spin as soon as the tokenizer returns
     */
    void Suspend();
        
    inline nsHtml5Tokenizer* GetTokenizer() {
      return mTokenizer;
    }

    /**
     * Posts a continue event if there isn't one already
     */
    void MaybePostContinueEvent();
    
    void DropStreamParser() {
      mStreamParser = nsnull;
    }

  private:

    /**
     * Parse until pending data is exhausted or tree builder suspends
     */
    void ParseUntilSuspend();

    // State variables

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

    // script execution

    // Gecko integration
    void*                         mRootContextKey;
    nsIRunnable*                  mContinueEvent;  // weak ref

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
     * The tree operation executor
     */
    nsRefPtr<nsHtml5TreeOpExecutor> mExecutor;

    /**
     * The HTML5 tree builder
     */
    const nsAutoPtr<nsHtml5TreeBuilder> mTreeBuilder;

    /**
     * The HTML5 tokenizer
     */
    const nsAutoPtr<nsHtml5Tokenizer>   mTokenizer;

    /**
     * The stream parser.
     */
    nsRefPtr<nsHtml5StreamParser> mStreamParser;

};
#endif
