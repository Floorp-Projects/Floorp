/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
  
#define XMLENCODING_PEEKBYTES 64
#define DISABLE_TRANSITIONAL_MODE



#include "nsIAtom.h"
#include "nsParser.h"
#include "nsString.h"
#include "nsCRT.h" 
#include "nsScanner.h"
#include "plstr.h"
#include "nsIStringStream.h"
#include "nsIChannel.h"
#include "nsICachingChannel.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICharsetAlias.h"
#include "nsIInputStream.h"
#include "CNavDTD.h"
#include "COtherDTD.h"
#include "prenv.h" 
#include "nsParserCIID.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsExpatDriver.h"
#include "nsIServiceManager.h"
//#define rickgdebug 

#ifdef MOZ_VIEW_SOURCE
#include "nsViewSourceHTML.h" 
#endif

#define NS_PARSER_FLAG_DTD_VERIFICATION       0x00000001
#define NS_PARSER_FLAG_PARSER_ENABLED         0x00000002
#define NS_PARSER_FLAG_OBSERVERS_ENABLED      0x00000004
#define NS_PARSER_FLAG_PENDING_CONTINUE_EVENT 0x00000008
#define NS_PARSER_FLAG_CAN_INTERRUPT          0x00000010
#define NS_PARSER_FLAG_FLUSH_TOKENS           0x00000020
#define NS_PARSER_FLAG_CAN_TOKENIZE           0x00000040

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID); 
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

//-------------------------------------------------------------------
 

class CDTDDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    nsIDTD* aDTD =(nsIDTD*)anObject;
    NS_RELEASE(aDTD);
    return 0;
  }
};

//-------------------------------------------------------------------

class CDTDFinder: public nsDequeFunctor{
public:
  CDTDFinder(nsIDTD* aDTD) {
    mTargetDTD=aDTD;
  }
  virtual ~CDTDFinder() {
  }
  virtual void* operator()(void* anObject) {
    nsIDTD* theDTD=(nsIDTD*)anObject;
    if(theDTD->GetMostDerivedIID().Equals(mTargetDTD->GetMostDerivedIID()))
      return anObject;
    return 0;
  }
  nsIDTD* mTargetDTD;
};

//-------------------------------------------------------------------

class CSharedParserObjects {
public:

  CSharedParserObjects()
  :mDTDDeque(0), 
   mHasViewSourceDTD(PR_FALSE),
   mHasXMLDTD(PR_FALSE) 
  {
    // do nothing.
  }

  ~CSharedParserObjects() {
    CDTDDeallocator theDeallocator;
    mDTDDeque.ForEach(theDeallocator);  //release all the DTD's
  }

  nsresult Init() {
    //Note: To cut down on startup time/overhead, we defer the construction of non-html DTD's. 
    nsIDTD* theDTD = 0;
    nsresult rv = NS_NewNavHTMLDTD(&theDTD);    //do this as a default HTML DTD...
    
    NS_ASSERTION(theDTD, "Failed to create DTD");
    NS_ENSURE_SUCCESS(rv, rv);
    
    mDTDDeque.Push(theDTD);
    mHasViewSourceDTD = PR_FALSE;
    mHasXMLDTD = PR_FALSE;
    return NS_OK;
  }

  nsresult RegisterDTD(nsIDTD* aDTD) {
    NS_ENSURE_ARG_POINTER(aDTD);
    nsCOMPtr<nsIDTD> dtd(aDTD);
    CDTDFinder theFinder(dtd);
    if (!mDTDDeque.FirstThat(theFinder)) {
      nsIDTD* theDTD;
      nsresult rv = dtd->CreateNewInstance(&theDTD);
      NS_ENSURE_SUCCESS(rv, rv);
      mDTDDeque.Push(theDTD);
    }
    return NS_OK;
  }
  
  nsDeque mDTDDeque;
  PRBool  mHasViewSourceDTD;  //this allows us to defer construction of this object.
  PRBool  mHasXMLDTD;         //also defer XML dtd construction
};


//-------------- Begin ParseContinue Event Definition ------------------------
/*
The parser can be explicitly interrupted by passing a return value of NS_ERROR_HTMLPARSER_INTERRUPTED
from BuildModel on the DTD. This will cause the parser to stop processing and allow 
the application to return to the event loop. The data which was left at the time of 
interruption will be processed the next time OnDataAvailable is called. If the parser
has received its final chunk of data then OnDataAvailable will no longer be called by the 
networking module, so the parser will schedule a nsParserContinueEvent which will call 
the parser to process the  remaining data after returning to the event loop. If the parser 
is interrupted while processing the remaining data it will schedule another 
ParseContinueEvent. The processing of data followed by scheduling of the continue events 
will proceed until either:

  1) All of the remaining data can be processed without interrupting
  2) The parser has been cancelled.


This capability is currently used in CNavDTD and nsHTMLContentSink. The nsHTMLContentSink is
notified by CNavDTD when a chunk of tokens is going to be processed and when each token 
is processed. The nsHTML content sink records the time when the chunk has started
processing and will return NS_ERROR_HTMLPARSER_INTERRUPTED if the token processing time 
has exceeded a threshold called max tokenizing processing time. This allows the content 
sink to limit how much data is processed in a single chunk which in turn gates how much 
time is spent away from the event loop. Processing smaller chunks of data also reduces 
the time spent in subsequent reflows.

This capability is most apparent when loading large documents. If the maximum token 
processing time is set small enough the application will remain responsive during 
document load. 

A side-effect of this capability is that document load is not complete when the last chunk
of data is passed to OnDataAvailable since  the parser may have been interrupted when 
the last chunk of data arrived. The document is complete when all of the document has 
been tokenized and there aren't any pending nsParserContinueEvents. This can cause 
problems if the application assumes that it can monitor the load requests to determine
when the document load has been completed. This is what happens in Mozilla. The document
is considered completely loaded when all of the load requests have been satisfied. To delay the
document load until all of the parsing has been completed the nsHTMLContentSink adds a 
dummy parser load request which is not removed until the nsHTMLContentSink's DidBuildModel
is called. The CNavDTD will not call DidBuildModel until the final chunk of data has been 
passed to the parser through the OnDataAvailable and there aren't any pending 
nsParserContineEvents.

Currently the parser is ignores requests to be interrupted during the processing of script. 
This is because a document.write followed by JavaScript calls to manipulate the DOM may 
fail if the parser was interrupted during the document.write. 


For more details @see bugzilla bug 76722
*/


struct nsParserContinueEvent : public PLEvent {

  nsParserContinueEvent(nsParser* aParser)
  {
    NS_ADDREF(aParser); 
    PL_InitEvent(this, aParser, HandleEvent, DestroyEvent);  
  }

  ~nsParserContinueEvent()
  {
    nsParser *parser = (nsParser*) owner;
    NS_RELEASE(parser);
  }

  PR_STATIC_CALLBACK(void*) HandleEvent(PLEvent* aEvent)
  {
    nsParser *parser = (nsParser*) aEvent->owner;
    parser->HandleParserContinueEvent();
    return nsnull;
  }

  PR_STATIC_CALLBACK(void) DestroyEvent(PLEvent* aEvent)
  {
    delete (nsParserContinueEvent*) aEvent;
  }
};

//-------------- End ParseContinue Event Definition ------------------------


static CSharedParserObjects* gSharedParserObjects=0;


//-------------------------------------------------------------------------

nsresult
GetSharedObjects(CSharedParserObjects** aSharedParserObjects) {
  if (!gSharedParserObjects) {
    gSharedParserObjects = new CSharedParserObjects();
    NS_ENSURE_TRUE(gSharedParserObjects, NS_ERROR_OUT_OF_MEMORY);
    nsresult rv = gSharedParserObjects->Init();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  *aSharedParserObjects = gSharedParserObjects;
  return NS_OK;
}

/** 
 *  This gets called when the htmlparser module is shutdown.
 *   
 *  @update  gess 01/04/99
 */
void nsParser::FreeSharedObjects(void) {
  if (gSharedParserObjects) {
    delete gSharedParserObjects;
    gSharedParserObjects=0;
  }
}


#ifdef DEBUG
static PRBool gDumpContent=PR_FALSE;
#endif

/** 
 *  default constructor
 *   
 *  @update  gess 01/04/99
 *  @param   
 *  @return   
 */
nsParser::nsParser() {
#ifdef NS_DEBUG
  if(!gDumpContent) {
    gDumpContent=(PR_GetEnv("PARSER_DUMP_CONTENT"))? PR_TRUE:PR_FALSE;
  }
#endif

  mCharset.AssignLiteral("ISO-8859-1");
  mParserContext=0;
  mStreamStatus=0;
  mCharsetSource=kCharsetUninitialized;
  mInternalState=NS_OK;;
  mCommand=eViewNormal;
  mFlags = NS_PARSER_FLAG_OBSERVERS_ENABLED | NS_PARSER_FLAG_PARSER_ENABLED | NS_PARSER_FLAG_CAN_TOKENIZE;
 
  MOZ_TIMER_DEBUGLOG(("Reset: Parse Time: nsParser::nsParser(), this=%p\n", this));
  MOZ_TIMER_RESET(mParseTime);  
  MOZ_TIMER_RESET(mDTDTime);  
  MOZ_TIMER_RESET(mTokenizeTime);

  nsresult rv = NS_OK;
  if (mEventQueue == nsnull) {
    // Cache the event queue of the current UI thread
    nsCOMPtr<nsIEventQueueService> eventService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_SUCCEEDED(rv) && (eventService)) {                  // XXX this implies that the UI is the current thread.
      rv = eventService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
    }

   // NS_ASSERTION(mEventQueue, "event queue is null");
  }
}

/**
 *  Default destructor
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @return  
 */
nsParser::~nsParser() {

#ifdef NS_DEBUG
  if(gDumpContent) {
    if(mSink) {
      // Sink ( HTMLContentSink at this time) supports nsIDebugDumpContent
      // interface. We can get to the content model through the sink.
      nsresult result=NS_OK;
      nsCOMPtr<nsIDebugDumpContent> trigger=do_QueryInterface(mSink,&result);
      if(NS_SUCCEEDED(result)) {
        trigger->DumpContentModel();
      }
    }
  }
#endif

  //don't forget to add code here to delete 
  //what may be several contexts...
  delete mParserContext;

  if (mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT) {
    NS_ASSERTION(mEventQueue != nsnull,"Event queue is null"); 
    mEventQueue->RevokeEvents(this);
  }
}


NS_IMPL_ADDREF(nsParser)
NS_IMPL_RELEASE(nsParser)


/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 01/04/99
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult nsParser::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                       
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIParser*)(this);                                        
  }
  else if(aIID.Equals(kIParserIID)) {  //do IParser base class...
    *aInstancePtr = (nsIParser*)(this);                                        
  }
  else if(aIID.Equals(NS_GET_IID(nsIRequestObserver))) {
    *aInstancePtr = (nsIRequestObserver*)(this);                                        
  }
  else if(aIID.Equals(NS_GET_IID(nsIStreamListener))) {
    *aInstancePtr = (nsIStreamListener*)(this);                                        
  }
  else if(aIID.Equals(kCParserCID)) {  //do this class...
    *aInstancePtr = (nsParser*)(this);                                        
  }   
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

// The parser continue event is posted only if
// all of the data to parse has been passed to ::OnDataAvailable
// and the parser has been interrupted by the content sink
// because the processing of tokens took too long.
 
