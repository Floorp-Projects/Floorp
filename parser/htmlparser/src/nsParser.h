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
#include "nsParserNode.h"
#include "nsIURL.h"
#include "CParserContext.h"
#include "nsParserCIID.h"
#include "nsITokenizer.h"
#include "nsHTMLTags.h"
#include "nsDTDUtils.h"
#include "nsThreadUtils.h"
#include "nsIContentSink.h"
#include "nsIParserFilter.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWeakReference.h"

class nsICharsetConverterManager;
class nsICharsetAlias;
class nsIDTD;
class nsScanner;
class nsSpeculativeScriptThread;
class nsIThreadPool;

#ifdef _MSC_VER
#pragma warning( disable : 4275 )
#endif


class nsParser : public nsIParser,
                 public nsIStreamListener,
                 public nsSupportsWeakReference
{
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
     * Destructor
     * @update	gess5/11/98
     */
    virtual ~nsParser();

    /**
     * Select given content sink into parser for parser output
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    NS_IMETHOD_(void) SetContentSink(nsIContentSink* aSink);

    /**
     * retrive the sink set into the parser 
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    NS_IMETHOD_(nsIContentSink*) GetContentSink(void);
    
    /**
     *  Call this method once you've created a parser, and want to instruct it
     *  about the command which caused the parser to be constructed. For example,
     *  this allows us to select a DTD which can do, say, view-source.
     *  
     *  @update  gess 3/25/98
     *  @param   aCommand -- ptrs to string that contains command
     *  @return	 nada
     */
    NS_IMETHOD_(void) GetCommand(nsCString& aCommand);
    NS_IMETHOD_(void) SetCommand(const char* aCommand);
    NS_IMETHOD_(void) SetCommand(eParserCommands aParserCommand);

    /**
     *  Call this method once you've created a parser, and want to instruct it
     *  about what charset to load
     *  
     *  @update  ftang 4/23/99
     *  @param   aCharset- the charset of a document
     *  @param   aCharsetSource- the source of the charset
     *  @return	 nada
     */
    NS_IMETHOD_(void) SetDocumentCharset(const nsACString& aCharset, PRInt32 aSource);

    NS_IMETHOD_(void) GetDocumentCharset(nsACString& aCharset, PRInt32& aSource)
    {
         aCharset = mCharset;
         aSource = mCharsetSource;
    }


    NS_IMETHOD_(void) SetParserFilter(nsIParserFilter* aFilter);

    /**
     * Cause parser to parse input from given URL 
     * @update	gess5/11/98
     * @param   aURL is a descriptor for source document
     * @param   aListener is a listener to forward notifications to
     * @return  TRUE if all went well -- FALSE otherwise
     */
    NS_IMETHOD Parse(nsIURI* aURL,
                     nsIRequestObserver* aListener = nsnull,
                     void* aKey = 0,
                     nsDTDMode aMode = eDTDMode_autodetect);

    /**
     * @update	gess5/11/98
     * @param   anHTMLString contains a string-full of real HTML
     * @param   appendTokens tells us whether we should insert tokens inline, or append them.
     * @return  TRUE if all went well -- FALSE otherwise
     */
    NS_IMETHOD Parse(const nsAString& aSourceBuffer,
                     void* aKey,
                     const nsACString& aContentType,
                     bool aLastCall,
                     nsDTDMode aMode = eDTDMode_autodetect);

    NS_IMETHOD_(void *) GetRootContextKey();

    /**
     * This method needs documentation
     */
    NS_IMETHOD ParseFragment(const nsAString& aSourceBuffer,
                             nsTArray<nsString>& aTagStack);
                             
    /**
     * This method gets called when the tokens have been consumed, and it's time
     * to build the model via the content sink.
     * @update	gess5/11/98
     * @return  YES if model building went well -- NO otherwise.
     */
    NS_IMETHOD BuildModel(void);

    NS_IMETHOD        ContinueInterruptedParsing();
    NS_IMETHOD_(void) BlockParser();
    NS_IMETHOD_(void) UnblockParser();
    NS_IMETHOD        Terminate(void);

    /**
     * Call this to query whether the parser is enabled or not.
     *
     *  @update  vidur 4/12/99
     *  @return  current state
     */
    NS_IMETHOD_(bool) IsParserEnabled();

    /**
     * Call this to query whether the parser thinks it's done with parsing.
     *
     *  @update  rickg 5/12/01
     *  @return  complete state
     */
    NS_IMETHOD_(bool) IsComplete();

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

    void              PushContext(CParserContext& aContext);
    CParserContext*   PopContext();
    CParserContext*   PeekContext() {return mParserContext;}

    /** 
     * Get the channel associated with this parser
     * @update harishd,gagan 07/17/01
     * @param aChannel out param that will contain the result
     * @return NS_OK if successful
     */
    NS_IMETHOD GetChannel(nsIChannel** aChannel);

    /** 
     * Get the DTD associated with this parser
     * @update vidur 9/29/99
     * @param aDTD out param that will contain the result
     * @return NS_OK if successful, NS_ERROR_FAILURE for runtime error
     */
    NS_IMETHOD GetDTD(nsIDTD** aDTD);
  
    /**
     * Get the nsIStreamListener for this parser
     * @param aDTD out param that will contain the result
     * @return NS_OK if successful
     */
    NS_IMETHOD GetStreamListener(nsIStreamListener** aListener);

    /** 
     * Detects the existence of a META tag with charset information in 
     * the given buffer.
     */
    bool DetectMetaTag(const char* aBytes, 
                         PRInt32 aLen, 
                         nsCString& oCharset, 
                         PRInt32& oCharsetSource);

    void SetSinkCharset(nsACString& aCharset);

    /**
     *  Removes continue parsing events
     *  @update  kmcclusk 5/18/98
     */

    NS_IMETHODIMP CancelParsingEvents();

    /**  
     *  Indicates whether the parser is in a state where it
     *  can be interrupted.
     *  @return PR_TRUE if parser can be interrupted, PR_FALSE if it can not be interrupted.
     *  @update  kmcclusk 5/18/98
     */
    virtual bool CanInterrupt();

    /**
     * Return true.
     */
    virtual bool IsInsertionPointDefined();

    /**
     * No-op.
     */
    virtual void BeginEvaluatingParserInsertedScript();

    /**
     * No-op.
     */
    virtual void EndEvaluatingParserInsertedScript();

    /**
     * No-op.
     */
    virtual void MarkAsNotScriptCreated();

    /**
     * Always false.
     */
    virtual bool IsScriptCreated();

    /**  
     *  Set to parser state to indicate whether parsing tokens can be interrupted
     *  @param aCanInterrupt PR_TRUE if parser can be interrupted, PR_FALSE if it can not be interrupted.
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
    void HandleParserContinueEvent(class nsParserContinueEvent *);

    static nsICharsetAlias* GetCharsetAliasService() {
      return sCharsetAliasService;
    }

    static nsICharsetConverterManager* GetCharsetConverterManager() {
      return sCharsetConverterManager;
    }

    virtual void Reset() {
      Cleanup();
      Initialize();
    }

    nsIThreadPool* ThreadPool() {
      return sSpeculativeThreadPool;
    }

    bool IsScriptExecuting() {
      return mSink && mSink->IsScriptExecuting();
    }

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

    void SpeculativelyParse();

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
     *  This is the tail-end of the code sandwich for the
     *  tokenization process. It gets called once tokenziation
     *  has completed.
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return  TRUE if all went well
     */
    bool DidTokenize(bool aIsFinalChunk = false);

protected:
    //*********************************************
    // And now, some data members...
    //*********************************************
    
      
    CParserContext*              mParserContext;
    nsCOMPtr<nsIDTD>             mDTD;
    nsCOMPtr<nsIRequestObserver> mObserver;
    nsCOMPtr<nsIContentSink>     mSink;
    nsIRunnable*                 mContinueEvent;  // weak ref
    nsRefPtr<nsSpeculativeScriptThread> mSpeculativeScriptThread;
   
    nsCOMPtr<nsIParserFilter> mParserFilter;
    nsTokenAllocator          mTokenAllocator;
    
    eParserCommands     mCommand;
    nsresult            mInternalState;
    PRInt32             mStreamStatus;
    PRInt32             mCharsetSource;
    
    PRUint16            mFlags;

    nsString            mUnusedInput;
    nsCString           mCharset;
    nsCString           mCommandStr;

    bool                mProcessingNetworkData;

    static nsICharsetAlias*            sCharsetAliasService;
    static nsICharsetConverterManager* sCharsetConverterManager;
    static nsIThreadPool*              sSpeculativeThreadPool;

    enum {
      kSpeculativeThreadLimit = 15,
      kIdleThreadLimit = 0,
      kIdleThreadTimeout = 50
    };
};

#endif 

