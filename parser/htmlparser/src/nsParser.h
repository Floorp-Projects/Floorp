/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
/**
 * MODULE NOTES:
 * @update  gess 4/1/98
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
#include "nsTimer.h"
#include "nsIProgressEventSink.h"
#include "nsIEventQueue.h"

class IContentSink;
class nsIDTD;
class nsScanner;
class nsIParserFilter;
class nsIProgressEventSink;

#ifdef XP_WIN
#pragma warning( disable : 4275 )
#endif


class nsParser : public nsIParser,
                 public nsIStreamListener{

  
  public:
    friend class CTokenHandler;
    static void FreeSharedObjects(void);

    NS_DECL_ISUPPORTS


    /**
     * default constructor
     * @update	gess5/11/98
     */
    nsParser(nsITokenObserver* anObserver=0);


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
    virtual nsIContentSink* SetContentSink(nsIContentSink* aSink);

    /**
     * retrive the sink set into the parser 
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* GetContentSink(void);
    
    /**
     *  Call this method once you've created a parser, and want to instruct it
	   *  about the command which caused the parser to be constructed. For example,
     *  this allows us to select a DTD which can do, say, view-source.
     *  
     *  @update  gess 3/25/98
     *  @param   aContentSink -- ptr to content sink that will receive output
     *  @return	 ptr to previously set contentsink (usually null)  
     */
    virtual void GetCommand(nsString& aCommand);
    virtual void SetCommand(const char* aCommand);
    virtual void SetCommand(eParserCommands aParserCommand);

    /**
     *  Call this method once you've created a parser, and want to instruct it
     *  about what charset to load
     *  
     *  @update  ftang 4/23/99
     *  @param   aCharset- the charest of a document
     *  @param   aCharsetSource- the soure of the chares
     *  @return	 nada
     */
    virtual void SetDocumentCharset(nsString& aCharset, nsCharsetSource aSource);

    void GetDocumentCharset(nsString& oCharset, nsCharsetSource& oSource)
    {
         oCharset = mCharset;
         oSource = mCharsetSource;
    }


    virtual nsIParserFilter* SetParserFilter(nsIParserFilter* aFilter);
    
    virtual void RegisterDTD(nsIDTD* aDTD);

    /**
     *  Retrieve the scanner from the topmost parser context
     *  
     *  @update  gess 6/9/98
     *  @return  ptr to scanner
     */
    virtual nsDTDMode GetParseMode(void);

    /**
     *  Retrieve the scanner from the topmost parser context
     *  
     *  @update  gess 6/9/98
     *  @return  ptr to scanner
     */
    virtual nsScanner* GetScanner(void);

    /**
     * Cause parser to parse input from given URL 
     * @update	gess5/11/98
     * @param   aURL is a descriptor for source document
     * @param   aListener is a listener to forward notifications to
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual nsresult Parse(nsIURI* aURL,nsIRequestObserver* aListener,PRBool aEnableVerify=PR_FALSE,void* aKey=0,nsDTDMode aMode=eDTDMode_autodetect);

    /**
     * Cause parser to parse input from given stream 
     * @update	gess5/11/98
     * @param   aStream is the i/o source
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual nsresult Parse(nsIInputStream& aStream,const nsAReadableString& aMimeType,PRBool aEnableVerify=PR_FALSE,void* aKey=0,nsDTDMode aMode=eDTDMode_autodetect);

    /**
     * @update	gess5/11/98
     * @param   anHTMLString contains a string-full of real HTML
     * @param   appendTokens tells us whether we should insert tokens inline, or append them.
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual nsresult Parse(const nsAReadableString& aSourceBuffer,void* aKey,const nsAReadableString& aContentType,PRBool aEnableVerify=PR_FALSE,PRBool aLastCall=PR_FALSE,nsDTDMode aMode=eDTDMode_autodetect);

    virtual nsresult  ParseFragment(const nsAReadableString& aSourceBuffer,
                                    void* aKey,
                                    nsVoidArray& aTagStack,
                                    PRUint32 anInsertPos,
                                    const nsAReadableString& aContentType,
                                    nsDTDMode aMode=eDTDMode_autodetect);


    /**
     *  Call this when you want control whether or not the parser will parse
     *  and tokenize input (TRUE), or whether it just caches input to be 
     *  parsed later (FALSE).
     *  
     *  @update  gess 9/1/98
     *  @param   aState determines whether we parse/tokenize or just cache.
     *  @return  current state
     */
    virtual nsresult  ContinueParsing();
    virtual void      BlockParser();
    virtual void      UnblockParser();
    virtual nsresult  Terminate(void);

    /**
     * Call this to query whether the parser is enabled or not.
     *
     *  @update  vidur 4/12/99
     *  @return  current state
     */
    virtual PRBool    IsParserEnabled();

    /**
     * Call this to query whether the parser thinks it's done with parsing.
     *
     *  @update  rickg 5/12/01
     *  @return  complete state
     */
    virtual PRBool    IsComplete();

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
    virtual nsresult ResumeParse(PRBool allowIteration=PR_TRUE, PRBool aIsFinalChunk=PR_FALSE);