nsresult
nsParser::PostContinueEvent()
{
  if (!(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT) && mEventQueue) {
    nsParserContinueEvent* ev = new nsParserContinueEvent(this);
    NS_ENSURE_TRUE(ev, NS_ERROR_OUT_OF_MEMORY);
    if (NS_FAILED(mEventQueue->PostEvent(ev))) {
        NS_ERROR("failed to post parser continuation event");
        PL_DestroyEvent(ev);
    }
    else
        mFlags |= NS_PARSER_FLAG_PENDING_CONTINUE_EVENT;
  }
  return NS_OK;
}


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
NS_IMETHODIMP_(void) nsParser::SetParserFilter(nsIParserFilter * aFilter)
{
  mParserFilter = aFilter;
}


NS_IMETHODIMP_(void) nsParser::GetCommand(nsString& aCommand)
{
  aCommand = mCommandStr;  
}

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about the command which caused the parser to be constructed. For example,
 *  this allows us to select a DTD which can do, say, view-source.
 *  
 *  @update  gess 01/04/99
 *  @param   aCommand the command string to set
 */
NS_IMETHODIMP_(void) nsParser::SetCommand(const char* aCommand)
{
  nsCAutoString theCommand(aCommand);
  if(theCommand.Equals(kViewSourceCommand))
    mCommand=eViewSource;
  else if(theCommand.Equals(kViewFragmentCommand))
    mCommand=eViewFragment;
  else mCommand=eViewNormal;
  mCommandStr.AssignWithConversion(aCommand);
}

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about the command which caused the parser to be constructed. For example,
 *  this allows us to select a DTD which can do, say, view-source.
 *  
 *  @update  gess 01/04/99
 *  @param   aParserCommand the command to set
 */
NS_IMETHODIMP_(void) nsParser::SetCommand(eParserCommands aParserCommand)
{
  mCommand = aParserCommand;
}


/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about what charset to load
 *  
 *  @update  ftang 4/23/99
 *  @param   aCharset- the charset of a document
 *  @param   aCharsetSource- the source of the charset
 *  @return	 nada
 */
NS_IMETHODIMP_(void)
nsParser::SetDocumentCharset(const nsACString& aCharset, PRInt32 aCharsetSource)
{
  mCharset = aCharset;
  mCharsetSource = aCharsetSource; 
  if(mParserContext && mParserContext->mScanner)
     mParserContext->mScanner->SetDocumentCharset(aCharset, aCharsetSource);
}

void nsParser::SetSinkCharset(nsACString& aCharset)
{
  if (mSink) {
    mSink->SetDocumentCharset(aCharset);
  }
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *  
 *  @update  gess 01/04/99
 *  @param   nsIContentSink interface for node receiver
 *  @return  
 */
NS_IMETHODIMP_(void) nsParser::SetContentSink(nsIContentSink* aSink)
{
  NS_PRECONDITION(aSink,"sink cannot be null!");
  mSink = aSink;
  
  if (mSink) {
    mSink->SetParser(this);
  }
}

/**
 * retrive the sink set into the parser 
 * @update	gess5/11/98
 * @return  current sink 
 */
NS_IMETHODIMP_(nsIContentSink*) nsParser::GetContentSink(void)
{
  return mSink;
}

/**
 *  Call this method when you want to
 *  register your dynamic DTD's with the parser.
 *  
 *  @update  gess 01/04/99
 *  @param   aDTD  is the object to be registered.
 *  @return  nothing.
 */
NS_IMETHODIMP
nsParser::RegisterDTD(nsIDTD* aDTD)
{
  CSharedParserObjects* sharedObjects;
  nsresult rv = GetSharedObjects(&sharedObjects);
  NS_ENSURE_SUCCESS(rv, rv);
  return sharedObjects->RegisterDTD(aDTD);
}

/**
 *  Retrieve parsemode from topmost parser context
 *  
 *  @update  gess 01/04/99
 *  @return  parsemode
 */
NS_IMETHODIMP_(nsDTDMode) nsParser::GetParseMode(void)
{
  if(mParserContext)
    return mParserContext->mDTDMode;
  NS_NOTREACHED("no parser context");
  return eDTDMode_unknown;
}


template <class CharT>
class CWordTokenizer {
public:
  CWordTokenizer(const CharT* aBuffer,PRInt32 aStartOffset,PRInt32 aMaxOffset) {
    mLength=0;
    mOffset=aStartOffset;
    mMaxOffset=aMaxOffset;
    mBuffer=aBuffer;
    mEndBuffer=mBuffer+mMaxOffset;
  }

  //********************************************************************************
  // Get offset of nth word in string.
  // We define words as: 
  //    1) sequence of alphanum; 
  //    2) quoted substring
  //    3) SGML comment -- ... -- 
  // Returns offset of nth word, or -1 (if out of words).
  //********************************************************************************
  
  PRInt32 GetNextWord(PRBool aSkipQuotes=PR_FALSE) {
    
    if(mOffset == kNotFound) {
      return kNotFound; // Ref. bug 89732
    }

    if (mOffset >= 0) {
      const CharT *cp=mBuffer+mOffset+mLength;  //skip last word

      mLength=0;  //reset this
      mOffset=-1; //reset this        

      //now skip whitespace...

      CharT target=0;
      PRBool    done=PR_FALSE;

      while((!done) && (cp++<mEndBuffer)) {
        switch(*cp) {
          case kSpace:  case kNewLine:
          case kCR:     case kTab:
          case kEqual:
            continue;

          case kQuote:
            target=*cp;
            if (aSkipQuotes) {
              ++cp;
            }
            done=PR_TRUE;
            break;

          case kMinus:
            target=*cp;
            done=PR_TRUE;
            break;

          default:
            done=PR_TRUE;
            break;
        }
      }

      if(cp<mEndBuffer) {  

        const CharT *firstcp=cp; //hang onto this...      
        PRInt32 theDashCount=2;

        ++cp; //just skip first letter to simplify processing...

        //ok, now find end of this word
        while(cp++<mEndBuffer) {
          if(kQuote==target) {
            if(kQuote==*cp) {
              ++cp;
              break; //we found our end...
            }
          }
          else if(kMinus==target) {
            //then let's look for SGML comments
            if(kMinus==*cp) {
              if(4==++theDashCount) {
                ++cp;
                break;
              }
            }
          }
          else {
            if((kSpace==*cp) ||
               (kNewLine==*cp) ||
               (kGreaterThan==*cp) ||
               (kQuote==*cp) ||
               (kCR==*cp) ||
               (kTab==*cp) || 
               (kEqual == *cp)) {
              break;
            }
          }
        }

        mLength=cp-firstcp;
        mOffset = (0<mLength) ? firstcp-mBuffer : -1;

      }
    }

    return mOffset;
  }

  PRInt32 GetLength() const {
    return mLength;
  }

  PRInt32     mOffset;
  PRInt32     mMaxOffset;
  PRInt32     mLength;
  const CharT*  mBuffer;
  const CharT*  mEndBuffer;
};

/**
 * Determine what DTD mode (and thus what layout nsCompatibility mode)
 * to use for this document based on the first chunk of data recieved
 * from the network (each parsercontext can have its own mode).  (No,
 * this is not an optimal solution -- we really don't need to know until
 * after we've received the DOCTYPE, and this could easily be part of
 * the regular parsing process if the parser were designed in a way that
 * made such modifications easy.)
 */

// Parse the PS production in the SGML spec (excluding the part dealing
// with entity references) starting at theIndex into theBuffer, and
// return the first index after the end of the production.
static PRInt32 ParsePS(const nsString& aBuffer, PRInt32 aIndex)
{
  for(;;) {
    PRUnichar ch = aBuffer.CharAt(aIndex);
    if ((ch == PRUnichar(' ')) || (ch == PRUnichar('\t')) ||
        (ch == PRUnichar('\n')) || (ch == PRUnichar('\r'))) {
      ++aIndex;
    } else if (ch == PRUnichar('-')) {
      PRInt32 tmpIndex;
      if (aBuffer.CharAt(aIndex+1) == PRUnichar('-') &&
          kNotFound != (tmpIndex=aBuffer.Find("--",PR_FALSE,aIndex+2,-1))) {
        aIndex = tmpIndex + 2;
      } else {
        return aIndex;
      }
    } else {
      return aIndex;
    }
  }
}

#define PARSE_DTD_HAVE_DOCTYPE          (1<<0)
#define PARSE_DTD_HAVE_PUBLIC_ID        (1<<1)
#define PARSE_DTD_HAVE_SYSTEM_ID        (1<<2)
#define PARSE_DTD_HAVE_INTERNAL_SUBSET  (1<<3)

// return PR_TRUE on success (includes not present), PR_FALSE on failure
static PRBool ParseDocTypeDecl(const nsString &aBuffer,
                               PRInt32 *aResultFlags,
                               nsString &aPublicID,
                               nsString &aSystemID)
{
  PRBool haveDoctype = PR_FALSE;
  *aResultFlags = 0;

  // Skip through any comments and processing instructions
  // The PI-skipping is a bit of a hack.
  PRInt32 theIndex = 0;
  do {
    theIndex = aBuffer.FindChar('<', theIndex);
    if (theIndex == kNotFound) break;
    PRUnichar nextChar = aBuffer.CharAt(theIndex+1);
    if (nextChar == PRUnichar('!')) {
      PRInt32 tmpIndex = theIndex;
      if (kNotFound !=
          (theIndex=aBuffer.Find("DOCTYPE", PR_TRUE, theIndex+2, 1))) {
        haveDoctype = PR_TRUE;
        theIndex += 7; // skip "DOCTYPE"
        break;
      }
      theIndex = ParsePS(aBuffer,tmpIndex);
      // -1, not 0, in case it's another markup declaration
      theIndex = aBuffer.FindChar('>', theIndex);
    } else if (nextChar == PRUnichar('?')) {
      theIndex = aBuffer.FindChar('>', theIndex);
    } else {
      break;
    }
  } while (theIndex != kNotFound);

  if (!haveDoctype)
    return PR_TRUE;
  *aResultFlags |= PARSE_DTD_HAVE_DOCTYPE;

  theIndex = ParsePS(aBuffer, theIndex);
  theIndex = aBuffer.Find("HTML", PR_TRUE, theIndex, 1);
  if(kNotFound == theIndex)
    return PR_FALSE;
  theIndex = ParsePS(aBuffer, theIndex+4);
  PRInt32 tmpIndex = aBuffer.Find("PUBLIC", PR_TRUE, theIndex, 1);

  if (kNotFound != tmpIndex) {
    theIndex = ParsePS(aBuffer, tmpIndex+6);

    // We get here only if we've read <!DOCTYPE HTML PUBLIC
    // (not case sensitive) possibly with comments within.

    // Now find the beginning and end of the public identifier
    // and the system identifier (if present).

    PRUnichar lit = aBuffer.CharAt(theIndex);
    if ((lit != PRUnichar('\"')) && (lit != PRUnichar('\'')))
      return PR_FALSE;

    // Start is the first character, excluding the quote, and End is
    // the final quote, so there are (end-start) characters.

    PRInt32 PublicIDStart = theIndex + 1;
    PRInt32 PublicIDEnd = aBuffer.FindChar(lit, PublicIDStart);
    if (kNotFound == PublicIDEnd)
      return PR_FALSE;
    theIndex = ParsePS(aBuffer, PublicIDEnd + 1);
    PRUnichar next = aBuffer.CharAt(theIndex);
    if (next == PRUnichar('>')) {
      // There was a public identifier, but no system
      // identifier,
      // so do nothing.
      // This is needed to avoid the else at the end, and it's
      // also the most common case.
    } else if ((next == PRUnichar('\"')) ||
               (next == PRUnichar('\''))) {
      // We found a system identifier.
      *aResultFlags |= PARSE_DTD_HAVE_SYSTEM_ID;
      PRInt32 SystemIDStart = theIndex + 1;
      PRInt32 SystemIDEnd = aBuffer.FindChar(next, SystemIDStart);
      if (kNotFound == SystemIDEnd)
        return PR_FALSE;
      aSystemID =
        Substring(aBuffer, SystemIDStart, SystemIDEnd - SystemIDStart);
    } else if (next == PRUnichar('[')) {
      // We found an internal subset.
      *aResultFlags |= PARSE_DTD_HAVE_INTERNAL_SUBSET;
    } else {
      // Something's wrong.
      return PR_FALSE;
    }

    // Since a public ID is a minimum literal, we must trim
    // and collapse whitespace
    aPublicID = Substring(aBuffer, PublicIDStart, PublicIDEnd - PublicIDStart);
    aPublicID.CompressWhitespace(PR_TRUE, PR_TRUE);
    *aResultFlags |= PARSE_DTD_HAVE_PUBLIC_ID;
  } else {
    tmpIndex=aBuffer.Find("SYSTEM", PR_TRUE, theIndex, 1);
    if (kNotFound != tmpIndex) {
      // DOCTYPES with system ID but no Public ID
      *aResultFlags |= PARSE_DTD_HAVE_SYSTEM_ID;
      
      theIndex = ParsePS(aBuffer, tmpIndex+6);
      PRUnichar next = aBuffer.CharAt(theIndex);
      if (next != PRUnichar('\"') && next != PRUnichar('\''))
        return PR_FALSE;

      PRInt32 SystemIDStart = theIndex + 1;
      PRInt32 SystemIDEnd = aBuffer.FindChar(next, SystemIDStart);

      if (kNotFound == SystemIDEnd)
        return PR_FALSE;
      aSystemID =
        Substring(aBuffer, SystemIDStart, SystemIDEnd - SystemIDStart);
      theIndex = ParsePS(aBuffer, SystemIDEnd + 1);
    }

    PRUnichar nextChar = aBuffer.CharAt(theIndex);
    if (nextChar == PRUnichar('['))
      *aResultFlags |= PARSE_DTD_HAVE_INTERNAL_SUBSET;
    else if (nextChar != PRUnichar('>'))
      return PR_FALSE;
  }
  return PR_TRUE;
}

struct PubIDInfo {
  enum eMode {
    eQuirks,         /* always quirks mode, unless there's an internal subset */
    eQuirks3,        /* ditto, but but pre-HTML4 (no tbody) */
    eAlmostStandards,/* eCompatibility_AlmostStandards */
    eFullStandards   /* eCompatibility_FullStandards */
      /*
       * public IDs that should trigger strict mode are not listed
       * since we want all future public IDs to trigger strict mode as
       * well
       */
  };

  const char* name;
  eMode mode_if_no_sysid;
  eMode mode_if_sysid;
};

#define ELEMENTS_OF(array_) (sizeof(array_)/sizeof(array_[0]))

// These must be in nsCRT::strcmp order so binary-search can be used.
// This is verified, |#ifdef DEBUG|, below.

// Even though public identifiers should be case sensitive, we will do
// all comparisons after converting to lower case in order to do
// case-insensitive comparison since there are a number of existing web
// sites that use the incorrect case.  Therefore all of the public
// identifiers below are in lower case (with the correct case following,
// in comments).  The case is verified, |#ifdef DEBUG|, below.
static const PubIDInfo kPublicIDs[] = {
  {"+//silmaril//dtd html pro v0r11 19970101//en" /* "+//Silmaril//dtd html Pro v0r11 19970101//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//advasoft ltd//dtd html 3.0 aswedit + extensions//en" /* "-//AdvaSoft Ltd//DTD HTML 3.0 asWedit + extensions//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//as//dtd html 3.0 aswedit + extensions//en" /* "-//AS//DTD HTML 3.0 asWedit + extensions//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 2.0 level 1//en" /* "-//IETF//DTD HTML 2.0 Level 1//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 2.0 level 2//en" /* "-//IETF//DTD HTML 2.0 Level 2//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 2.0 strict level 1//en" /* "-//IETF//DTD HTML 2.0 Strict Level 1//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 2.0 strict level 2//en" /* "-//IETF//DTD HTML 2.0 Strict Level 2//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 2.0 strict//en" /* "-//IETF//DTD HTML 2.0 Strict//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 2.0//en" /* "-//IETF//DTD HTML 2.0//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 2.1e//en" /* "-//IETF//DTD HTML 2.1E//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 3.0//en" /* "-//IETF//DTD HTML 3.0//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 3.0//en//" /* "-//IETF//DTD HTML 3.0//EN//" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 3.2 final//en" /* "-//IETF//DTD HTML 3.2 Final//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 3.2//en" /* "-//IETF//DTD HTML 3.2//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html 3//en" /* "-//IETF//DTD HTML 3//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html level 0//en" /* "-//IETF//DTD HTML Level 0//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html level 0//en//2.0" /* "-//IETF//DTD HTML Level 0//EN//2.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html level 1//en" /* "-//IETF//DTD HTML Level 1//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html level 1//en//2.0" /* "-//IETF//DTD HTML Level 1//EN//2.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html level 2//en" /* "-//IETF//DTD HTML Level 2//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html level 2//en//2.0" /* "-//IETF//DTD HTML Level 2//EN//2.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html level 3//en" /* "-//IETF//DTD HTML Level 3//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html level 3//en//3.0" /* "-//IETF//DTD HTML Level 3//EN//3.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict level 0//en" /* "-//IETF//DTD HTML Strict Level 0//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict level 0//en//2.0" /* "-//IETF//DTD HTML Strict Level 0//EN//2.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict level 1//en" /* "-//IETF//DTD HTML Strict Level 1//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict level 1//en//2.0" /* "-//IETF//DTD HTML Strict Level 1//EN//2.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict level 2//en" /* "-//IETF//DTD HTML Strict Level 2//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict level 2//en//2.0" /* "-//IETF//DTD HTML Strict Level 2//EN//2.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict level 3//en" /* "-//IETF//DTD HTML Strict Level 3//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict level 3//en//3.0" /* "-//IETF//DTD HTML Strict Level 3//EN//3.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict//en" /* "-//IETF//DTD HTML Strict//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict//en//2.0" /* "-//IETF//DTD HTML Strict//EN//2.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html strict//en//3.0" /* "-//IETF//DTD HTML Strict//EN//3.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html//en" /* "-//IETF//DTD HTML//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html//en//2.0" /* "-//IETF//DTD HTML//EN//2.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//ietf//dtd html//en//3.0" /* "-//IETF//DTD HTML//EN//3.0" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//metrius//dtd metrius presentational//en" /* "-//Metrius//DTD Metrius Presentational//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 2.0 html strict//en" /* "-//Microsoft//DTD Internet Explorer 2.0 HTML Strict//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//microsoft//dtd internet explorer 2.0 html//en" /* "-//Microsoft//DTD Internet Explorer 2.0 HTML//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//microsoft//dtd internet explorer 2.0 tables//en" /* "-//Microsoft//DTD Internet Explorer 2.0 Tables//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//microsoft//dtd internet explorer 3.0 html strict//en" /* "-//Microsoft//DTD Internet Explorer 3.0 HTML Strict//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//microsoft//dtd internet explorer 3.0 html//en" /* "-//Microsoft//DTD Internet Explorer 3.0 HTML//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//microsoft//dtd internet explorer 3.0 tables//en" /* "-//Microsoft//DTD Internet Explorer 3.0 Tables//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//netscape comm. corp.//dtd html//en" /* "-//Netscape Comm. Corp.//DTD HTML//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//netscape comm. corp.//dtd strict html//en" /* "-//Netscape Comm. Corp.//DTD Strict HTML//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//o'reilly and associates//dtd html 2.0//en" /* "-//O'Reilly and Associates//DTD HTML 2.0//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//o'reilly and associates//dtd html extended 1.0//en" /* "-//O'Reilly and Associates//DTD HTML Extended 1.0//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//o'reilly and associates//dtd html extended relaxed 1.0//en" /* "-//O'Reilly and Associates//DTD HTML Extended Relaxed 1.0//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//en" /* "-//SoftQuad Software//DTD HoTMetaL PRO 6.0::19990601::extensions to HTML 4.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//en" /* "-//SoftQuad//DTD HoTMetaL PRO 4.0::19971010::extensions to HTML 4.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//spyglass//dtd html 2.0 extended//en" /* "-//Spyglass//DTD HTML 2.0 Extended//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//sq//dtd html 2.0 hotmetal + extensions//en" /* "-//SQ//DTD HTML 2.0 HoTMetaL + extensions//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//sun microsystems corp.//dtd hotjava html//en" /* "-//Sun Microsystems Corp.//DTD HotJava HTML//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//sun microsystems corp.//dtd hotjava strict html//en" /* "-//Sun Microsystems Corp.//DTD HotJava Strict HTML//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3c//dtd html 3 1995-03-24//en" /* "-//W3C//DTD HTML 3 1995-03-24//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3c//dtd html 3.2 draft//en" /* "-//W3C//DTD HTML 3.2 Draft//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3c//dtd html 3.2 final//en" /* "-//W3C//DTD HTML 3.2 Final//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3c//dtd html 3.2//en" /* "-//W3C//DTD HTML 3.2//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3c//dtd html 3.2s draft//en" /* "-//W3C//DTD HTML 3.2S Draft//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3c//dtd html 4.0 frameset//en" /* "-//W3C//DTD HTML 4.0 Frameset//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 4.0 transitional//en" /* "-//W3C//DTD HTML 4.0 Transitional//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 4.01 frameset//en" /* "-//W3C//DTD HTML 4.01 Frameset//EN" */, PubIDInfo::eQuirks, PubIDInfo::eAlmostStandards},
  {"-//w3c//dtd html 4.01 transitional//en" /* "-//W3C//DTD HTML 4.01 Transitional//EN" */, PubIDInfo::eQuirks, PubIDInfo::eAlmostStandards},
  {"-//w3c//dtd html experimental 19960712//en" /* "-//W3C//DTD HTML Experimental 19960712//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3c//dtd html experimental 970421//en" /* "-//W3C//DTD HTML Experimental 970421//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3c//dtd w3 html//en" /* "-//W3C//DTD W3 HTML//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3c//dtd xhtml 1.0 frameset//en" /* "-//W3C//DTD XHTML 1.0 Frameset//EN" */, PubIDInfo::eAlmostStandards, PubIDInfo::eAlmostStandards},
  {"-//w3c//dtd xhtml 1.0 transitional//en" /* "-//W3C//DTD XHTML 1.0 Transitional//EN" */, PubIDInfo::eAlmostStandards, PubIDInfo::eAlmostStandards},
  {"-//w3o//dtd w3 html 3.0//en" /* "-//W3O//DTD W3 HTML 3.0//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3o//dtd w3 html 3.0//en//" /* "-//W3O//DTD W3 HTML 3.0//EN//" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//w3o//dtd w3 html strict 3.0//en//" /* "-//W3O//DTD W3 HTML Strict 3.0//EN//" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//webtechs//dtd mozilla html 2.0//en" /* "-//WebTechs//DTD Mozilla HTML 2.0//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-//webtechs//dtd mozilla html//en" /* "-//WebTechs//DTD Mozilla HTML//EN" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
  {"-/w3c/dtd html 4.0 transitional/en" /* "-/W3C/DTD HTML 4.0 Transitional/EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"html" /* "HTML" */, PubIDInfo::eQuirks3, PubIDInfo::eQuirks3},
};

#ifdef DEBUG
static void VerifyPublicIDs()
{
  static PRBool gVerified = PR_FALSE;
  if (!gVerified) {
    gVerified = PR_TRUE;
    PRUint32 i;
    for (i = 0; i < ELEMENTS_OF(kPublicIDs) - 1; ++i) {
      if (nsCRT::strcmp(kPublicIDs[i].name, kPublicIDs[i+1].name) >= 0) {
        NS_NOTREACHED("doctypes out of order");
        printf("Doctypes %s and %s out of order.\n",
               kPublicIDs[i].name, kPublicIDs[i+1].name);
      }
    }
    for (i = 0; i < ELEMENTS_OF(kPublicIDs); ++i) {
      nsCAutoString lcPubID(kPublicIDs[i].name);
      ToLowerCase(lcPubID);
      if (nsCRT::strcmp(kPublicIDs[i].name, lcPubID.get()) != 0) {
        NS_NOTREACHED("doctype not lower case");
        printf("Doctype %s not lower case.\n", kPublicIDs[i].name);
      }
    } 
  }
}
#endif

static void DetermineHTMLParseMode(const nsString& aBuffer,
                                   nsDTDMode& aParseMode,
                                   eParserDocType& aDocType)
{
#ifdef DEBUG
  VerifyPublicIDs();
#endif
  PRInt32 resultFlags;
  nsAutoString publicIDUCS2, sysIDUCS2;
  if (ParseDocTypeDecl(aBuffer, &resultFlags, publicIDUCS2, sysIDUCS2)) {
    if (!(resultFlags & PARSE_DTD_HAVE_DOCTYPE)) {

      // no DOCTYPE
      aParseMode = eDTDMode_quirks;
      aDocType = eHTML_Quirks;

    } else if ((resultFlags & PARSE_DTD_HAVE_INTERNAL_SUBSET) ||
               !(resultFlags & PARSE_DTD_HAVE_PUBLIC_ID)) {

      // A doctype with an internal subset is always full_standards.
      // A doctype without a public ID is always full_standards.
      aDocType = eHTML_Strict;
      aParseMode = eDTDMode_full_standards;

      // Special hack for IBM's custom DOCTYPE.
      if (!(resultFlags & PARSE_DTD_HAVE_INTERNAL_SUBSET) &&
          sysIDUCS2 == NS_LITERAL_STRING(
               "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd")) {
        aParseMode = eDTDMode_quirks;
        aDocType = eHTML_Quirks;
      }

    } else {

      // We have to check our list of public IDs to see what to do.

      // Yes, we want UCS2 to ASCII lossy conversion.
      nsCAutoString publicID;
      publicID.AssignWithConversion(publicIDUCS2);

      // See comment above definition of kPublicIDs about case
      // sensitivity.
      ToLowerCase(publicID);

      // binary search to see if we can find the correct public ID
        // These must be signed since maximum can go below zero and we'll
        // crash if it's unsigned.
      PRInt32 minimum = 0;
      PRInt32 maximum = ELEMENTS_OF(kPublicIDs) - 1;
      PRInt32 index;
      for (;;) {
        index = (minimum + maximum) / 2;
        PRInt32 comparison =
            nsCRT::strcmp(publicID.get(), kPublicIDs[index].name);
        if (comparison == 0)
          break;
        if (comparison < 0)
          maximum = index - 1;
        else
          minimum = index + 1;

        if (maximum < minimum) {
          // The DOCTYPE is not in our list, so it must be full_standards.
          aParseMode = eDTDMode_full_standards;
          aDocType = eHTML_Strict;
          return;
        }
      }

      switch ((resultFlags & PARSE_DTD_HAVE_SYSTEM_ID)
                ? kPublicIDs[index].mode_if_sysid
                : kPublicIDs[index].mode_if_no_sysid)
      {
        case PubIDInfo::eQuirks3:
          aParseMode = eDTDMode_quirks;
          aDocType = eHTML3_Quirks;
          break;
        case PubIDInfo::eQuirks:
          aParseMode = eDTDMode_quirks;
          aDocType = eHTML_Quirks;
          break;
        case PubIDInfo::eAlmostStandards:
          aParseMode = eDTDMode_almost_standards;
          aDocType = eHTML_Strict;
          break;
        case PubIDInfo::eFullStandards:
          aParseMode = eDTDMode_full_standards;
          aDocType = eHTML_Strict;
          break;
        default:
          NS_NOTREACHED("no other cases!");
      }

    }
  } else {
    // badly formed DOCTYPE -> quirks
    aParseMode = eDTDMode_quirks;
    aDocType = eHTML3_Quirks;
  }
}

static 
void DetermineParseMode(const nsString& aBuffer,
                        nsDTDMode& aParseMode,
                        eParserDocType& aDocType,
                        const nsACString& aMimeType)
{
  if (aMimeType.EqualsLiteral(kHTMLTextContentType)) {
    DetermineHTMLParseMode(aBuffer, aParseMode, aDocType);
  } else if (aMimeType.EqualsLiteral(kPlainTextContentType) ||
             aMimeType.EqualsLiteral(kTextCSSContentType) ||
             aMimeType.EqualsLiteral(kApplicationJSContentType) ||
             aMimeType.EqualsLiteral(kTextJSContentType)) {
    aDocType = ePlainText;
    aParseMode = eDTDMode_quirks;
  } else { // Some form of XML
    aDocType = eXML;
    aParseMode = eDTDMode_full_standards;
  }
}

/**
 *  
 *  
 *  @update  gess 5/13/98
 *  @param   
 *  @return  
 */
static
nsresult
FindSuitableDTD(CParserContext& aParserContext,
                PRBool* aReturn)
{
  *aReturn = PR_FALSE;
  //Let's start by trying the defaultDTD, if one exists...
  if(aParserContext.mDTD) {
    eAutoDetectResult canParse = aParserContext.mDTD->CanParse(aParserContext);
    if(canParse != eUnknownDetect && canParse != eInvalidDetect)
      return PR_TRUE;
  }

  CSharedParserObjects* sharedObjects;
  nsresult rv = GetSharedObjects(&sharedObjects);
  NS_ENSURE_SUCCESS(rv, rv);

  aParserContext.mAutoDetectStatus = eUnknownDetect;
  PRInt32 theDTDIndex = 0;
  nsIDTD* theBestDTD  = 0;
  nsIDTD* theDTD      = 0;
  PRBool  thePrimaryFound = PR_FALSE;

  while ((theDTDIndex <= sharedObjects->mDTDDeque.GetSize()) && 
         (aParserContext.mAutoDetectStatus != ePrimaryDetect)){
    theDTD = NS_STATIC_CAST(nsIDTD*, sharedObjects->mDTDDeque.ObjectAt(theDTDIndex++));
    if (theDTD) {
      // Store detect status in temp ( theResult ) to avoid bugs such as
      // 36233, 36754, 36491, 36323. Basically, we should avoid calling DTD's
      // WillBuildModel() multiple times, i.e., we shouldn't leave auto-detect-status
      // unknown.
      eAutoDetectResult theResult = theDTD->CanParse(aParserContext);
      if (eValidDetect == theResult){
        aParserContext.mAutoDetectStatus = eValidDetect;
        theBestDTD = theDTD;
      }
      else if (ePrimaryDetect == theResult) {  
        theBestDTD = theDTD;
        thePrimaryFound = PR_TRUE;
        aParserContext.mAutoDetectStatus = ePrimaryDetect;
      }
    }
    if (theDTDIndex == sharedObjects->mDTDDeque.GetSize() && !thePrimaryFound) {
      if (!sharedObjects->mHasXMLDTD) {
        rv = NS_NewExpatDriver(&theDTD); //do this to view XML files...
        NS_ENSURE_SUCCESS(rv, rv);

        sharedObjects->mDTDDeque.Push(theDTD);
        sharedObjects->mHasXMLDTD = PR_TRUE;
      }
#ifdef MOZ_VIEW_SOURCE
      else if (!sharedObjects->mHasViewSourceDTD) {
        rv = NS_NewViewSourceHTML(&theDTD);  //do this so all non-html files can be viewed...
        NS_ENSURE_SUCCESS(rv, rv);
        
        sharedObjects->mDTDDeque.Push(theDTD);
        sharedObjects->mHasViewSourceDTD = PR_TRUE;
      }
#endif
    }
  }

  if(theBestDTD) {
    rv = theBestDTD->CreateNewInstance(&aParserContext.mDTD);
    NS_ENSURE_SUCCESS(rv, rv);

    *aReturn = PR_TRUE;
  }

  return rv;
}

NS_IMETHODIMP 
nsParser::CancelParsingEvents()
{
  if (mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT) {
    NS_ASSERTION(mEventQueue,"Event queue is null");
    // Revoke all pending continue parsing events 
    if (mEventQueue != nsnull) {
      mEventQueue->RevokeEvents(this);
    } 

    mFlags &= ~NS_PARSER_FLAG_PENDING_CONTINUE_EVENT;
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////


/**
 * This gets called just prior to the model actually
 * being constructed. It's important to make this the
 * last thing that happens right before parsing, so we
 * can delay until the last moment the resolution of
 * which DTD to use (unless of course we're assigned one).
 */
nsresult 
nsParser::WillBuildModel(nsString& aFilename)
{
  if (!mParserContext)
    return kInvalidParserContext;

  if (eUnknownDetect != mParserContext->mAutoDetectStatus)
    return NS_OK;

  if (eDTDMode_unknown == mParserContext->mDTDMode ||
      eDTDMode_autodetect == mParserContext->mDTDMode) {
    
    nsAutoString theBuffer;
    mParserContext->mScanner->Peek(theBuffer, 1024);    
    DetermineParseMode(theBuffer, mParserContext->mDTDMode,
                       mParserContext->mDocType, mParserContext->mMimeType);
  }
  
  PRBool found;
  nsresult rv = FindSuitableDTD(*mParserContext, &found);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!found)
    return rv;

  nsITokenizer* tokenizer;
  mParserContext->GetTokenizer(mParserContext->mDTD->GetType(), tokenizer);
  return mParserContext->mDTD->WillBuildModel(*mParserContext, tokenizer, mSink);
}

/**
 * This gets called when the parser is done with its input.
 * Note that the parser may have been called recursively, so we
 * have to check for a prev. context before closing out the DTD/sink.
 * @update	gess5/18/98
 * @param 
 * @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::DidBuildModel(nsresult anErrorCode) {
  //One last thing...close any open containers.
  nsresult result=anErrorCode;

  if (IsComplete()) {
    if (mParserContext && !mParserContext->mPrevContext) {
      if (mParserContext->mDTD) {
        result = mParserContext->mDTD->DidBuildModel(anErrorCode,PR_TRUE,this,mSink);
      }
      //Ref. to bug 61462.
      mParserContext->mRequest = 0;
    }//if
  }

  return result;
}


/**
 * This method adds a new parser context to the list,
 * pushing the current one to the next position.
 * @update	gess7/22/98
 * @param   ptr to new context
 * @return  nada
 */
void nsParser::PushContext(CParserContext& aContext) {
  aContext.mPrevContext=mParserContext;  
  mParserContext=&aContext;
}

/**
 * This method pops the topmost context off the stack,
 * returning it to the user. The next context  (if any)
 * becomes the current context.
 * @update	gess7/22/98
 * @return  prev. context
 */
CParserContext* nsParser::PopContext() 
{
  CParserContext* oldContext = mParserContext;
  if (oldContext) {
    mParserContext = oldContext->mPrevContext;
    if (mParserContext) {
      // If the old context was blocked, propagate the blocked state
      // back to the new one. Also, propagate the stream listener state
      // but don't override onStop state to guarantee the call to DidBuildModel().
      if (mParserContext->mStreamListenerState != eOnStop) {
        mParserContext->mStreamListenerState = oldContext->mStreamListenerState;
      }
      // Preserve tokenizer state so that information is not lost
      // between document.write. This fixes bug 99467
      if (mParserContext->mTokenizer)
        mParserContext->mTokenizer->CopyState(oldContext->mTokenizer);
    }
  }
  return oldContext;
}

/**
 *  Call this when you want control whether or not the parser will parse
 *  and tokenize input (TRUE), or whether it just caches input to be 
 *  parsed later (FALSE).
 *  
 *  @update  gess 1/29/99
 *  @param   aState determines whether we parse/tokenize or just cache.
 *  @return  current state
 */
void nsParser::SetUnusedInput(nsString& aBuffer)
{
  mUnusedInput=aBuffer;
}

/**
 *  Call this when you want to *force* the parser to terminate the
 *  parsing process altogether. This is binary -- so once you terminate
 *  you can't resume without restarting altogether.
 *  
 *  @update  gess 7/4/99
 *  @return  should return NS_OK once implemented
 */
NS_IMETHODIMP nsParser::Terminate(void)
{
  nsresult result = NS_OK;
  if (mParserContext && mParserContext->mDTD) {
    mParserContext->mDTD->Terminate();
    // XXX - [ until we figure out a way to break parser-sink circularity ]
    // Hack - Hold a reference until we are completely done...
    nsCOMPtr<nsIParser> kungFuDeathGrip(this); 
    mInternalState = result = NS_ERROR_HTMLPARSER_STOPPARSING;

    // CancelParsingEvents must be called to avoid leaking the nsParser object
    // @see bug 108049
    // If NS_PARSER_FLAG_PENDING_CONTINUE_EVENT is set then CancelParsingEvents 
    // will reset it so DidBuildModel will call DidBuildModel on the DTD. Note: 
    // The IsComplete() call inside of DidBuildModel looks at the pendingContinueEvents flag.
    CancelParsingEvents();
    DidBuildModel(result);
  }
  return result;
}


/**
 *  
 *  @update  gess 1/29/99
 *  @param   aState determines whether we parse/tokenize or just cache.
 *  @return  current state
 */
NS_IMETHODIMP nsParser::ContinueParsing()
{    
  // If the stream has already finished, there's a good chance
  // that we might start closing things down when the parser
  // is reenabled. To make sure that we're not deleted across
  // the reenabling process, hold a reference to ourselves.
  nsresult result=NS_OK;
  nsCOMPtr<nsIParser> kungFuDeathGrip(this); 

  mFlags |= NS_PARSER_FLAG_PARSER_ENABLED;

  PRBool isFinalChunk=(mParserContext && mParserContext->mStreamListenerState==eOnStop)? PR_TRUE:PR_FALSE;
  
  result=ResumeParse(PR_TRUE,isFinalChunk); // Ref. bug 57999
  
  if(result!=NS_OK) 
    result=mInternalState;
  
  return result;
}

/**
 *  Stops parsing temporarily. That's it will prevent the
 *  parser from building up content model.
 *
 *  @update  
 *  @return  
 */
NS_IMETHODIMP_(void) nsParser::BlockParser()
{
  mFlags &= ~NS_PARSER_FLAG_PARSER_ENABLED;
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: nsParser::BlockParser(), this=%p\n", this));
  MOZ_TIMER_STOP(mParseTime);
}

/**
 *  Open up the parser for tokenization, building up content 
 *  model..etc. However, this method does not resume parsing 
 *  automatically. It's the callers' responsibility to restart
 *  the parsing engine.
 *
 *  @update  
 *  @return  
 */
NS_IMETHODIMP_(void) nsParser::UnblockParser()
{
  mFlags |= NS_PARSER_FLAG_PARSER_ENABLED;
  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: nsParser::UnblockParser(), this=%p\n", this));
  MOZ_TIMER_START(mParseTime);
}

/**
 * Call this to query whether the parser is enabled or not.
 *
 *  @update  vidur 4/12/99
 *  @return  current state
 */
NS_IMETHODIMP_(PRBool) nsParser::IsParserEnabled()
{
  return mFlags & NS_PARSER_FLAG_PARSER_ENABLED;
}

/**
 * Call this to query whether the parser thinks it's done with parsing.
 *
 *  @update  rickg 5/12/01
 *  @return  complete state
 */
NS_IMETHODIMP_(PRBool) nsParser::IsComplete()
{
  return !(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT);
}


void nsParser::HandleParserContinueEvent() {
  mFlags &= ~NS_PARSER_FLAG_PENDING_CONTINUE_EVENT;
  ContinueParsing();
}

PRBool nsParser::CanInterrupt(void) {
  return mFlags & NS_PARSER_FLAG_CAN_INTERRUPT;
}

void nsParser::SetCanInterrupt(PRBool aCanInterrupt) {
  if (aCanInterrupt) {
    mFlags |= NS_PARSER_FLAG_CAN_INTERRUPT;
  }
  else {
    mFlags &= ~NS_PARSER_FLAG_CAN_INTERRUPT;
  }
}

/**
 *  This is the main controlling routine in the parsing process. 
 *  Note that it may get called multiple times for the same scanner, 
 *  since this is a pushed based system, and all the tokens may 
 *  not have been consumed by the scanner during a given invocation 
 *  of this method. 
 *
 *  @update  gess 01/04/99
 *  @param   aFilename -- const char* containing file to be parsed.
 *  @return  error code -- 0 if ok, non-zero if error.
 */
NS_IMETHODIMP
nsParser::Parse(nsIURI* aURL,
                nsIRequestObserver* aListener,
                PRBool aVerifyEnabled,
                void* aKey,
                nsDTDMode aMode)
{  

  NS_PRECONDITION(aURL, "Error: Null URL given");

  nsresult result=kBadURL;
  mObserver = aListener;
 
  if (aVerifyEnabled) {
    mFlags |= NS_PARSER_FLAG_DTD_VERIFICATION;
  }
  else {
    mFlags &= ~NS_PARSER_FLAG_DTD_VERIFICATION;
  }

  if(aURL) {
    nsCAutoString spec;
    nsresult rv = aURL->GetSpec(spec);
    if (rv != NS_OK) {      
      return rv;
    }
    NS_ConvertUTF8toUCS2 theName(spec);

    nsScanner* theScanner=new nsScanner(theName,PR_FALSE,mCharset,mCharsetSource);
    CParserContext* pc=new CParserContext(theScanner,aKey,mCommand,aListener);
    if(pc && theScanner) {
      pc->mMultipart=PR_TRUE;
      pc->mContextType=CParserContext::eCTURL;
      pc->mDTDMode=aMode;
      PushContext(*pc);
      result=NS_OK;
    }
    else{
      result=mInternalState=NS_ERROR_HTMLPARSER_BADCONTEXT;
    }
  }  
  return result;
}


/**
 * Cause parser to parse input from given stream 
 * @update	vidur 12/11/98
 * @param   aStream is the i/o source
 * @return  error code -- 0 if ok, non-zero if error.
 */
NS_IMETHODIMP
nsParser::Parse(nsIInputStream* aStream,
                const nsACString& aMimeType,
                PRBool aVerifyEnabled,
                void* aKey,
                nsDTDMode aMode)
{
  if (aVerifyEnabled) {
    mFlags |= NS_PARSER_FLAG_DTD_VERIFICATION;
  }
  else {
    mFlags &= ~NS_PARSER_FLAG_DTD_VERIFICATION;
  }
  
  nsresult  result=NS_ERROR_OUT_OF_MEMORY;

  //ok, time to create our tokenizer and begin the process
  nsAutoString theUnknownFilename(NS_LITERAL_STRING("unknown"));

  // references 
  nsScanner* theScanner=new nsScanner(theUnknownFilename,aStream,mCharset,mCharsetSource);

  CParserContext* pc=new CParserContext(theScanner,aKey,mCommand,0);
  if(pc && theScanner) {
    PushContext(*pc);
    pc->SetMimeType(aMimeType);
    pc->mStreamListenerState=eOnStart;  
    pc->mMultipart=PR_FALSE;
    pc->mContextType=CParserContext::eCTStream;
    pc->mDTDMode=aMode;
    mParserContext->mScanner->Eof();
    result=ResumeParse();
    pc=PopContext();
    delete pc;
  }
  else{
    result=mInternalState=NS_ERROR_HTMLPARSER_BADCONTEXT;
  }  
  return result;
}


/**
 * Call this method if all you want to do is parse 1 string full of HTML text.
 * In particular, this method should be called by the DOM when it has an HTML
 * string to feed to the parser in real-time.
 *
 * @update	gess5/11/98
 * @param   aSourceBuffer contains a string-full of real content
 * @param   aMimeType tells us what type of content to expect in the given string
 * @return  error code -- 0 if ok, non-zero if error.
 */
NS_IMETHODIMP
nsParser::Parse(const nsAString& aSourceBuffer,
                void* aKey,
                const nsACString& aMimeType,
                PRBool aVerifyEnabled,
                PRBool aLastCall,
                nsDTDMode aMode)
{ 

  //NOTE: Make sure that updates to this method don't cause 
  //      bug #2361 to break again! 

  nsresult result=NS_OK;

  if(aLastCall && aSourceBuffer.IsEmpty()) {
    // Nothing is being passed to the parser so return
    // immediately. mUnusedInput will get processed when
    // some data is actually passed in.
    return result;
  }

  // hack to pass on to the dtd the caller's desire to 
  // parse a fragment without worrying about containment rules
  if (aMode == eDTDMode_fragment)
    mCommand = eViewFragment;
  
  // Maintain a reference to ourselves so we don't go away 
  // till we're completely done. 
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);

  if(!aSourceBuffer.IsEmpty() || !mUnusedInput.IsEmpty()) {
    
    if (aVerifyEnabled) {
      mFlags |= NS_PARSER_FLAG_DTD_VERIFICATION;
    }
    else {
      mFlags &= ~NS_PARSER_FLAG_DTD_VERIFICATION;
    }
    
    CParserContext* pc=0;

    if((!mParserContext) || (mParserContext->mKey!=aKey))  { 
      //only make a new context if we dont have one, OR if we do, but has a different context key... 
  
      nsScanner* theScanner = new nsScanner(mUnusedInput,mCharset,mCharsetSource);
      NS_ENSURE_TRUE(theScanner, NS_ERROR_OUT_OF_MEMORY);
      
      nsIDTD *theDTD = 0; 
      eAutoDetectResult theStatus = eUnknownDetect; 

      if (mParserContext && mParserContext->mMimeType==aMimeType) {
        NS_ASSERTION(mParserContext->mDTD,"How come the DTD is null?"); // Ref. Bug 90379
        
        if (mParserContext) {
          // To fix bug 32263 we used create a new instance of the DTD!.
          // All we need is a new tokenizer which now gets created with
          // a parser context.
          theDTD = mParserContext->mDTD; 
          theStatus=mParserContext->mAutoDetectStatus; 
          //added this to fix bug 32022.
        }
      } 

      pc = new CParserContext(theScanner, aKey, mCommand, 0, theDTD, theStatus, aLastCall);
      NS_ENSURE_TRUE(pc, NS_ERROR_OUT_OF_MEMORY);

      PushContext(*pc); 

      pc->mMultipart=!aLastCall; //by default 
      if (pc->mPrevContext) { 
        pc->mMultipart |= pc->mPrevContext->mMultipart;  //if available 
      } 

      // start fix bug 40143
      if(pc->mMultipart) {
        pc->mStreamListenerState=eOnDataAvail;
        if(pc->mScanner) pc->mScanner->SetIncremental(PR_TRUE);
      }
      else {
        pc->mStreamListenerState=eOnStop;
        if(pc->mScanner) pc->mScanner->SetIncremental(PR_FALSE);
      }
      // end fix for 40143

      pc->mContextType=CParserContext::eCTString; 
      pc->SetMimeType(aMimeType);
      pc->mDTDMode=aMode;
      mUnusedInput.Truncate(0); 

      //printf("Parse(string) iterate: %i",PR_FALSE); 
      pc->mScanner->Append(aSourceBuffer); 
      // Do not interrupt document.write() - bug 95487
      result = ResumeParse(PR_FALSE, PR_FALSE, PR_FALSE);
    } 
    else { 
      mParserContext->mScanner->Append(aSourceBuffer); 
      if(!mParserContext->mPrevContext) {
        // Set stream listener state to eOnStop, on the final context - Fix 68160,
        // to guarantee DidBuildModel() call - Fix 36148
        if(aLastCall) {
          mParserContext->mStreamListenerState=eOnStop;
        }
        ResumeParse(PR_FALSE, PR_FALSE, PR_FALSE);
      }
    } 
  }//if 

  return result; 
}  

/**
 *
 *  @update  gess 04/01/99
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsParser::ParseFragment(const nsAString& aSourceBuffer,
                        void* aKey,
                        nsVoidArray& aTagStack,
                        PRUint32 anInsertPos,
                        const nsACString& aMimeType,
                        nsDTDMode aMode)
{
  nsresult result = NS_OK;
  nsAutoString  theContext;
  PRUint32 theCount = aTagStack.Count();
  PRUint32 theIndex = 0;
  
  while (theIndex++ < theCount){
    theContext.AppendLiteral("<");
    theContext.Append((PRUnichar*)aTagStack.ElementAt(theCount - theIndex));
    theContext.AppendLiteral(">");
  }
  
  theContext.AppendLiteral("<endnote>");       //XXXHack! I'll make this better later.
    
  //now it's time to try to build the model from this fragment

  mFlags &= ~NS_PARSER_FLAG_OBSERVERS_ENABLED; //disable observers for fragments
  result = Parse(theContext + aSourceBuffer,(void*)&theContext,aMimeType,PR_FALSE,PR_TRUE, aMode);
  mFlags |= NS_PARSER_FLAG_OBSERVERS_ENABLED; //now reenable.

  return result;
}

 
/**
 *  This routine is called to cause the parser to continue parsing it's underlying stream. 
 *  This call allows the parse process to happen in chunks, such as when the content is push 
 *  based, and we need to parse in pieces.
 *  
 *  An interesting change in how the parser gets used has led us to add extra processing to this method. 
 *  The case occurs when the parser is blocked in one context, and gets a parse(string) call in another context.
 *  In this case, the parserContexts are linked. No problem.
 *
 *  The problem is that Parse(string) assumes that it can proceed unabated, but if the parser is already
 *  blocked that assumption is false. So we needed to add a mechanism here to allow the parser to continue
 *  to process (the pop and free) contexts until 1) it get's blocked again; 2) it runs out of contexts.
 *
 *
 *  @update  rickg 03.10.2000 
 *  @param   allowItertion : set to true if non-script resumption is requested
 *  @param   aIsFinalChunk : tells us when the last chunk of data is provided.
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::ResumeParse(PRBool allowIteration, PRBool aIsFinalChunk, PRBool aCanInterrupt) {

  //printf("  Resume %i, prev-context: %p\n",allowIteration,mParserContext->mPrevContext);
  

  nsresult result=NS_OK;

  if((mFlags & NS_PARSER_FLAG_PARSER_ENABLED) && 
     mInternalState != NS_ERROR_HTMLPARSER_STOPPARSING) {


    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: nsParser::ResumeParse(), this=%p\n", this));
    MOZ_TIMER_START(mParseTime);

    result = WillBuildModel(mParserContext->mScanner->GetFilename());
    if (NS_FAILED(result)) {
      mFlags &= ~NS_PARSER_FLAG_CAN_TOKENIZE;
      return result;
    }

    if(mParserContext->mDTD) {

      mParserContext->mDTD->WillResumeParse(mSink);
      PRBool theFirstTime=PR_TRUE;
      PRBool theIterationIsOk=(theFirstTime || allowIteration||(!mParserContext->mPrevContext));
       
      while((result==NS_OK) && (theIterationIsOk)) {
        theFirstTime=PR_FALSE;
        if(!mUnusedInput.IsEmpty()) {
          if(mParserContext->mScanner) {
            // -- Ref: Bug# 22485 --
            // Insert the unused input into the source buffer 
            // as if it was read from the input stream. 
            // Adding UngetReadable() per vidur!!
            mParserContext->mScanner->UngetReadable(mUnusedInput);
           mUnusedInput.Truncate(0);
          }
        }

        //Only allow parsing to be interrupted in the subsequent call
        //to build model.
        SetCanInterrupt(aCanInterrupt); 
        nsresult theTokenizerResult = mFlags & NS_PARSER_FLAG_CAN_TOKENIZE ? Tokenize(aIsFinalChunk) : NS_OK;   // kEOF==2152596456
        result=BuildModel(); 

        if(result==NS_ERROR_HTMLPARSER_INTERRUPTED) {
          if(aIsFinalChunk)
            PostContinueEvent();
        }
        SetCanInterrupt(PR_FALSE); 

        theIterationIsOk=PRBool((kEOF!=theTokenizerResult) && (result!=NS_ERROR_HTMLPARSER_INTERRUPTED));

       // Make sure not to stop parsing too early. Therefore, before shutting down the 
        // parser, it's important to check whether the input buffer has been scanned to 
        // completion ( theTokenizerResult should be kEOF ). kEOF -> End of buffer.

        // If we're told to block the parser, we disable all further parsing 
        // (and cache any data coming in) until the parser is re-enabled.

        if(NS_ERROR_HTMLPARSER_BLOCK==result) {
          //BLOCK == 2152596464
          if (mParserContext->mDTD) {
            mParserContext->mDTD->WillInterruptParse(mSink);
          }
          
          BlockParser();
          return NS_OK;
        }
        
        else if (NS_ERROR_HTMLPARSER_STOPPARSING==result) {
          // Note: Parser Terminate() calls DidBuildModel.
          if(mInternalState!=NS_ERROR_HTMLPARSER_STOPPARSING) {
            DidBuildModel(mStreamStatus);
            mInternalState = result;
          }
          return NS_OK;
        }
                  
        else if(((NS_OK==result) && (theTokenizerResult==kEOF)) || (result==NS_ERROR_HTMLPARSER_INTERRUPTED)){

          PRBool theContextIsStringBased=PRBool(CParserContext::eCTString==mParserContext->mContextType);
          if( (eOnStop==mParserContext->mStreamListenerState) || 
              (!mParserContext->mMultipart) || theContextIsStringBased) {

            if(!mParserContext->mPrevContext) {
              if(eOnStop==mParserContext->mStreamListenerState) {

                DidBuildModel(mStreamStatus);          

                MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: nsParser::ResumeParse(), this=%p\n", this));
                MOZ_TIMER_STOP(mParseTime);

                MOZ_TIMER_LOG(("Parse Time (this=%p): ", this));
                MOZ_TIMER_PRINT(mParseTime);

                MOZ_TIMER_LOG(("DTD Time: "));
                MOZ_TIMER_PRINT(mDTDTime);

                MOZ_TIMER_LOG(("Tokenize Time: "));
                MOZ_TIMER_PRINT(mTokenizeTime);

                return NS_OK;
              }

            }
            else { 

              CParserContext* theContext=PopContext();
              if(theContext) {
                theIterationIsOk=PRBool(allowIteration && theContextIsStringBased);
                if(theContext->mCopyUnused) {
                  theContext->mScanner->CopyUnusedData(mUnusedInput);
                }
                delete theContext;
              }
              result = mInternalState;  
              aIsFinalChunk=(mParserContext && mParserContext->mStreamListenerState==eOnStop)? PR_TRUE:PR_FALSE;
              
                //...then intentionally fall through to WillInterruptParse()...
            }

          }             

        }

        if((kEOF==theTokenizerResult) || (result==NS_ERROR_HTMLPARSER_INTERRUPTED)) {
          result = (result == NS_ERROR_HTMLPARSER_INTERRUPTED) ? NS_OK : result;
          if (mParserContext->mDTD) {
            mParserContext->mDTD->WillInterruptParse(mSink);
          }
        }


      }//while
    }//if
    else {
      mInternalState=result=NS_ERROR_HTMLPARSER_UNRESOLVEDDTD;
    }
  }//if

  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: nsParser::ResumeParse(), this=%p\n", this));
  MOZ_TIMER_STOP(mParseTime);

  return (result==NS_ERROR_HTMLPARSER_INTERRUPTED) ? NS_OK : result;
}

/**
 *  This is where we loop over the tokens created in the 
 *  tokenization phase, and try to make sense out of them. 
 *
 *  @update  gess 01/04/99
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::BuildModel() {
  CParserContext* theRootContext = mParserContext;
  nsITokenizer*   theTokenizer = 0;

  nsresult result = NS_OK;
  if (mParserContext) {
    PRInt32 type = mParserContext->mDTD ? mParserContext->mDTD->GetType() : NS_IPARSER_FLAG_HTML;
    mParserContext->GetTokenizer(type, theTokenizer);
  }

  if (theTokenizer) {

    //Get the root DTD for use in model building...
    while (theRootContext->mPrevContext) {
      theRootContext = theRootContext->mPrevContext;
    }

    nsIDTD* theRootDTD = theRootContext->mDTD;
    if (theRootDTD) {      
      MOZ_TIMER_START(mDTDTime);
      
      result = theRootDTD->BuildModel(this, theTokenizer, nsnull, mSink);  
      
      MOZ_TIMER_STOP(mDTDTime);
    }
  }
  else{
    mInternalState = result = NS_ERROR_HTMLPARSER_BADTOKENIZER;
  }
  return result;
}


/**
 * 
 * @update	gess1/22/99
 * @param 
 * @return
 */
nsresult nsParser::GetTokenizer(nsITokenizer*& aTokenizer) {
  nsresult result = NS_OK;
  aTokenizer = nsnull;
  if(mParserContext) {
    PRInt32 type = mParserContext->mDTD ? mParserContext->mDTD->GetType() : NS_IPARSER_FLAG_HTML;
    result = mParserContext->GetTokenizer(type, aTokenizer);
  }
  return result;
}

/*******************************************************************
  These methods are used to talk to the netlib system...
 *******************************************************************/

#ifdef rickgdebug
#include <fstream.h>
  fstream* gOutFile;
#endif

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::OnStartRequest(nsIRequest *request, nsISupports* aContext) {

  NS_PRECONDITION(eNone==mParserContext->mStreamListenerState,
                  "Parser's nsIStreamListener API was not setup "
                  "correctly in constructor.");

  if (mObserver) {
    mObserver->OnStartRequest(request, aContext);
  }
  mParserContext->mStreamListenerState = eOnStart;
  mParserContext->mAutoDetectStatus = eUnknownDetect;
  mParserContext->mDTD = 0;
  mParserContext->mRequest = request;

  nsresult rv;
  nsCAutoString contentType;
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  NS_ASSERTION(channel, "parser needs a channel to find a dtd");

  rv = channel->GetContentType(contentType);
  if (NS_SUCCEEDED(rv))
  {
    mParserContext->SetMimeType(contentType);
  }

#ifdef rickgdebug
  gOutFile= new fstream("c:/temp/out.file",ios::trunc);
#endif

  return NS_OK;
}


#define UTF16_BE "UTF-16BE"
#define UTF16_LE "UTF-16LE"
#define UCS4_BE "UTF-32BE"
#define UCS4_LE "UTF-32LE"
#define UCS4_2143 "X-ISO-10646-UCS-4-2143"
#define UCS4_3412 "X-ISO-10646-UCS-4-3412"
#define UTF8 "UTF-8"

static inline PRBool IsSecondMarker(unsigned char aChar)
{
  switch (aChar) {
    case '!':
    case '?':
    case 'h':
    case 'H':
      return PR_TRUE;
    default:
      return PR_FALSE;
  }
}

static PRBool DetectByteOrderMark(const unsigned char* aBytes, PRInt32 aLen, nsCString& oCharset, PRInt32& oCharsetSource) {
 oCharsetSource= kCharsetFromAutoDetection;
 oCharset.Truncate();
 // See http://www.w3.org/TR/2000/REC-xml-20001006#sec-guessing
 // for details
 // Also, MS Win2K notepad now generate 3 bytes BOM in UTF8 as UTF8 signature
 // We need to check that
 // UCS2 BOM FEFF = UTF8 EF BB BF
 switch(aBytes[0])
	 {
   case 0x00:
     if(0x00==aBytes[1]) {
        // 00 00
        if((0xFE==aBytes[2]) && (0xFF==aBytes[3])) {
           // 00 00 FE FF UCS-4, big-endian machine (1234 order)
           oCharset.Assign(UCS4_BE);
        } else if((0x00==aBytes[2]) && (0x3C==aBytes[3])) {
           // 00 00 00 3C UCS-4, big-endian machine (1234 order)
           oCharset.Assign(UCS4_BE);
        } else if((0xFF==aBytes[2]) && (0xFE==aBytes[3])) {
           // 00 00 FF FE UCS-4, unusual octet order (2143)
           oCharset.Assign(UCS4_2143);
        } else if((0x3C==aBytes[2]) && (0x00==aBytes[3])) {
           // 00 00 3C 00 UCS-4, unusual octet order (2143)
           oCharset.Assign(UCS4_2143);
        } 
        oCharsetSource = kCharsetFromByteOrderMark;
     } else if((0x3C==aBytes[1]) && (0x00==aBytes[2])) {
        // 00 3C 00
        if(IsSecondMarker(aBytes[3])) {
           // 00 3C 00 SM UTF-16,  big-endian, no Byte Order Mark 
           oCharset.Assign(UTF16_BE); 
        } else if((0x00==aBytes[3])) {
           // 00 3C 00 00 UCS-4, unusual octet order (3412)
           oCharset.Assign(UCS4_3412);
        } 
        oCharsetSource = kCharsetFromByteOrderMark;
     }
   break;
   case 0x3C:
     if(0x00==aBytes[1] && (0x00==aBytes[3])) {
        // 3C 00 XX 00
        if(IsSecondMarker(aBytes[2])) {
           // 3C 00 SM 00 UTF-16,  little-endian, no Byte Order Mark 
           oCharset.Assign(UTF16_LE); 
        } else if((0x00==aBytes[2])) {
           // 3C 00 00 00 UCS-4, little-endian machine (4321 order)
           oCharset.Assign(UCS4_LE); 
        } 
        oCharsetSource = kCharsetFromByteOrderMark;
     // For html, meta tag detector is invoked before this so that we have 
     // to deal only with XML here.
     } else if(                     (0x3F==aBytes[1]) &&
               (0x78==aBytes[2]) && (0x6D==aBytes[3]) &&
               (0 == PL_strncmp("<?xml", (char*)aBytes, 5 ))) {
       // 3C 3F 78 6D
       // ASCII characters are in their normal positions, so we can safely
       // deal with the XML declaration in the old C way
       // XXX This part could be made simpler by using CWordTokenizer<char>,
       //     but bug 104479 must be fixed first.
       // The shortest string so far (strlen==5):
       // <?xml
       PRInt32 i;
       PRBool versionFound = PR_FALSE, encodingFound = PR_FALSE;
       for (i=6; i < aLen && !encodingFound; ++i) {
         // end of XML declaration?
         if ((((char*)aBytes)[i] == '?') && 
           ((i+1) < aLen) &&
           (((char*)aBytes)[i+1] == '>')) {
           break;
         }
         // Version is required.
         if (!versionFound) {
           // Want to avoid string comparisons, hence looking for 'n'
           // and only if found check the string leading to it. Not
           // foolproof, but fast.
           // The shortest string allowed before this is  (strlen==13):
           // <?xml version
           if ((((char*)aBytes)[i] == 'n') &&
             (i >= 12) && 
             (0 == PL_strncmp("versio", (char*)(aBytes+i-6), 6 ))) {
             // Fast forward through version
             char q = 0;
             for (++i; i < aLen; ++i) {
               char qi = ((char*)aBytes)[i];
               if (qi == '\'' || qi == '"') {
                 if (q && q == qi) {
                   //  ending quote
                   versionFound = PR_TRUE;
                   break;
                 } else {
                   // Starting quote
                   q = qi;
                 }
               }
             }
           }
         } else {
           // encoding must follow version
           // Want to avoid string comparisons, hence looking for 'g'
           // and only if found check the string leading to it. Not
           // foolproof, but fast.
           // The shortest allowed string before this (strlen==26):
           // <?xml version="1" encoding
           if ((((char*)aBytes)[i] == 'g') &&
             (i >= 25) && 
             (0 == PL_strncmp("encodin", (char*)(aBytes+i-7), 7 ))) {
             PRInt32 encStart = 0;
             char q = 0;
             for (++i; i < aLen; ++i) {
               char qi = ((char*)aBytes)[i];
               if (qi == '\'' || qi == '"') {
                 if (q && q == qi) {
                   PRInt32 count = i - encStart;
                   // encoding value is invalid if it is UTF-16
                   if (count > 0 && 
                     (0 != PL_strcmp("UTF-16", (char*)(aBytes+encStart)))) {
                     oCharset.Assign((char*)(aBytes+encStart),count);
                     oCharsetSource = kCharsetFromMetaTag;
                   }
                   encodingFound = PR_TRUE;
                   break;
                 } else {
                   encStart = i+1;
                   q = qi;
                 }
               }
             }
           }
         } // if (!versionFound)
       } // for
     }
   break;
   case 0xEF:  
     if((0xBB==aBytes[1]) && (0xBF==aBytes[2])) {
        // EF BB BF
        // Win2K UTF-8 BOM
        oCharset.Assign(UTF8); 
        oCharsetSource= kCharsetFromByteOrderMark;
     }
   break;
   case 0xFE:
     if(0xFF==aBytes[1]) {
        if(0x00==aBytes[2] && 0x00==aBytes[3]) {
          // FE FF 00 00  UCS-4, unusual octet order (3412)
          oCharset.Assign(UCS4_3412);
        } else {
          // FE FF UTF-16, big-endian 
          oCharset.Assign(UTF16_BE); 
        }
        oCharsetSource= kCharsetFromByteOrderMark;
     }
   break;
   case 0xFF:
     if(0xFE==aBytes[1]) {
        if(0x00==aBytes[2] && 0x00==aBytes[3]) 
         // FF FE 00 00  UTF-32, little-endian
           oCharset.Assign(UCS4_LE); 
        else
        // FF FE
        // UTF-16, little-endian 
           oCharset.Assign(UTF16_LE); 
        oCharsetSource= kCharsetFromByteOrderMark;
     }
   break;
   // case 0x4C: if((0x6F==aBytes[1]) && ((0xA7==aBytes[2] && (0x94==aBytes[3])) {
   //   We do not care EBCIDIC here....
   // }
   // break;
 }  // switch
 return !oCharset.IsEmpty();
}

inline const char GetNextChar(nsACString::const_iterator& aStart,
                              nsACString::const_iterator& aEnd)
{
  NS_ASSERTION(aStart != aEnd, "end of buffer");
  return (++aStart != aEnd) ? *aStart : '\0';
}

PRBool 
nsParser::DetectMetaTag(const char* aBytes, 
                        PRInt32 aLen, 
                        nsCString& aCharset, 
                        PRInt32& aCharsetSource) 
{
  aCharsetSource= kCharsetFromMetaTag;
  aCharset.SetLength(0);

  // XXX Only look inside HTML documents for now. For XML
  // documents we should be looking inside the XMLDecl.
  if (!mParserContext->mMimeType.EqualsLiteral(kHTMLTextContentType)) {
    return PR_FALSE;
  }

  // Fast and loose parsing to determine if we have a complete
  // META tag in this block, looking upto 2k into it.
  const nsASingleFragmentCString& str =
      Substring(aBytes, aBytes + PR_MIN(aLen, 2048));
  // XXXldb Should be const_char_iterator when FindInReadable supports it.
  nsACString::const_iterator begin, end;
  
  str.BeginReading(begin);
  str.EndReading(end);
  nsACString::const_iterator currPos(begin);
  nsACString::const_iterator tokEnd;
  nsACString::const_iterator tagEnd(begin);
  
  while (currPos != end) {
    if (!FindCharInReadable('<', currPos, end)) 
      break; // no tag found in this buffer

    if (GetNextChar(currPos, end) == '!' && 
        GetNextChar(currPos, end) == '-' &&
        GetNextChar(currPos, end) == '-') {
      // Found MDO ( <!-- ). Now search for MDC ( --[*s]> )
      PRBool foundMDC = PR_FALSE;
      PRBool foundMatch = PR_FALSE; 
      while (!foundMDC) {
        if (GetNextChar(currPos, end) == '-' && 
            GetNextChar(currPos, end) == '-') {
          foundMatch = !foundMatch; // toggle until we've matching "--"
        }
        else if (currPos == end) {
          return PR_FALSE; // Couldn't find --[*s]> in this buffer
        }
        else if (foundMatch && *currPos == '>') {
          foundMDC = PR_TRUE; // found comment end delimiter.
          ++currPos;
        }
      }
      continue; // continue searching for META tag.
    }

    // Find the end of the tag, break if incomplete
    tagEnd = currPos;
    if (!FindCharInReadable('>', tagEnd, end))
      break;

    // If this is not a META tag, continue to next loop
    if ( (*currPos != 'm' && *currPos != 'M') ||
         (*(++currPos) != 'e' && *currPos != 'E') ||
         (*(++currPos) != 't' && *currPos != 'T') ||
         (*(++currPos) != 'a' && *currPos != 'A') ) {
      currPos = tagEnd;
      continue;
    }

    // If could not find "charset" in this tag, skip this tag and try next
    tokEnd = tagEnd;
    if (!CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("CHARSET"), currPos, tokEnd)) {
      currPos = tagEnd;
      continue;
    }
    currPos = tokEnd;

    // skip spaces before '='
    while (*currPos == kSpace || *currPos == kNewLine ||
           *currPos == kCR || *currPos == kTab)  
      ++currPos;
    // skip '='
    if (*currPos != '=') {
      currPos = tagEnd;
      continue;
    }
    ++currPos;
    // skip spaces after '='
    while (*currPos == kSpace || *currPos == kNewLine ||
           *currPos == kCR || *currPos == kTab)  
      ++currPos;
          
    // skip open quote
    if ((*currPos == '\'' || *currPos == '\"'))
      ++currPos;

    // find the end of charset string
    tokEnd = currPos;
    while (*tokEnd != '\'' && *tokEnd != '\"' && tokEnd != tagEnd) 
      ++tokEnd;

    // return true if we successfully got something for charset
    if (currPos != tokEnd) {
      aCharset.Assign(currPos.get(), tokEnd.get() - currPos.get());
      return PR_TRUE;
    } 
    
    //nothing specified as charset, continue next loop
    currPos = tagEnd;
  } 
  
  return PR_FALSE;
}