#ifdef DEBUG
    void  DebugDumpSource(nsOutputStream& anOutput);
#endif

     //*********************************************
      // These methods are callback methods used by
      // net lib to let us know about our inputstream.
      //*********************************************
    NS_DECL_NSIPROGRESSEVENTSINK

    // nsIRequestObserver methods:
    NS_DECL_NSIREQUESTOBSERVER

    // nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER

    void              PushContext(CParserContext& aContext);
    CParserContext*   PopContext();
    CParserContext*   PeekContext() {return mParserContext;}

    /**
     * 
     * @update	gess 1/22/99
     * @param 
     * @return
     */
    virtual nsITokenizer* GetTokenizer(void);

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
     *  Call this method to determine a DTD for a DOCTYPE
     *  
     *  @update  harishd 05/01/00
     *  @param   aDTD  -- Carries the deduced ( from DOCTYPE ) DTD.
     *  @param   aDocTypeStr -- A doctype for which a DTD is to be selected.
     *  @param   aMimeType   -- A mimetype for which a DTD is to be selected.
                                Note: aParseMode might be required.
     *  @param   aCommand    -- A command for which a DTD is to be selected.
     *  @param   aParseMode  -- Used with aMimeType to choose the correct DTD.
     *  @return  NS_OK if succeeded else ERROR.
     */
    NS_IMETHOD CreateCompatibleDTD(nsIDTD** aDTD, 
                                   nsString* aDocTypeStr,
                                   eParserCommands aCommand,
                                   const nsString* aMimeType=nsnull, 
                                   nsDTDMode aDTDMode=eDTDMode_unknown);

    /** 
     * Detects the existence of a META tag with charset information in 
     * the given buffer.
     */
    PRBool DetectMetaTag(const char* aBytes, 
                         PRInt32 aLen, 
                         nsString& oCharset, 
                         nsCharsetSource& oCharsetSource);

    void SetSinkCharset(nsAWritableString& aCharset);

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
    PRBool CanInterrupt(void);

    /**  
     *  Set to parser state to indicate whether parsing tokens can be interrupted
     *  @param aCanInterrupt PR_TRUE if parser can be interrupted, PR_FALSE if it can not be interrupted.
     *  @update  kmcclusk 5/18/98
     */
    void SetCanInterrupt(PRBool aCanInterrupt);

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
    void HandleParserContinueEvent(void);

protected:

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

    /**
     * This method gets called when the tokens have been consumed, and it's time
     * to build the model via the content sink.
     * @update	gess5/11/98
     * @return  YES if model building went well -- NO otherwise.
     */
    virtual nsresult BuildModel(void);
    
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
    PRBool WillTokenize(PRBool aIsFinalChunk = PR_FALSE);

   
    /**
     *  This is the primary control routine. It iteratively
     *  consumes tokens until an error occurs or you run out
     *  of data.
     *  
     *  @update  gess 3/25/98
     *  @return  error code 
     */
    nsresult Tokenize(PRBool aIsFinalChunk = PR_FALSE);

    /**
     *  This is the tail-end of the code sandwich for the
     *  tokenization process. It gets called once tokenziation
     *  has completed.
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return  TRUE if all went well
     */
    PRBool DidTokenize(PRBool aIsFinalChunk = PR_FALSE);

  
protected:
    //*********************************************
    // And now, some data members...
    //*********************************************
    
  
    CParserContext*     mParserContext;
    PRInt32             mMajorIteration;
    PRInt32             mMinorIteration;

    nsIRequestObserver*   mObserver;
    nsIProgressEventSink* mProgressEventSink;
    nsIContentSink*     mSink;
   
    nsIParserFilter*    mParserFilter;
    PRBool              mDTDVerification;
    eParserCommands     mCommand;
    PRInt32             mStreamStatus;
    nsITokenObserver*   mTokenObserver;
    nsString            mUnusedInput;
    nsString            mCharset;
    nsCharsetSource     mCharsetSource;
    nsresult            mInternalState;
    PRBool              mObserversEnabled;
    nsString            mCommandStr;
    PRBool              mParserEnabled;
    nsTokenAllocator    mTokenAllocator;

    nsCOMPtr<nsIEventQueue> mEventQueue;
    PRPackedBool        mPendingContinueEvent;
    PRPackedBool        mCanInterrupt;
   
public:  
   
    MOZ_TIMER_DECLARE(mParseTime)
    MOZ_TIMER_DECLARE(mDTDTime)
    MOZ_TIMER_DECLARE(mTokenizeTime)
};

#endif 