typedef struct {
  PRBool mNeedCharsetCheck;
  nsParser* mParser;
  nsIParserFilter* mParserFilter;
  nsScanner* mScanner;
  nsIRequest* mRequest;
} ParserWriteStruct;

/*
 * This function is invoked as a result of a call to a stream's
 * ReadSegments() method. It is called for each contiguous buffer 
 * of data in the underlying stream or pipe. Using ReadSegments
 * allows us to avoid copying data to read out of the stream.
 */
static NS_METHOD
ParserWriteFunc(nsIInputStream* in,
                void* closure,
                const char* fromRawSegment,
                PRUint32 toOffset,
                PRUint32 count,
                PRUint32 *writeCount)
{
  nsresult result;
  ParserWriteStruct* pws = NS_STATIC_CAST(ParserWriteStruct*, closure);
  const char* buf = fromRawSegment;
  PRUint32 theNumRead = count;

  if (!pws) {
    return NS_ERROR_FAILURE;
  }

  if(pws->mNeedCharsetCheck) { 
    PRInt32 guessSource;
    nsCAutoString guess;
    nsCAutoString preferred; 
  
    pws->mNeedCharsetCheck = PR_FALSE; 
    if(pws->mParser->DetectMetaTag(buf, theNumRead,
                                   guess, guessSource) ||
       ((count >= 4) && 
        DetectByteOrderMark((const unsigned char*)buf, 
                            theNumRead, guess, guessSource))) { 
      nsCOMPtr<nsICharsetAlias> alias(do_GetService(NS_CHARSETALIAS_CONTRACTID));
      result = alias->GetPreferred(guess, preferred);
      // Only continue if it's a recognized charset and not
      // one of a designated set that we ignore.
      if (NS_SUCCEEDED(result) &&
          ((kCharsetFromByteOrderMark == guessSource) ||
           (!preferred.EqualsLiteral("UTF-16") &&
            !preferred.EqualsLiteral("UTF-16BE") &&
            !preferred.EqualsLiteral("UTF-16LE") &&
            !preferred.EqualsLiteral("UTF-32BE") &&
            !preferred.EqualsLiteral("UTF-32LE")))) {
        guess = preferred;
        pws->mParser->SetDocumentCharset(guess, guessSource); 
        pws->mParser->SetSinkCharset(preferred);
        nsCOMPtr<nsICachingChannel> channel(do_QueryInterface(pws->mRequest));
        if (channel) {
          nsCOMPtr<nsISupports> cacheToken;
          channel->GetCacheToken(getter_AddRefs(cacheToken));
          if (cacheToken) {
            nsCOMPtr<nsICacheEntryDescriptor> cacheDescriptor(do_QueryInterface(cacheToken));
            if (cacheDescriptor) {
#ifdef DEBUG
              nsresult rv =
#endif
                cacheDescriptor->SetMetaDataElement("charset",
                                                    guess.get());
              NS_ASSERTION(NS_SUCCEEDED(rv),"cannot SetMetaDataElement");
            }
          }
        }
      }
    } 
  } 

  if(pws->mParserFilter) 
    pws->mParserFilter->RawBuffer(buf, &theNumRead); 
  
  result = pws->mScanner->Append(buf, theNumRead);
  if (NS_SUCCEEDED(result)) {
    *writeCount = count;
  }

  return result;
}

/**
 *  
 *  
 *  @update  gess 1/4/99
 *  @param   pIStream contains the input chars
 *  @param   length is the number of bytes waiting input
 *  @return  error code (usually 0)
 */

nsresult nsParser::OnDataAvailable(nsIRequest *request, nsISupports* aContext, 
                                   nsIInputStream *pIStream, PRUint32 sourceOffset, PRUint32 aLength) 
{ 

 
NS_PRECONDITION((eOnStart == mParserContext->mStreamListenerState ||
                 eOnDataAvail == mParserContext->mStreamListenerState),
            "Error: OnStartRequest() must be called before OnDataAvailable()");

  nsresult result=NS_OK; 

  CParserContext *theContext=mParserContext; 
  
  while(theContext) { 
    if(theContext->mRequest!=request && theContext->mPrevContext) 
      theContext=theContext->mPrevContext; 
    else break; 
  } 

  if(theContext && theContext->mRequest==request) { 

    theContext->mStreamListenerState=eOnDataAvail; 

    if(eInvalidDetect==theContext->mAutoDetectStatus) { 
      if(theContext->mScanner) { 
        nsScannerIterator iter;
        theContext->mScanner->EndReading(iter);
        theContext->mScanner->SetPosition(iter, PR_TRUE);
      } 
    } 

    PRUint32 totalRead;
    ParserWriteStruct pws;
    pws.mNeedCharsetCheck = 
              ((0 == sourceOffset) && (mCharsetSource<kCharsetFromMetaTag)); 
    pws.mParser = this;
    pws.mParserFilter = mParserFilter;
    pws.mScanner = theContext->mScanner;
    pws.mRequest = request;

    result = pIStream->ReadSegments(ParserWriteFunc, (void*)&pws, aLength, &totalRead);
    if (NS_FAILED(result)) {
      return result;
    }

    result=ResumeParse(); 
  }

  return result; 
} 

/**
 *  This is called by the networking library once the last block of data
 *  has been collected from the net.
 *  
 *  @update  gess 04/01/99
 *  @param   
 *  @return  
 */
nsresult nsParser::OnStopRequest(nsIRequest *request, nsISupports* aContext,
                                 nsresult status)
{  

  nsresult result=NS_OK;
  
  if(eOnStart==mParserContext->mStreamListenerState) {
    //If you're here, then OnDataAvailable() never got called. 
    //Prior to necko, we never dealt with this case, but the problem may have existed.
    //Everybody can live with an empty input stream, so just resume parsing.
    result=ResumeParse(PR_TRUE,PR_TRUE);    
  }

  mParserContext->mStreamListenerState=eOnStop;
  mStreamStatus=status;

  if(mParserFilter)
     mParserFilter->Finish();

  mParserContext->mScanner->SetIncremental(PR_FALSE);
  result=ResumeParse(PR_TRUE,PR_TRUE);
  
  // If the parser isn't enabled, we don't finish parsing till
  // it is reenabled.


  // XXX Should we wait to notify our observers as well if the
  // parser isn't yet enabled?
  if (mObserver) {
    mObserver->OnStopRequest(request, aContext, status);
  }

#ifdef rickgdebug
  if(gOutFile){
    gOutFile->close();
    delete gOutFile;
    gOutFile=0;
  }
#endif

  return result;
}


/*******************************************************************
  Here comes the tokenization methods...
 *******************************************************************/


/**
 *  Part of the code sandwich, this gets called right before
 *  the tokenization process begins. The main reason for
 *  this call is to allow the delegate to do initialization.
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @return  TRUE if it's ok to proceed
 */
PRBool nsParser::WillTokenize(PRBool aIsFinalChunk){
  nsITokenizer* theTokenizer=0;
  nsresult result = NS_OK;
  if (mParserContext) {
    PRInt32 type = mParserContext->mDTD ? mParserContext->mDTD->GetType() : NS_IPARSER_FLAG_HTML;
    mParserContext->GetTokenizer(type, theTokenizer);
  }

  if (theTokenizer) {
    result = theTokenizer->WillTokenize(aIsFinalChunk,&mTokenAllocator);
  }  
  return result;
}


/**
 *  This is the primary control routine to consume tokens. 
 *	It iteratively consumes tokens until an error occurs or 
 *	you run out of data.
 *  
 *  @update  gess 01/04/99
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::Tokenize(PRBool aIsFinalChunk){
  
  nsITokenizer* theTokenizer = 0;
    
  nsresult result = NS_OK;

  if (mParserContext) {
    PRInt32 type = mParserContext->mDTD ? mParserContext->mDTD->GetType() : NS_IPARSER_FLAG_HTML;
    mParserContext->GetTokenizer(type, theTokenizer);
  }

  if (theTokenizer) { 
    if (mFlags & NS_PARSER_FLAG_FLUSH_TOKENS) {
      // For some reason tokens didn't get flushed ( probably
      // the parser got blocked before all the tokens in the
      // stack got handled ). Flush 'em now. Ref. bug 104856
      if (theTokenizer->GetCount() == 0) {
        mFlags &= ~NS_PARSER_FLAG_FLUSH_TOKENS; // reset since the tokens have been flushed.
        // Resume tokenization for the rest of the document 
        // since all the tokens in the tokenizer got flushed.
        result = Tokenize(aIsFinalChunk); 
      }
    }
    else {
      PRBool flushTokens=PR_FALSE;

      MOZ_TIMER_START(mTokenizeTime);

      WillTokenize(aIsFinalChunk);
      while (NS_SUCCEEDED(result)) {
        mParserContext->mScanner->Mark();
        result=theTokenizer->ConsumeToken(*mParserContext->mScanner, flushTokens);
        if (NS_FAILED(result)) {
          mParserContext->mScanner->RewindToMark();
          if (kEOF == result){
            break;
          }
          else if(NS_ERROR_HTMLPARSER_STOPPARSING==result) {
            result = Terminate();
            break;
          }
        }
        else if (flushTokens && (mFlags & NS_PARSER_FLAG_OBSERVERS_ENABLED)) {
          // I added the extra test of NS_PARSER_FLAG_OBSERVERS_ENABLED to fix Bug# 23931.
          // Flush tokens on seeing </SCRIPT> -- Ref: Bug# 22485 --
          // Also remember to update the marked position.
          mFlags |= NS_PARSER_FLAG_FLUSH_TOKENS;
          mParserContext->mScanner->Mark();
          break;
        }
      } 
      DidTokenize(aIsFinalChunk);

      MOZ_TIMER_STOP(mTokenizeTime);
    }  
  }
  else{
    result = mInternalState = NS_ERROR_HTMLPARSER_BADTOKENIZER;
  }
  
  return result;
}

/**
 *  This is the tail-end of the code sandwich for the
 *  tokenization process. It gets called once tokenziation
 *  has completed for each phase.
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @return  TRUE if all went well
 */
PRBool nsParser::DidTokenize(PRBool aIsFinalChunk){
  PRBool result=PR_TRUE;

  nsITokenizer* theTokenizer=0;
  nsresult rv = NS_OK;
  if (mParserContext) {
    PRInt32 type = mParserContext->mDTD ? mParserContext->mDTD->GetType() : NS_IPARSER_FLAG_HTML;
    mParserContext->GetTokenizer(type, theTokenizer);
  }

  if (NS_SUCCEEDED(rv) && theTokenizer) {
    result = theTokenizer->DidTokenize(aIsFinalChunk);
  }
  return result;
}

/** 
 * Get the channel associated with this parser
 * @update harishd,gagan 07/17/01
 * @param aChannel out param that will contain the result
 * @return NS_OK if successful
 */
NS_IMETHODIMP 
nsParser::GetChannel(nsIChannel** aChannel)
{
  nsresult result = NS_ERROR_NOT_AVAILABLE;
  if (mParserContext && mParserContext->mRequest)
    result = CallQueryInterface(mParserContext->mRequest, aChannel);
  return result;
}

/** 
 * Get the DTD associated with this parser
 * @update vidur 9/29/99
 * @param aDTD out param that will contain the result
 * @return NS_OK if successful, NS_ERROR_FAILURE for runtime error
 */
NS_IMETHODIMP 
nsParser::GetDTD(nsIDTD** aDTD)
{
  if (mParserContext) {
    *aDTD = mParserContext->mDTD;
    NS_IF_ADDREF(mParserContext->mDTD);
  }
  
  return NS_OK;
}

