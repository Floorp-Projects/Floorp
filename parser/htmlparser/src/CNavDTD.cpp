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

//#define ENABLE_CRC
//#define ALLOW_TR_AS_CHILD_OF_TABLE  //by setting this to true, TR is allowable directly in TABLE.

#define ENABLE_RESIDUALSTYLE  

     
#include "nsDebug.h"
#include "nsIAtom.h"
#include "CNavDTD.h" 
#include "nsHTMLTokens.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsIParser.h"
#include "nsIHTMLContentSink.h" 
#include "nsScanner.h"
#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"
#include "nsDTDUtils.h"
#include "nsHTMLTokenizer.h"
#include "nsTime.h"
#include "nsParserNode.h"
#include "nsHTMLEntities.h"
#include "nsLinebreakConverter.h"
#include "nsIFormProcessor.h"
#include "nsVoidArray.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "prmem.h"
#include "nsIServiceManager.h"

#ifdef NS_DEBUG
#include "nsLoggingSink.h"
#endif


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_INAVHTML_DTD_IID); 

static NS_DEFINE_CID(kFormProcessorCID, NS_FORMPROCESSOR_CID); 
 
#ifdef DEBUG
static const  char kNullToken[] = "Error: Null token given";
static const  char kInvalidTagStackPos[] = "Error: invalid tag stack position";
#endif

#ifdef  ENABLE_CRC
static char gShowCRC;
#endif

#include "nsElementTable.h"


#ifdef MOZ_PERF_METRICS
#  define START_TIMER()                    \
    if(mParser) MOZ_TIMER_START(mParser->mParseTime); \
    if(mParser) MOZ_TIMER_START(mParser->mDTDTime); 

#  define STOP_TIMER()                     \
    if(mParser) MOZ_TIMER_STOP(mParser->mParseTime); \
    if(mParser) MOZ_TIMER_STOP(mParser->mDTDTime); 
#else
#  define STOP_TIMER() 
#  define START_TIMER()
#endif

/************************************************************************
  And now for the main class -- CNavDTD...
 ************************************************************************/


#define NS_DTD_FLAG_NONE                   0x00000000
#define NS_DTD_FLAG_HAS_OPEN_HEAD          0x00000001
#define NS_DTD_FLAG_HAS_OPEN_BODY          0x00000002
#define NS_DTD_FLAG_HAS_OPEN_FORM          0x00000004
#define NS_DTD_FLAG_HAS_OPEN_SCRIPT        0x00000008
#define NS_DTD_FLAG_HAD_BODY               0x00000010
#define NS_DTD_FLAG_HAD_FRAMESET           0x00000020
#define NS_DTD_FLAG_ENABLE_RESIDUAL_STYLE  0x00000040
#define NS_DTD_FLAG_SCRIPT_ENABLED         0x00000100
#define NS_DTD_FLAG_FRAMES_ENABLED         0x00000200
#define NS_DTD_FLAG_ALTERNATE_CONTENT      0x00000400 // NOFRAMES, NOSCRIPT 
#define NS_DTD_FLAG_MISPLACED_CONTENT      0x00000800
#define NS_DTD_FLAG_STOP_PARSING           0x00001000

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 4/8/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult CNavDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kIDTDIID)) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (CNavDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(CNavDTD)
NS_IMPL_RELEASE(CNavDTD)

/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CNavDTD::CNavDTD() : nsIDTD(), 
    mMisplacedContent(0), 
    mSkippedContent(0),
    mSink(0),
    mTokenAllocator(0),
    mTempContext(0),
    mParser(0),       
    mTokenizer(0),
    mDTDMode(eDTDMode_quirks),
    mDocType(eHTML3_Quirks), // why not eHTML_Quirks?
    mParserCommand(eViewNormal),
    mSkipTarget(eHTMLTag_unknown),
    mLineNumber(1),
    mOpenMapCount(0),
    mFlags(NS_DTD_FLAG_NONE)
#ifdef ENABLE_CRC
    ,mComputedCRC32(0),
    mExpectedCRC32(0)
#endif
{
  mBodyContext=new nsDTDContext();
}

/**
 * 
 * @update  gess1/8/99
 * @param 
 * @return
 */
const nsIID& CNavDTD::GetMostDerivedIID(void)const {
  return kClassIID;
}


#ifdef NS_DEBUG

nsLoggingSink* GetLoggingSink() {

  //these are used when you want to generate a log file for contentsink construction...

  static  PRBool checkForPath=PR_TRUE;
  static  nsLoggingSink *theSink=0;
  static  const char* gLogPath=0; 

  if(checkForPath) {
    
    // we're only going to check the environment once per session.

    gLogPath = /* "c:/temp/parse.log"; */ PR_GetEnv("PARSE_LOGFILE"); 
    checkForPath=PR_FALSE;
  }
  

  if(gLogPath && (!theSink)) {
    static  nsLoggingSink gLoggingSink;

    PRIntn theFlags = 0;

     // create the file exists, only open for read/write
     // otherwise, create it
    if(PR_Access(gLogPath,PR_ACCESS_EXISTS) != PR_SUCCESS)
      theFlags = PR_CREATE_FILE;
    theFlags |= PR_RDWR;

     // open the record file
    PRFileDesc *theLogFile = PR_Open(gLogPath,theFlags,0);
    gLoggingSink.SetOutputStream(theLogFile,PR_TRUE);
    theSink=&gLoggingSink;
  }

  return theSink;
}
 
#endif

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CNavDTD::~CNavDTD(){
  if(mBodyContext) {
    delete mBodyContext;
    mBodyContext=0;
  }

  if(mTempContext) {
    delete mTempContext;
    mTempContext=0;
  }

  
#ifdef NS_DEBUG
  if(mSink) {
    nsLoggingSink *theLogSink=GetLoggingSink();
    if(mSink==theLogSink) {
      theLogSink->ReleaseProxySink();
    }
  }
#endif

  NS_IF_RELEASE(mSink);
}
 

/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update  gess 25May2000
 * @param 
 * @return
 */
nsresult CNavDTD::CreateNewInstance(nsIDTD** aInstancePtrResult)
{
  nsresult result = NS_NewNavHTMLDTD(aInstancePtrResult);
  NS_ENSURE_SUCCESS(result, result);

  CNavDTD* dtd = NS_STATIC_CAST(CNavDTD*, *aInstancePtrResult);
    
  dtd->mDTDMode = mDTDMode;
  dtd->mParserCommand = mParserCommand;
  dtd->mDocType = mDocType;

  return result;
}

/**
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type.
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @param aParserContext -- the context for this document (knows
 *           the content type, document type, parser command, etc).
 * @return eUnknownDetect if you don't know how to parse it,
 *         eValidDetect if you do, but someone may have a better idea,
 *         ePrimaryDetect if you think you know best
 */
NS_IMETHODIMP_(eAutoDetectResult)
CNavDTD::CanParse(CParserContext& aParserContext)
{
  NS_ASSERTION(!aParserContext.mMimeType.IsEmpty(),
               "How'd we get here with an unknown type?");
  
  if (aParserContext.mParserCommand != eViewSource &&
      aParserContext.mDocType != eXML) {
    // This means that we're
    // 1) Looking at a type the parser claimed to know how to handle (so XML
    //    or HTML or a plaintext type)
    // 2) Not looking at XML
    //
    // Therefore, we want to handle this data with this DTD
    return ePrimaryDetect;
  }

  return eUnknownDetect;
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	rickg 03.20.2000
  * @param	aParserContext
  * @param	aSink
  * @return	error code (almost always 0)
  */
nsresult CNavDTD::WillBuildModel(const CParserContext& aParserContext,
                                 nsITokenizer* aTokenizer,
                                 nsIContentSink* aSink) {
  nsresult result=NS_OK;

  mFilename=aParserContext.mScanner->GetFilename();
  mFlags = NS_DTD_FLAG_ENABLE_RESIDUAL_STYLE; // residual style is always on. This will also reset the flags
  mLineNumber = 1;
  mDTDMode = aParserContext.mDTDMode;
  mParserCommand = aParserContext.mParserCommand;
  mMimeType = aParserContext.mMimeType;
  mDocType = aParserContext.mDocType;
  mSkipTarget = eHTMLTag_unknown;
  mTokenizer = aTokenizer;
  mBodyContext->SetNodeAllocator(&mNodeAllocator);

  if(!aParserContext.mPrevContext && aSink) {

#ifdef DEBUG
    mBodyContext->ResetCounters();
#endif

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::WillBuildModel(), this=%p\n", this));
    
    result = aSink->WillBuildModel();
    
    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::WillBuildModel(), this=%p\n", this));
    START_TIMER();

    if (NS_SUCCEEDED(result) && !mSink) {
      result = CallQueryInterface(aSink, &mSink);
      if (NS_FAILED(result)) {
        mFlags |= NS_DTD_FLAG_STOP_PARSING;
        return result;
      }
    }
    
    //let's see if the environment is set up for us to write output to
    //a logging sink. If so, then we'll create one, and make it the
    //proxy for the real sink we're given from the parser.
#ifdef NS_DEBUG
    nsLoggingSink *theLogSink=GetLoggingSink();
    if(theLogSink) {   
      theLogSink->SetProxySink(mSink);
      mSink=theLogSink;
    }
#endif    

   if(mSink) {
      PRBool enabled;
      mSink->IsEnabled(eHTMLTag_frameset, &enabled);
      if(enabled) {
        mFlags |= NS_DTD_FLAG_FRAMES_ENABLED;
      }
      
      mSink->IsEnabled(eHTMLTag_script, &enabled);
      if(enabled) {
        mFlags |= NS_DTD_FLAG_SCRIPT_ENABLED;
      }
    }
    
#ifdef ENABLE_CRC
    mComputedCRC32=0;
    mExpectedCRC32=0;
#endif
  }

  return result;
}


/**
  * This is called when it's time to read as many tokens from the tokenizer
  * as you can. Not all tokens may make sense, so you may not be able to
  * read them all (until more come in later).
  *
  * @update gess5/18/98
  * @param  aParser is the parser object that's driving this process
  * @return error code (almost always NS_OK)
  */
nsresult CNavDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  NS_PRECONDITION(mBodyContext!=nsnull,"Create a context before calling build model");

  nsresult result = NS_OK;

  if (aTokenizer && aParser) {
    nsITokenizer*  oldTokenizer = mTokenizer;

    mTokenizer      = aTokenizer;
    mParser         = (nsParser*)aParser;
    mTokenAllocator = mTokenizer->GetTokenAllocator();
    
    if (mSink) {
      if (mBodyContext->GetCount() == 0) {
        CStartToken* theToken=nsnull;
        if(ePlainText==mDocType) {
          //we do this little trick for text files, in both normal and viewsource mode...
          theToken=NS_STATIC_CAST(CStartToken*,mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_pre));
          if(theToken) {
            mTokenizer->PushTokenFront(theToken);
          }
        }

        // always open a body if frames are disabled....
        if(!(mFlags & NS_DTD_FLAG_FRAMES_ENABLED)) {
          theToken=NS_STATIC_CAST(CStartToken*,mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_body,NS_LITERAL_STRING("body")));
          mTokenizer->PushTokenFront(theToken);
        }
          //if the content model is empty, then begin by opening <html>...
        theToken = (CStartToken*)mTokenizer->GetTokenAt(0);
        if (theToken) {
          eHTMLTags theTag = (eHTMLTags)theToken->GetTypeID();
          eHTMLTokenTypes theType = eHTMLTokenTypes(theToken->GetTokenType());
          if (theTag != eHTMLTag_html || theType != eToken_start) {
            theToken = NS_STATIC_CAST(CStartToken*,mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_html,NS_LITERAL_STRING("html")));          	
            if (theToken) {
              mTokenizer->PushTokenFront(theToken); //this token should get pushed on the context stack.
            }
          }
        }
        else {
          theToken = NS_STATIC_CAST(CStartToken*,mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_html,NS_LITERAL_STRING("html")));
          if (theToken) {
            mTokenizer->PushTokenFront(theToken); //this token should get pushed on the context stack.
          }
        }
      }
    
      mSink->WillProcessTokens();

      while (NS_SUCCEEDED(result)) {
        if (!(mFlags & NS_DTD_FLAG_STOP_PARSING)) {
          CToken* theToken = mTokenizer->PopToken();
          if (theToken) { 
            result = HandleToken(theToken,aParser);
          }
          else break;
        }
        else {
          result = NS_ERROR_HTMLPARSER_STOPPARSING;
          break;
        }

        if ((NS_ERROR_HTMLPARSER_INTERRUPTED == mSink->DidProcessAToken())) {
          // The content sink has requested that DTD interrupt processing tokens
          // So we need to make sure the parser is in a state where it can be
          // interrupted. 
          // The mParser->CanInterrupt will return TRUE if BuildModel was called
          // from a place in the parser where it prepared to handle a return value of
          // NS_ERROR_HTMLPARSER_INTERRUPTED.
          // If the parser has mPrevContext then it may be processing
          // Script so we should not allow it to be interrupted.
        
          if ((mParser->CanInterrupt()) && 
            (nsnull == mParser->PeekContext()->mPrevContext) && 
            (eHTMLTag_unknown==mSkipTarget)) {     
            result = NS_ERROR_HTMLPARSER_INTERRUPTED;
            break;
          }
        }
      }//while
      mTokenizer = oldTokenizer;
    }
    else {
      result = mFlags & NS_DTD_FLAG_STOP_PARSING ? NS_ERROR_HTMLPARSER_STOPPARSING : result;
    }
  }

  return result;
}

/**
 * @param aTarget - Tag that was neglected in the document.
 * @param aType   - Specifies the type of the target. Ex. start, end, text, etc.
 * @param aParser - Parser to drive this process
 * @param aSink   - HTML Content sink
 */
nsresult
CNavDTD::BuildNeglectedTarget(eHTMLTags aTarget,
                              eHTMLTokenTypes aType,
                              nsIParser* aParser,
                              nsIContentSink* aSink)
{ 
  NS_ASSERTION(mTokenizer, "tokenizer is null! unable to build target.");
  NS_ASSERTION(mTokenAllocator, "unable to create tokens without an allocator.");
  if (!mTokenizer || !mTokenAllocator)
    return NS_OK;
  if (eHTMLTag_unknown != mSkipTarget && eHTMLTag_title == aTarget) {
    PRInt32 size = mSkippedContent.GetSize();
    // Note: The first location of the skipped content 
    // deque contains the opened-skip-target. Do not include
    // that when guessing title contents. The term "guessing" 
    // is used because the document did not contain an end title
    // and hence it's almost impossible to know what markup
    // should belong in the title. The assumption used here is that
    // if the markup is anything other than "text", or "entity" or,
    // "whitespace" then it's least likely to belong in the title.
    PRInt32 index;
    for (index = 1; index < size; index++) {
      CHTMLToken* token = 
        NS_REINTERPRET_CAST(CHTMLToken*, mSkippedContent.ObjectAt(index));
      NS_ASSERTION(token, "there is a null token in the skipped content list!");
      eHTMLTokenTypes type = eHTMLTokenTypes(token->GetTokenType());
      if (eToken_whitespace != type && 
          eToken_newline != type    && 
          eToken_text != type       && 
          eToken_entity != type     &&
          eToken_attribute != type) {
        // Now pop the tokens that do not belong ( just a guess work )
        // in the title and push them into the tokens queue.
        while (size != index++) {
          token = NS_REINTERPRET_CAST(CHTMLToken*, mSkippedContent.Pop()); 
          mTokenizer->PushTokenFront(token);
        }
        break;
      }
    }
  }
  CHTMLToken* target = 
      NS_STATIC_CAST(CHTMLToken*, mTokenAllocator->CreateTokenOfType(aType, aTarget));
  mTokenizer->PushTokenFront(target);
  return BuildModel(aParser, mTokenizer, 0, aSink);
}

/**
 * 
 * @update  gess5/18/98
 * @param 
 * @return 
 */
nsresult CNavDTD::DidBuildModel(nsresult anErrorCode,
                                PRBool aNotifySink,
                                nsIParser* aParser,
                                nsIContentSink* aSink)
{
  if (!aSink)
    return NS_OK;
  nsresult result = NS_OK;
  if (aParser && aNotifySink) { 
    if (NS_OK == anErrorCode) {
      if (eHTMLTag_unknown != mSkipTarget) {
        // Looks like there is an open target ( ex. <title>, <textarea> ).
        // Create a matching target to handle the unclosed target.
        result = BuildNeglectedTarget(mSkipTarget, eToken_end, aParser, aSink);
        NS_ENSURE_SUCCESS(result , result);
      }
      if (!(mFlags & (NS_DTD_FLAG_HAD_FRAMESET | NS_DTD_FLAG_HAD_BODY))) {
        // This document is not a frameset document, however, it did not contain
        // a body tag either. So, make one!. Note: Body tag is optional per spec..
        result = BuildNeglectedTarget(eHTMLTag_body, eToken_start, aParser, aSink);
        NS_ENSURE_SUCCESS(result , result);
      }
      if (mFlags & NS_DTD_FLAG_MISPLACED_CONTENT) {
        // Looks like the misplaced contents are not processed yet.
        // Here is our last chance to handle the misplaced content.
        mFlags &= ~NS_DTD_FLAG_MISPLACED_CONTENT; 
        
        // mContextTopIndex refers to the misplaced content's legal parent index.
        result = HandleSavedTokens(mBodyContext->mContextTopIndex);
        NS_ENSURE_SUCCESS(result, result);

        mBodyContext->mContextTopIndex = -1; 
      }          
      //now let's disable style handling to save time when closing remaining stack members...
      mFlags &= ~NS_DTD_FLAG_ENABLE_RESIDUAL_STYLE;
      while (mBodyContext->GetCount() > 0) { 
        result = CloseContainersTo(mBodyContext->Last(), PR_FALSE);
        if (NS_FAILED(result)) {
          //No matter what, you need to call did build model.
          aSink->DidBuildModel();
          return result;
        }
      } 
    } 
    else {
      //If you're here, then an error occured, but we still have nodes on the stack.
      //At a minimum, we should grab the nodes and recycle them.
      //Just to be correct, we'll also recycle the nodes.
      while (mBodyContext->GetCount() > 0) { 
        nsEntryStack* theChildStyles = 0;
        nsCParserNode* theNode = mBodyContext->Pop(theChildStyles);
        IF_DELETE(theChildStyles,&mNodeAllocator);
        IF_FREE(theNode, &mNodeAllocator);
      } 
    }

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::DidBuildModel(), this=%p\n", this));

#ifdef  ENABLE_CRC

      //let's only grab this state once! 
    if (!gShowCRC) { 
      gShowCRC=1; //this only indicates we'll not initialize again. 
      char* theEnvString = PR_GetEnv("RICKG_CRC"); 
      if (theEnvString){ 
        if (('1'== theEnvString[0]) || ('Y'== theEnvString[0]) || ('y'== theEnvString[0])){ 
          gShowCRC=2;  //this indicates that the CRC flag was found in the environment. 
        } 
      } 
    } 

    if (2 == gShowCRC) { 
      if (mComputedCRC32 != mExpectedCRC32) { 
        if (mExpectedCRC32 != 0) { 
          printf("CRC Computed: %u  Expected CRC: %u\n,",mComputedCRC32,mExpectedCRC32); 
          result = aSink->DidBuildModel(); 
        } 
        else { 
          printf("Computed CRC: %u.\n",mComputedCRC32); 
          result = aSink->DidBuildModel(); 
          NS_ENSURE_SUCCESS(result, result);
        } 
      } 
    }
#endif

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::DidBuildModel(), this=%p\n", this));
    START_TIMER();

      //Now make sure the misplaced content list is empty,
      //by forcefully recycling any tokens we might find there.

    CToken* theToken = 0;
    while ((theToken = (CToken*)mMisplacedContent.Pop())) {
      IF_FREE(theToken, mTokenAllocator);
    }
  } //if aparser

  //No matter what, you need to call did build model.
  return aSink->DidBuildModel(); 
}

NS_IMETHODIMP_(void) 
CNavDTD::Terminate() 
{ 
  mFlags |= NS_DTD_FLAG_STOP_PARSING; 
}


NS_IMETHODIMP_(PRInt32) 
CNavDTD::GetType() 
{ 
  return NS_IPARSER_FLAG_HTML; 
}

/**
 * --- Backwards compatibility ---
 * Use this method to determine if the tag in question needs a BODY.
 * ---
 */
static 
PRBool DoesRequireBody(CToken* aToken,nsITokenizer* aTokenizer) {

  PRBool result=PR_FALSE;

  if(aToken) {
    eHTMLTags theTag=(eHTMLTags)aToken->GetTypeID();
    if(gHTMLElements[theTag].HasSpecialProperty(kRequiresBody)) {
      if(theTag==eHTMLTag_input) {
        // IE & Nav4x opens up a body for type=text - Bug 66985
        PRInt32 ac=aToken->GetAttributeCount();
        for(PRInt32 i=0; i<ac; ++i) {
          CAttributeToken* attr=NS_STATIC_CAST(CAttributeToken*,aTokenizer->GetTokenAt(i));
          const nsAString& name=attr->GetKey();
          const nsAString& value=attr->GetValue();

          if((name.EqualsLiteral("type") || 
              name.EqualsLiteral("TYPE"))    
             && 
             !(value.EqualsLiteral("hidden") || 
             value.EqualsLiteral("HIDDEN"))) {
            result=PR_TRUE; 
            break;
          }
        }//for
      }
      else {
        result=PR_TRUE;
      }
    }
  }
 
  return result;
}

static void
InPlaceConvertLineEndings( nsAString& aString )
{
    // go from '\r\n' or '\r' to '\n'
  nsAString::iterator iter;
  aString.BeginWriting(iter);

  PRUnichar* S = iter.get();
  size_t N = iter.size_forward();

    // this fragment must be the entire string because
    //  (a) no multi-fragment string is writable, so only an illegal cast could give us one, and
    //  (b) else we would have to do more work (watching for |to| to fall off the end)
  NS_ASSERTION(aString.Length() == N, "You cheated... multi-fragment strings are never writable!");

    // we scan/convert in two phases (but only one pass over the string)
    // until we have to skip a character, we only need to touch end-of-line chars
    // after that, we'll have to start moving every character we want to keep

    // use array indexing instead of pointers, because compilers optimize that better


    // this first loop just converts line endings... no characters get moved
  size_t i = 0;
  PRBool just_saw_cr = PR_FALSE;
  for ( ; i < N; ++i )
    {
        // if it's something we need to convert...
      if ( S[i] == '\r' )
        {
          S[i] = '\n';
          just_saw_cr = PR_TRUE;
        }
      else
        {
            // else, if it's something we need to skip...
            //   i.e., a '\n' immediately following a '\r',
            //   then we need to start moving any character we want to keep
            //   and we have a second loop for that, so get out of this one
          if ( S[i] == '\n' && just_saw_cr )
            break;

          just_saw_cr = PR_FALSE;
        }
    }


    // this second loop handles the rest of the buffer, moving characters down
    //  _and_ converting line-endings as it goes
    //  start the loop at |from = i| so that that |just_saw_cr| gets cleared automatically
  size_t to = i;
  for ( size_t from = i; from < N; ++from )
    {
        // if it's something we need to convert...
      if ( S[from] == '\r' )
        {
          S[to++] = '\n';
          just_saw_cr = PR_TRUE;
        }
      else
        {
            // else, if it's something we need to copy...
            //  i.e., NOT a '\n' immediately following a '\r'
          if ( S[from] != '\n' || !just_saw_cr )
            S[to++] = S[from];

          just_saw_cr = PR_FALSE;
        }
    }

    // if we chopped characters out of the string, we need to shorten it logically
  if ( to < N )
    aString.SetLength(to);
}

/**
 *  This big dispatch method is used to route token handler calls to the right place.
 *  What's wrong with it? This table, and the dispatch methods themselves need to be 
 *  moved over to the delegate. Ah, so much to do...
 *  
 *  @update  gess 12/1/99
 *  @param   aToken 
 *  @param   aParser 
 *  @return  
 */
nsresult CNavDTD::HandleToken(CToken* aToken,nsIParser* aParser){
  nsresult  result=NS_OK;

  if(aToken) {
    CHTMLToken*     theToken= NS_STATIC_CAST(CHTMLToken*, aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    eHTMLTags       theTag=(eHTMLTags)theToken->GetTypeID();
    PRBool          execSkipContent=PR_FALSE;

    aToken->SetLineNumber(mLineNumber);
    
    mLineNumber += aToken->GetNewlineCount();
   
    /* ---------------------------------------------------------------------------------
       To understand this little piece of code, you need to look below too.
       In essence, this code caches "skipped content" until we find a given skiptarget.
       Once we find the skiptarget, we take all skipped content up to that point and
       coallate it. Then we push those tokens back onto the tokenizer deque.
       ---------------------------------------------------------------------------------
     */

      // printf("token: %p\n",aToken);

   if(mSkipTarget){  //handle a preexisting target...
      if((theTag==mSkipTarget) && (eToken_end==theType)){
        mSkipTarget=eHTMLTag_unknown; //stop skipping.  
        //mTokenizer->PushTokenFront(aToken); //push the end token...
        execSkipContent=PR_TRUE;
        IF_FREE(aToken, mTokenAllocator);
        theToken=(CHTMLToken*)mSkippedContent.PopFront();
        theType=eToken_start;
      }
      else {
        mSkippedContent.Push(theToken);
        return result;
      }
    }
    else if(mFlags & NS_DTD_FLAG_ALTERNATE_CONTENT) {
      if(theTag != mBodyContext->Last() || theType!=eToken_end) {
        // attribute source is a part of start token.
        if(theType!=eToken_attribute) {
          aToken->AppendSourceTo(mScratch);
        }
        IF_FREE(aToken, mTokenAllocator);
        return result;
      }
      else {
        // If you're here then we have either seen a /noscript,
        // or /noframes, or /iframe. After handling the text token 
        // intentionally fall thro' to handle the current end token.
        CTextToken theTextToken(mScratch);        
        result=HandleStartToken(&theTextToken);
        
        if(NS_FAILED(result)) {
          return result;
        }

        mScratch.Truncate();
        mScratch.SetCapacity(0);
      }
    }
    else if(mFlags & NS_DTD_FLAG_MISPLACED_CONTENT) {
      // Included TD & TH to fix Bug# 20797
      static eHTMLTags gLegalElements[]={eHTMLTag_table,eHTMLTag_thead,eHTMLTag_tbody,
                                         eHTMLTag_tr,eHTMLTag_td,eHTMLTag_th,eHTMLTag_tfoot};
      if(theToken) {
        eHTMLTags theParentTag=mBodyContext->Last();
        theTag=(eHTMLTags)theToken->GetTypeID();
        if(FindTagInSet(theTag, gLegalElements,
                        NS_ARRAY_LENGTH(gLegalElements)) ||
           (gHTMLElements[theParentTag].CanContain(theTag,mDTDMode) &&
            // Here's a problem.  If theTag is legal in here, we don't move it
            // out.  So if we're moving stuff out of here, the parent of theTag
            // gets closed at this point.  But some things are legal
            // _everywhere_ and hence would effectively close out misplaced
            // content in tables.  This is undesirable, so treat them as
            // illegal here so they'll be shipped out with their parents and
            // siblings.  See bug 40855 for an explanation (that bug was for
            // comments, but the same issues arise with whitespace, newlines,
            // noscript, etc).  Script is special, though.  Shipping it out
            // breaks document.write stuff.  See bug 243064.
            (!gHTMLElements[theTag].HasSpecialProperty(kLegalOpen) ||
             theTag == eHTMLTag_script))) {
            
          mFlags &= ~NS_DTD_FLAG_MISPLACED_CONTENT; // reset the state since all the misplaced tokens are about to get handled.

          result = HandleSavedTokens(mBodyContext->mContextTopIndex);
          NS_ENSURE_SUCCESS(result, result);

          mBodyContext->mContextTopIndex = -1; 

          if (mSkipTarget) {
            mSkippedContent.Push(theToken);
            return result;
          }
          // Fall through if the skipped content collection is |not| in progress - bug 124788
        }
        else {
          PushIntoMisplacedStack(theToken);
          return result;
        }
      }
    }
  
   
    /* ---------------------------------------------------------------------------------
       This section of code is used to "move" misplaced content from one location in 
       our document model to another. (Consider what would happen if we found a <P> tag
       and text in the head.) To move content, we throw it onto the misplacedcontent 
       deque until we can deal with it.
       ---------------------------------------------------------------------------------
     */
    if(!execSkipContent) {

      switch(theTag) {
        case eHTMLTag_html:
        case eHTMLTag_noframes:
        case eHTMLTag_noscript:
        case eHTMLTag_script:
        case eHTMLTag_doctypeDecl:
        case eHTMLTag_instruction:
          break;
        case eHTMLTag_comment:
        case eHTMLTag_newline:
        case eHTMLTag_whitespace:
        case eHTMLTag_userdefined:
          if (mMisplacedContent.GetSize() == 0) {
            // simply pass these through to token handler without further ado...
            // fix for bugs 17017,18308,23765,24275,69331
            break;  
          }
        default:
          if(!gHTMLElements[eHTMLTag_html].SectionContains(theTag,PR_FALSE)) {
            if(!(mFlags & (NS_DTD_FLAG_HAD_BODY | NS_DTD_FLAG_HAD_FRAMESET))) {

              //For bug examples from this code, see bugs: 18928, 20989.

              //At this point we know the body/frameset aren't open. 
              //If the child belongs in the head, then handle it (which may open the head);
              //otherwise, push it onto the misplaced stack.

              PRBool theExclusive=PR_FALSE;
              PRBool theChildBelongsInHead=gHTMLElements[eHTMLTag_head].IsChildOfHead(theTag,theExclusive);
              if(!theChildBelongsInHead) {

                //If you're here then we found a child of the body that was out of place.
                //We're going to move it to the body by storing it temporarily on the misplaced stack.
                //However, in quirks mode, a few tags request, ambiguosly, for a BODY. - Bugs 18928, 24204.-
                PushIntoMisplacedStack(aToken);
                if(DoesRequireBody(aToken,mTokenizer)) {
                  CToken* theBodyToken=NS_STATIC_CAST(CToken*,mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_body,NS_LITERAL_STRING("body")));
                  result=HandleToken(theBodyToken,aParser);
                }
                return result;
              }
            } //if
          } //if
      }//switch

    } //if
    
    if(theToken){
      //Before dealing with the token normally, we need to deal with skip targets
     CStartToken* theStartToken=NS_STATIC_CAST(CStartToken*,aToken);
     if((!execSkipContent)                  && 
        (theType!=eToken_end)               &&
        (eHTMLTag_unknown==mSkipTarget)     && 
        (gHTMLElements[theTag].mSkipTarget) && 
        (!theStartToken->IsEmpty())) { // added empty token check for bug 44186
        //create a new target
        NS_ASSERTION(mSkippedContent.GetSize() == 0, "all the skipped content tokens did not get handled");
        mSkippedContent.Empty();
        mSkipTarget=gHTMLElements[theTag].mSkipTarget;
        mSkippedContent.Push(theToken);
      }
      else {

        mParser=(nsParser*)aParser;

        switch(theType) {
          case eToken_text:
          case eToken_start:
          case eToken_whitespace: 
          case eToken_newline:
            result=HandleStartToken(theToken); break;

          case eToken_end:
            result=HandleEndToken(theToken); break;
          
          case eToken_cdatasection:
          case eToken_comment:
          case eToken_markupDecl:
            result=HandleCommentToken(theToken); break;

          case eToken_entity:
            result=HandleEntityToken(theToken); break;

          case eToken_attribute:
            result=HandleAttributeToken(theToken); break;

          case eToken_instruction:
            result=HandleProcessingInstructionToken(theToken); break;

          case eToken_doctypeDecl:
            result=HandleDocTypeDeclToken(theToken); break;

          default:
            break;
        }//switch


        if(NS_SUCCEEDED(result) || (NS_ERROR_HTMLPARSER_BLOCK==result)) {
           IF_FREE(theToken, mTokenAllocator);
        }
        else if(result==NS_ERROR_HTMLPARSER_STOPPARSING) {
          mFlags |= NS_DTD_FLAG_STOP_PARSING;
        }
        else {
          return NS_OK;
        }
      }
    }

  }//if
  return result;
}
 
/**
 * This gets called after we've handled a given start tag.
 * It's a generic hook to let us to post processing.
 * @param   aToken contains the tag in question
 * @param   aChildTag is the tag itself.
 * @return  status
 */
nsresult CNavDTD::DidHandleStartTag(nsIParserNode& aNode,eHTMLTags aChildTag){
  nsresult result=NS_OK;

#if 0
    // XXX --- Ignore this: it's just rickg debug testing...
  nsAutoString theStr;
  aNode.GetSource(theStr);
#endif

  switch(aChildTag){

    case eHTMLTag_pre:
    case eHTMLTag_listing:
      {
        CToken* theNextToken=mTokenizer->PeekToken();
        if(theNextToken)  {
          eHTMLTokenTypes theType=eHTMLTokenTypes(theNextToken->GetTokenType());
          if(eToken_newline==theType){
            mLineNumber += theNextToken->GetNewlineCount();
            theNextToken=mTokenizer->PopToken();  //skip 1st newline inside PRE and LISTING
            IF_FREE(theNextToken, mTokenAllocator); // fix for Bug 29379
          }//if
        }//if
      }
      break;

    case eHTMLTag_plaintext:
    case eHTMLTag_xmp:
      //grab the skipped content and dump it out as text...
      {        
        STOP_TIMER()
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::DidHandleStartTag(), this=%p\n", this));
        nsAutoString theString;
        PRInt32 lineNo = 0;
        
        result = CollectSkippedContent(aChildTag, theString, lineNo);
        NS_ENSURE_SUCCESS(result, result);

        if(0<theString.Length()) {
          CTextToken *theToken=NS_STATIC_CAST(CTextToken*,mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,theString));
          nsCParserNode theNode(theToken, mTokenAllocator);
          result=mSink->AddLeaf(theNode); //when the node get's destructed, so does the new token
        }
        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::DidHandleStartTag(), this=%p\n", this));
        START_TIMER()
      }
      break;

#ifdef DEBUG
    case eHTMLTag_counter:
      {
        PRInt32   theCount=mBodyContext->GetCount();
        eHTMLTags theGrandParentTag=mBodyContext->TagAt(theCount-1);
        
        nsAutoString  theNumber;
        
        mBodyContext->IncrementCounter(theGrandParentTag,aNode,theNumber);

        CTextToken theToken(theNumber);
        nsCParserNode theNode(&theToken, 0 /*stack token*/);
        result=mSink->AddLeaf(theNode);
      }
      break;

    case eHTMLTag_meta:
      {
          //we should only enable user-defined entities in debug builds...

        PRInt32 theCount=aNode.GetAttributeCount();
        const nsAString* theNamePtr=0;
        const nsAString* theValuePtr=0;

        if(theCount) {
          PRInt32 theIndex=0;
          for(theIndex=0;theIndex<theCount;++theIndex){
            const nsAString& theKey = aNode.GetKeyAt(theIndex);
            if(theKey.Equals(NS_LITERAL_STRING("ENTITY"), nsCaseInsensitiveStringComparator())) {
              const nsAString& theName=aNode.GetValueAt(theIndex);
              theNamePtr=&theName;
            }
            else if(theKey.Equals(NS_LITERAL_STRING("VALUE"), nsCaseInsensitiveStringComparator())) {
              //store the named enity with the context...
              const nsAString& theValue=aNode.GetValueAt(theIndex);
              theValuePtr=&theValue;
            }
          }
        }
        if(theNamePtr && theValuePtr) {
          mBodyContext->RegisterEntity(*theNamePtr,*theValuePtr);
        }
      }
      break; 
#endif

    default:
      break;
  }//switch 

  //handle <empty/> tags by generating a close tag...
  //added this to fix bug 48351, which contains XHTML and uses empty tags.
  nsCParserNode* theNode=NS_STATIC_CAST(nsCParserNode*,&aNode);
  if(nsHTMLElement::IsContainer(aChildTag) && theNode && theNode->mToken) {  //nullptr test fixes bug 56085
    CStartToken *theToken=NS_STATIC_CAST(CStartToken*,theNode->mToken);
    if(theToken->IsEmpty()){

      CToken *theEndToken=mTokenAllocator->CreateTokenOfType(eToken_end,aChildTag); 
      if(theEndToken) {
        result=HandleEndToken(theEndToken);
        IF_FREE(theEndToken, mTokenAllocator);
      }
    }
  }

  return result;
} 
 
/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRInt32 CNavDTD::LastOf(eHTMLTags aTagSet[],PRInt32 aCount) const {
  int theIndex=0;
  for(theIndex=mBodyContext->GetCount()-1;theIndex>=0;theIndex--){
    if(FindTagInSet((*mBodyContext)[theIndex],aTagSet,aCount)) { 
      return theIndex;
    } 
  } 
  return kNotFound;
}

/**
 *  Call this to find the index of a given child, or (if not found)
 *  the index of its nearest synonym.
 *   
 *  @update  gess 3/25/98
 *  @param   aTagStack -- list of open tags
 *  @param   aTag -- tag to test for containership
 *  @return  index of kNotFound
 */
static
PRInt32 GetIndexOfChildOrSynonym(nsDTDContext& aContext,eHTMLTags aChildTag) {

#if 1
  PRInt32 theChildIndex=nsHTMLElement::GetIndexOfChildOrSynonym(aContext,aChildTag);
#else 
  PRInt32 theChildIndex=aContext.LastOf(aChildTag);
  if(kNotFound==theChildIndex) {
    TagList* theSynTags=gHTMLElements[aChildTag].GetSynonymousTags(); //get the list of tags that THIS tag can close
    if(theSynTags) {
      theChildIndex=LastOf(aContext,*theSynTags);
    } 
    else{
      PRInt32 theGroup=nsHTMLElement::GetSynonymousGroups(aChildTag);
      if(theGroup) {
        theChildIndex=aContext.GetCount();
        while(-1<--theChildIndex) {
          eHTMLTags theTag=aContext[theChildIndex];
          if(gHTMLElements[theTag].IsMemberOf(theGroup)) {
            break;   
          }
        }
      }
    }
  }
#endif
  return theChildIndex;
}

/** 
 *  This method is called to determine whether or not the child
 *  tag is happy being OPENED in the context of the current
 *  tag stack. This is only called if the current parent thinks
 *  it wants to contain the given childtag.
 *    
 *  @param   aChildTag -- tag enum of child to be opened
 *  @param   aTagStack -- ref to current tag stack in DTD.
 *  @return  PR_TRUE if child agrees to be opened here.
 */  
static
PRBool CanBeContained(eHTMLTags aChildTag,nsDTDContext& aContext) {

  /* #    Interesting test cases:       Result:
   * 1.   <UL><LI>..<B>..<LI>           inner <LI> closes outer <LI>
   * 2.   <CENTER><DL><DT><A><CENTER>   allow nested <CENTER>
   * 3.   <TABLE><TR><TD><TABLE>...     allow nested <TABLE>
   * 4.   <FRAMESET> ... <FRAMESET>
   */

  //Note: This method is going away. First we need to get the elementtable to do closures right, and
  //      therefore we must get residual style handling to work.

  //the changes to this method were added to fix bug 54651...

  PRBool  result=PR_TRUE;
  PRInt32 theCount=aContext.GetCount();

  if(0<theCount){
    const TagList* theRootTags=gHTMLElements[aChildTag].GetRootTags();
    const TagList* theSpecialParents=gHTMLElements[aChildTag].GetSpecialParents();
    if(theRootTags) {
      PRInt32 theRootIndex=LastOf(aContext,*theRootTags);
      PRInt32 theSPIndex=(theSpecialParents) ? LastOf(aContext,*theSpecialParents) : kNotFound;  
      PRInt32 theChildIndex=GetIndexOfChildOrSynonym(aContext,aChildTag);
      PRInt32 theTargetIndex=(theRootIndex>theSPIndex) ? theRootIndex : theSPIndex;

      if((theTargetIndex==theCount-1) ||
        ((theTargetIndex==theChildIndex) && gHTMLElements[aChildTag].CanContainSelf())) {
        result=PR_TRUE;
      }
      else {
        
        result=PR_FALSE;

        static eHTMLTags gTableElements[]={eHTMLTag_td,eHTMLTag_th};

        PRInt32 theIndex=theCount-1;
        while(theChildIndex<theIndex) {
          eHTMLTags theParentTag=aContext.TagAt(theIndex--);
          if (gHTMLElements[theParentTag].IsMemberOf(kBlockEntity)  || 
              gHTMLElements[theParentTag].IsMemberOf(kHeading)      || 
              gHTMLElements[theParentTag].IsMemberOf(kPreformatted) || 
              gHTMLElements[theParentTag].IsMemberOf(kFormControl)  ||  //fixes bug 44479
              gHTMLElements[theParentTag].IsMemberOf(kList)) {
            if(!HasOptionalEndTag(theParentTag)) {
              result=PR_TRUE;
              break;
            }
          }
          else if(FindTagInSet(theParentTag,gTableElements,sizeof(gTableElements)/sizeof(eHTMLTag_unknown))){
            result=PR_TRUE;  //added this to catch a case we missed; bug 57173.
            break;
          }
        }
      }
    }
  }

  return result;

}

enum eProcessRule {eNormalOp,eLetInlineContainBlock};

/** 
 *  This method gets called when a start token has been 
 *  encountered in the parse process. If the current container
 *  can contain this tag, then add it. Otherwise, you have
 *  two choices: 1) create an implicit container for this tag
 *                  to be stored in
 *               2) close the top container, and add this to
 *                  whatever container ends up on top.
 *   
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsCParserNode *aNode) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult  result=NS_OK;
  PRBool  theChildIsContainer=nsHTMLElement::IsContainer(aChildTag);

  // client of parser is spefically trying to parse a fragment that 
  // may lack required context.  Suspend containment rules if so.
  if (mParserCommand != eViewFragment)
  {
    PRBool  theChildAgrees=PR_TRUE;
    PRInt32 theIndex=mBodyContext->GetCount();
    PRBool  theParentContains=-1;
    
    do {
   
      eHTMLTags theParentTag=mBodyContext->TagAt(--theIndex);
      theParentContains=CanContain(theParentTag,aChildTag);  //precompute containment, and pass it to CanOmit()...

      if(CanOmit(theParentTag,aChildTag,theParentContains)) {
        result=HandleOmittedTag(aToken,aChildTag,theParentTag,aNode);
        return result;
      }

      eProcessRule theRule=eNormalOp; 

      if (!theParentContains &&
          (IsBlockElement(aChildTag,theParentTag) &&
           IsInlineElement(theParentTag,theParentTag))) { //broaden this to fix <inline><block></block></inline>
        if (eHTMLTag_li != aChildTag) {  //remove test for table to fix 57554
          nsCParserNode* theParentNode = NS_STATIC_CAST(nsCParserNode*, mBodyContext->PeekNode());
          if (theParentNode && theParentNode->mToken->IsWellFormed()) {
            theRule = eLetInlineContainBlock;
          }
        }
      }

      switch(theRule){

        case eNormalOp:        

          theChildAgrees=PR_TRUE;
          if(theParentContains) {

            eHTMLTags theAncestor=gHTMLElements[aChildTag].mRequiredAncestor;
            if(eHTMLTag_unknown!=theAncestor){
              theChildAgrees=HasOpenContainer(theAncestor);
            }
            
            if(theChildAgrees && theChildIsContainer) {
              if(theParentTag!=aChildTag) {             
                // Double check the power structure a
                // Note: The bit is currently set on <A> and <LI>.
                if(gHTMLElements[aChildTag].ShouldVerifyHierarchy()){
                  PRInt32 theChildIndex=GetIndexOfChildOrSynonym(*mBodyContext,aChildTag);
              
                  if((kNotFound<theChildIndex) && (theChildIndex<theIndex)) {
                   
                  /*-------------------------------------------------------------------------------------
                     1  Here's a tricky case from bug 22596:  <h5><li><h5>
                        How do we know that the 2nd <h5> should close the <LI> rather than nest inside the <LI>?
                        (Afterall, the <h5> is a legal child of the <LI>).
              
                        The way you know is that there is no root between the two, so the <h5> binds more
                        tightly to the 1st <h5> than to the <LI>.
                     2.  Also, bug 6148 shows this case: <SPAN><DIV><SPAN>
                        From this case we learned not to execute this logic if the parent is a block.
                    
                     3. Fix for 26583
                        Ex. <A href=foo.html><B>foo<A href-bar.html>bar</A></B></A>  <-- A legal HTML
                        In the above example clicking on "foo" or "bar" should link to
                        foo.html or bar.html respectively. That is, the inner <A> should be informed
                        about the presence of an open <A> above <B>..so that the inner <A> can close out
                        the outer <A>. The following code does it for us.
                     
                     4. Fix for 27865 [ similer to 22596 ]. Ex: <DL><DD><LI>one<DD><LI>two
                   -------------------------------------------------------------------------------------*/
                     
                    theChildAgrees=CanBeContained(aChildTag,*mBodyContext);
                  } //if
                }//if
              } //if
            } //if
          } //if parentcontains

          if(!(theParentContains && theChildAgrees)) {
            if (!CanPropagate(theParentTag,aChildTag,theParentContains)) { 
              if(theChildIsContainer || (!theParentContains)){ 
                if(!theChildAgrees && !gHTMLElements[aChildTag].CanAutoCloseTag(*mBodyContext,aChildTag)) {
                  // Closing the tags above might cause non-compatible results.
                  // Ex. <TABLE><TR><TD><TBODY>Text</TD></TR></TABLE>. 
                  // In the example above <TBODY> is badly misplaced, but 
                  // we should not attempt to close the tags above it, 
                  // The safest thing to do is to discard this tag.
                  return result;
                }
                else if (mBodyContext->mContextTopIndex > 0 && theIndex <= mBodyContext->mContextTopIndex) {
                  // Looks like the parent tag does not want to contain the current tag ( aChildTag ). 
                  // However, we have to force the containment, when handling misplaced content, to avoid data loss.
                  // Ref. bug 138577.
                  theParentContains = PR_TRUE;
                }
                else {
                  CloseContainersTo(theIndex,aChildTag,PR_TRUE);
                }
              }//if
              else break;
            }//if
            else {
              CreateContextStackFor(aChildTag);
              theIndex=mBodyContext->GetCount();
            }
          }//if
          break;

        case eLetInlineContainBlock:
          theParentContains=theChildAgrees=PR_TRUE; //cause us to fall out of loop and open the block.
          break;

        default:
          break;

      }//switch
    } while(!(theParentContains && theChildAgrees));
  }
  
  if(theChildIsContainer){
    result=OpenContainer(aNode,aChildTag,PR_TRUE);
  }
  else {  //we're writing a leaf...
    result=AddLeaf(aNode);
  }

  return result;
}
 
/**
 * This gets called before we've handled a given start tag.
 * It's a generic hook to let us do pre processing.
 * @param   aToken contains the tag in question
 * @param   aTag is the tag itself.
 * @param   aNode is the node (tag) with associated attributes.
 * @return  TRUE if tag processing should continue; FALSE if the tag has been handled.
 */
nsresult CNavDTD::WillHandleStartTag(CToken* aToken,eHTMLTags aTag,nsIParserNode& aNode) 
{ 
  nsresult result = NS_OK;

  //this little gem creates a special attribute for the editor team to use.
  //The attribute only get's applied to unknown tags, and is used by ender
  //(during editing) to display a special icon for unknown tags.
  if(eHTMLTag_userdefined == aTag) {
    CAttributeToken* theToken= NS_STATIC_CAST(CAttributeToken*,mTokenAllocator->CreateTokenOfType(eToken_attribute,aTag));
    if(theToken) {
      theToken->SetKey(NS_LITERAL_STRING("_moz-userdefined"));
      aNode.AddAttribute(theToken);    
    }
  }

    /**************************************************************************************
     *
     * Now a little code to deal with bug #49687 (crash when layout stack gets too deep)
     * I've also opened this up to any container (not just inlines): re bug 55095
     * Improved to handle bug 55980 (infinite loop caused when DEPTH is exceeded and
     * </P> is encountered by itself (<P>) is continuously produced.
     *
     **************************************************************************************/
  
  PRInt32 stackDepth = mBodyContext->GetCount();
  if (stackDepth > MAX_REFLOW_DEPTH) {
    if (nsHTMLElement::IsContainer(aTag) &&
        !gHTMLElements[aTag].HasSpecialProperty(kHandleStrayTag)) {
      // Ref. bug 98261,49678,55095,55980
      // Instead of throwing away the current tag close it's parent
      // such that the stack level does not go beyond the max_reflow_depth.
      // This would allow leaf tags, that follow the current tag, to find
      // the correct node. 
      while (stackDepth != MAX_REFLOW_DEPTH && NS_SUCCEEDED(result)) {
        result = CloseContainersTo(mBodyContext->Last(),PR_FALSE);
        --stackDepth;
      }
    }        
  }

  STOP_TIMER()
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::WillHandleStartTag(), this=%p\n", this));

  if (aTag <= NS_HTML_TAG_MAX) {
    result = mSink->NotifyTagObservers(&aNode);
  }

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::WillHandleStartTag(), this=%p\n", this));
  START_TIMER()

  if(NS_SUCCEEDED(result)) {

#ifdef ENABLE_CRC

    STOP_TIMER()

      if(eHTMLTag_meta==aTag) {  
      PRInt32 theCount=aNode.GetAttributeCount(); 
      if(1<theCount){ 
  
        const nsAString& theKey = aNode.GetKeyAt(0);
        if(theKey.Equals("NAME",IGNORE_CASE)) { 
          const nsString& theValue1=aNode.GetValueAt(0); 
          if(theValue1.Equals("\"CRC\"",IGNORE_CASE)) { 
            const nsAString& theKey2 = aNode.GetKeyAt(1); 
            if(theKey2.Equals("CONTENT",IGNORE_CASE)) { 
              const nsString& theValue2=aNode.GetValueAt(1); 
              PRInt32 err=0; 
              mExpectedCRC32=theValue2.ToInteger(&err); 
            } //if 
          } //if 
        } //else 

      } //if 
    }//if 

    START_TIMER()

#endif

    if(NS_OK==result) {
      result=gHTMLElements[aTag].HasSpecialProperty(kDiscardTag) ? 1 : NS_OK;
    }
    
    //this code is here to make sure the head is closed before we deal 
    //with any tags that don't belong in the head.
    if (NS_SUCCEEDED(result) && (mFlags & NS_DTD_FLAG_HAS_OPEN_HEAD && 
                                 eHTMLTag_newline != aTag &&
                                 eHTMLTag_whitespace != aTag &&
                                 eHTMLTag_userdefined != aTag)) {
      PRBool theExclusive = PR_FALSE;
      if (!gHTMLElements[eHTMLTag_head].IsChildOfHead(aTag, theExclusive)) {      
        result = CloseHead();
      }
    }
  }
  return result;
}

static void PushMisplacedAttributes(nsIParserNode& aNode,nsDeque& aDeque,PRInt32& aCount) {
  if(aCount > 0) {
    CToken* theAttrToken=nsnull;
    nsCParserNode* theAttrNode = (nsCParserNode*)&aNode;
    if(theAttrNode) {
      while(aCount){ 
        theAttrToken=theAttrNode->PopAttributeToken();
        if(theAttrToken) {
          theAttrToken->SetNewlineCount(0);
          aDeque.Push(theAttrToken);
        }
        aCount--;
      }//while
    }//if
  }//if
}

/** 
 *  This method gets called when a start token has been encountered that the parent
 *  wants to omit. 
 *   
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aChildTag -- id of the child in question
 *  @param   aParent -- id of the parent in question
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleOmittedTag(CToken* aToken,eHTMLTags aChildTag,eHTMLTags aParent,nsIParserNode* aNode) {
  NS_PRECONDITION(mBodyContext != nsnull,"need a context to work with");

  nsresult  result=NS_OK;

  //The trick here is to see if the parent can contain the child, but prefers not to.
  //Only if the parent CANNOT contain the child should we look to see if it's potentially a child
  //of another section. If it is, the cache it for later.
  //  1. Get the root node for the child. See if the ultimate node is the BODY, FRAMESET, HEAD or HTML
  PRInt32   theTagCount = mBodyContext->GetCount();

  if(aToken) {
    PRInt32   attrCount = aToken->GetAttributeCount();
    if((gHTMLElements[aParent].HasSpecialProperty(kBadContentWatch)) &&
       (!nsHTMLElement::IsWhitespaceTag(aChildTag))) {
      eHTMLTags theTag=eHTMLTag_unknown;
    
      // Determine the insertion point
      while(theTagCount > 0) {
        theTag = mBodyContext->TagAt(--theTagCount);
        if(!gHTMLElements[theTag].HasSpecialProperty(kBadContentWatch)) {
          mBodyContext->mContextTopIndex = theTagCount; // This is our insertion point
          break;
        }
      }

      if(mBodyContext->mContextTopIndex>-1) {
                  
        PushIntoMisplacedStack(aToken);  

        IF_HOLD(aToken);  // Hold on to this token for later use.

        // If the token is attributed then save those attributes too.    
        if(attrCount > 0) PushMisplacedAttributes(*aNode,mMisplacedContent,attrCount);

        if(gHTMLElements[aChildTag].mSkipTarget) {
          nsAutoString theString;
          PRInt32 lineNo = 0;
          
          result = CollectSkippedContent(aChildTag, theString, lineNo);
          NS_ENSURE_SUCCESS(result, result);

          PushIntoMisplacedStack(mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,theString));
          PushIntoMisplacedStack(mTokenAllocator->CreateTokenOfType(eToken_end,aChildTag));      
        }
              
        mFlags |= NS_DTD_FLAG_MISPLACED_CONTENT; // This state would help us in gathering all the misplaced elements
      }//if
    }//if

    if((aChildTag!=aParent) && (gHTMLElements[aParent].HasSpecialProperty(kSaveMisplaced))) {
      
      IF_HOLD(aToken);  // Hold on to this token for later use. Ref Bug. 53695
      
      PushIntoMisplacedStack(aToken);
      // If the token is attributed then save those attributes too.
       if(attrCount > 0) PushMisplacedAttributes(*aNode,mMisplacedContent,attrCount);
    }
  }
  return result;
}

/** 
 *  This method gets called when a kegen token is found.
 *   
 *  @update  harishd 05/02/00
 *  @param   aNode -- CParserNode representing keygen
 *  @return  NS_OK if all went well; ERROR if error occured
 */
nsresult CNavDTD::HandleKeyGen(nsIParserNode* aNode) { 
  nsresult result=NS_OK; 

  if(aNode) { 
  
    nsCOMPtr<nsIFormProcessor> theFormProcessor = 
             do_GetService(kFormProcessorCID, &result);
  
    if(NS_SUCCEEDED(result)) { 
      PRInt32      theAttrCount=aNode->GetAttributeCount(); 
      nsVoidArray  theContent; 
      nsAutoString theAttribute; 
      nsAutoString theFormType; 
      CToken*      theToken=nsnull; 

      theFormType.Assign(NS_LITERAL_STRING("select")); 
  
      result=theFormProcessor->ProvideContent(theFormType,theContent,theAttribute); 

      if(NS_SUCCEEDED(result)) {
        nsString* theTextValue=nsnull; 
        PRInt32   theIndex=nsnull; 
  
        if(mTokenizer && mTokenAllocator) {
          // Populate the tokenizer with the fabricated elements in the reverse order 
          // such that <SELECT> is on the top fo the tokenizer followed by <OPTION>s 
          // and </SELECT> 
          theToken=mTokenAllocator->CreateTokenOfType(eToken_end,eHTMLTag_select); 
          mTokenizer->PushTokenFront(theToken); 
  
          for(theIndex=theContent.Count()-1;theIndex>-1;theIndex--) { 
            theTextValue=(nsString*)theContent[theIndex]; 
            theToken=mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,*theTextValue); 
            mTokenizer->PushTokenFront(theToken); 
            theToken=mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_option); 
            mTokenizer->PushTokenFront(theToken); 
          } 
  
          // The attribute ( provided by the form processor ) should be a part of the SELECT.
          // Placing the attribute token on the tokenizer to get picked up by the SELECT.
          theToken=mTokenAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown,theAttribute);

          ((CAttributeToken*)theToken)->SetKey(NS_LITERAL_STRING("_moz-type")); 
          mTokenizer->PushTokenFront(theToken); 
  
          // Pop out NAME and CHALLENGE attributes ( from the keygen NODE ) 
          // and place it in the tokenizer such that the attribtues get 
          // sucked into SELECT node. 
          for(theIndex=theAttrCount;theIndex>0;theIndex--) { 
            mTokenizer->PushTokenFront(((nsCParserNode*)aNode)->PopAttributeToken()); 
          } 
  
          theToken=mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_select); 
          // Increament the count because of the additional attribute from the form processor.
          theToken->SetAttributeCount(theAttrCount+1); 
          mTokenizer->PushTokenFront(theToken); 
        }//if(mTokenizer && mTokenAllocator)
      }//if(NS_SUCCEEDED(result))
    }// if(NS_SUCCEEDED(result)) 
  } //if(aNode) 
  return result; 
} 


/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. If the current container
 *  can contain this tag, then add it. Otherwise, you have
 *  two choices: 1) create an implicit container for this tag
 *                  to be stored in
 *               2) close the top container, and add this to
 *                  whatever container ends up on top.
 *  
 *  @update  gess 1/04/99
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */      
nsresult CNavDTD::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  //Begin by gathering up attributes...

  nsCParserNode* theNode=mNodeAllocator.CreateNode(aToken, mTokenAllocator);
  
  eHTMLTags     theChildTag=(eHTMLTags)aToken->GetTypeID();
  PRInt16       attrCount=aToken->GetAttributeCount();
  eHTMLTags     theParent=mBodyContext->Last();
  nsresult      result=(0==attrCount) ? NS_OK : CollectAttributes(*theNode,theChildTag,attrCount);

  if(NS_OK==result) {
    result=WillHandleStartTag(aToken,theChildTag,*theNode);
    if(NS_OK==result) {
      PRBool isTokenHandled =PR_FALSE;
      PRBool theHeadIsParent=PR_FALSE;

      if(nsHTMLElement::IsSectionTag(theChildTag)){
        switch(theChildTag){
          case eHTMLTag_html:
            if(mBodyContext->GetCount()>0) {
              result=OpenContainer(theNode,theChildTag,PR_FALSE);
              isTokenHandled=PR_TRUE;
            }
            break;
          case eHTMLTag_body:
            if(mFlags & NS_DTD_FLAG_HAS_OPEN_BODY) {
              result=OpenContainer(theNode,theChildTag,PR_FALSE);
              isTokenHandled=PR_TRUE;
            }
            break;
          case eHTMLTag_head:
            if(mFlags & (NS_DTD_FLAG_HAD_BODY | NS_DTD_FLAG_HAD_FRAMESET)) {
              result=HandleOmittedTag(aToken,theChildTag,theParent,theNode);
              isTokenHandled=PR_TRUE;
            }
            break;
          default:
            break;
        }
      }

      PRBool theExclusive=PR_FALSE;
      theHeadIsParent=nsHTMLElement::IsChildOfHead(theChildTag,theExclusive);
      
      switch(theChildTag) { 
        case eHTMLTag_area:
          if(!mOpenMapCount) isTokenHandled=PR_TRUE;

          STOP_TIMER();
          MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleStartToken(), this=%p\n", this));
          
          if (mOpenMapCount>0 && mSink) {
            result=mSink->AddLeaf(*theNode);
            isTokenHandled=PR_TRUE;
          }
          
          MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleStartToken(), this=%p\n", this));
          START_TIMER();
          break;

        case eHTMLTag_image:
          aToken->SetTypeID(theChildTag=eHTMLTag_img);
          break;

        case eHTMLTag_keygen:
          result=HandleKeyGen(theNode);
          isTokenHandled=PR_TRUE;
          break;

        case eHTMLTag_script:
          theHeadIsParent = !(mFlags & NS_DTD_FLAG_HAS_OPEN_BODY);
          mFlags |= NS_DTD_FLAG_HAS_OPEN_SCRIPT;

        default:
            break;
      }//switch

      if(!isTokenHandled) {
        if(theHeadIsParent || ((mFlags & NS_DTD_FLAG_HAS_OPEN_HEAD)  && 
                               (eHTMLTag_newline == theChildTag    || 
                                eHTMLTag_whitespace == theChildTag ||
                                eHTMLTag_userdefined == theChildTag))) {
            result = AddHeadLeaf(theNode);
        }
        else 
          result = HandleDefaultStartToken(aToken,theChildTag,theNode); 
      }

      //now do any post processing necessary on the tag...
      if(NS_OK==result)
        DidHandleStartTag(*theNode,theChildTag);
    }//if
  } //if

  if(kHierarchyTooDeep==result) {
    //reset this error to ok; all that happens here is that given inline tag
    //gets dropped because the stack is too deep. Don't terminate parsing.
    result=NS_OK;
  }

  IF_FREE(theNode, &mNodeAllocator);
  return result;
}

/**
 *  Call this to see if you have a closeable peer on the stack that
 *  is ABOVE one of its root tags.
 *   
 *  @update  gess 4/11/99
 *  @param   aRootTagList -- list of root tags for aTag
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
static
PRBool HasCloseablePeerAboveRoot(const TagList& aRootTagList,nsDTDContext& aContext,eHTMLTags aTag,PRBool anEndTag) {
  PRInt32   theRootIndex=LastOf(aContext,aRootTagList);  
  const TagList*  theCloseTags=(anEndTag) ? gHTMLElements[aTag].GetAutoCloseEndTags() : gHTMLElements[aTag].GetAutoCloseStartTags();
  PRInt32 theChildIndex=-1;

  if(theCloseTags) {
    theChildIndex=LastOf(aContext,*theCloseTags);
  }
  else {
    if((anEndTag) || (!gHTMLElements[aTag].CanContainSelf()))
      theChildIndex=aContext.LastOf(aTag);
  }
    // I changed this to theRootIndex<=theChildIndex so to handle this case:
    //  <SELECT><OPTGROUP>...</OPTGROUP>
    //
  return PRBool(theRootIndex<=theChildIndex);  
}


/**
 *  This method is called to determine whether or not an END tag
 *  can be autoclosed. This means that based on the current
 *  context, the stack should be closed to the nearest matching
 *  tag.
 *     
 *  @param   aTag -- tag enum of child to be tested
 *  @return  PR_TRUE if autoclosure should occur
 */ 
static
eHTMLTags FindAutoCloseTargetForEndTag(eHTMLTags aCurrentTag,nsDTDContext& aContext,nsDTDMode aMode) {
  int theTopIndex=aContext.GetCount();
  eHTMLTags thePrevTag=aContext.Last();
 
  if(nsHTMLElement::IsContainer(aCurrentTag)){
    PRInt32 theChildIndex=GetIndexOfChildOrSynonym(aContext,aCurrentTag);
    
    if(kNotFound<theChildIndex) {
      if(thePrevTag==aContext[theChildIndex]){
        return aContext[theChildIndex];
      } 
    
      if(nsHTMLElement::IsBlockCloser(aCurrentTag)) {

          /*here's what to do: 
              Our here is sitting at aChildIndex. There are other tags above it
              on the stack. We have to try to close them out, but we may encounter
              one that can block us. The way to tell is by comparing each tag on
              the stack against our closeTag and rootTag list. 

              For each tag above our hero on the stack, ask 3 questions:
                1. Is it in the closeTag list? If so, the we can skip over it
                2. Is it in the rootTag list? If so, then we're gated by it
                3. Otherwise its non-specified and we simply presume we can close it.
          */
 
        const TagList* theCloseTags=gHTMLElements[aCurrentTag].GetAutoCloseEndTags();
        const TagList* theRootTags=gHTMLElements[aCurrentTag].GetEndRootTags();
      
        if(theCloseTags){  
            //at a min., this code is needed for H1..H6
        
          while(theChildIndex<--theTopIndex) {
            eHTMLTags theNextTag=aContext[theTopIndex];
            if(PR_FALSE==FindTagInSet(theNextTag,theCloseTags->mTags,theCloseTags->mCount)) {
              if(PR_TRUE==FindTagInSet(theNextTag,theRootTags->mTags,theRootTags->mCount)) {
                return eHTMLTag_unknown; //we encountered a tag in root list so fail (because we're gated).
              }
              //otherwise presume it's something we can simply ignore and continue search...
            }
            //otherwise its in the close list so skip to next tag...
          } 
          eHTMLTags theTarget=aContext.TagAt(theChildIndex);
          if(aCurrentTag!=theTarget) {
            aCurrentTag=theTarget; //use the synonym.
          }
          return aCurrentTag; //if you make it here, we're ungated and found a target!
        }//if
        else if(theRootTags) {
          //since we didn't find any close tags, see if there is an instance of aCurrentTag
          //above the stack from the roottag.
          if(HasCloseablePeerAboveRoot(*theRootTags,aContext,aCurrentTag,PR_TRUE))
            return aCurrentTag;
          else return eHTMLTag_unknown;
        }
      } //if blockcloser
      else{
        //Ok, a much more sensible approach for non-block closers; use the tag group to determine closure:
        //For example: %phrasal closes %phrasal, %fontstyle and %special
        return gHTMLElements[aCurrentTag].GetCloseTargetForEndTag(aContext,theChildIndex,aMode);
      }
    }//if
  } //if
  return eHTMLTag_unknown;
}

/**
 * 
 * @param   
 * @param   
 * @update  gess 10/11/99
 * @return  nada
 */
static void StripWSFollowingTag(eHTMLTags aChildTag,nsITokenizer* aTokenizer,nsTokenAllocator* aTokenAllocator, PRInt32& aNewlineCount){ 
  CToken* theToken= (aTokenizer)? aTokenizer->PeekToken():nsnull; 

  if(aTokenAllocator) { 
    while(theToken) { 
      eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType()); 
      switch(theType) { 
        case eToken_newline: ++aNewlineCount; 
        case eToken_whitespace: 
          theToken=aTokenizer->PopToken();
          IF_FREE(theToken, aTokenAllocator);
          theToken=aTokenizer->PeekToken(); 
          break; 
        default: 
          theToken=0; 
          break; 
      } 
    } 
  } 
}

/**
 *  This method gets called when an end token has been 
 *  encountered in the parse process. If the end tag matches
 *  the start tag on the stack, then simply close it. Otherwise,
 *  we have a erroneous state condition. This can be because we
 *  have a close tag with no prior open tag (user error) or because
 *  we screwed something up in the parse process. I'm not sure
 *  yet how to tell the difference.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult    result=NS_OK;
  eHTMLTags   theChildTag=(eHTMLTags)aToken->GetTypeID();

  switch(theChildTag) {

    case eHTMLTag_script:
      mFlags &= ~NS_DTD_FLAG_HAS_OPEN_SCRIPT;
    case eHTMLTag_style:
    case eHTMLTag_link:
    case eHTMLTag_meta:
    case eHTMLTag_textarea:
    case eHTMLTag_title:
      break;

    case eHTMLTag_head:
      StripWSFollowingTag(theChildTag,mTokenizer, mTokenAllocator, mLineNumber);
      result = CloseContainer(eHTMLTag_head, theChildTag, PR_FALSE);
      break;

    case eHTMLTag_form:
      result = CloseContainer(eHTMLTag_form, theChildTag, PR_FALSE);
      break;

    case eHTMLTag_br:
      {
          //This is special NAV-QUIRKS code that allows users
          //to use </BR>, even though that isn't a legitimate tag.
        if(eDTDMode_quirks==mDTDMode) {
          // Use recycler and pass the token thro' HandleToken() to fix bugs like 32782.
          CHTMLToken* theToken = NS_STATIC_CAST(CHTMLToken*,mTokenAllocator->CreateTokenOfType(eToken_start,theChildTag));
          result=HandleToken(theToken,mParser);
        }
      }
      break;

    case eHTMLTag_body:
    case eHTMLTag_html:
      StripWSFollowingTag(theChildTag,mTokenizer,mTokenAllocator,mLineNumber);
      break;

    default:
     {
        //now check to see if this token should be omitted, or 
        //if it's gated from closing by the presence of another tag.
        if(gHTMLElements[theChildTag].CanOmitEndTag()) {
          PopStyle(theChildTag);
        }
        else {
          eHTMLTags theParentTag=mBodyContext->Last();

          if(kNotFound==GetIndexOfChildOrSynonym(*mBodyContext,theChildTag)) {

            // Ref: bug 30487
            // Make sure that we don't cross boundaries, of certain elements,
            // to close stylistic information.
            // Ex. <font face="helvetica"><table><tr><td></font></td></tr></table> some text...
            // In the above ex. the orphaned FONT tag, inside TD, should cross TD boundaryto 
            // close the FONT tag above TABLE. 
            static eHTMLTags gBarriers[]={eHTMLTag_thead,eHTMLTag_tbody,eHTMLTag_tfoot,eHTMLTag_table};

            if(!FindTagInSet(theParentTag,gBarriers,sizeof(gBarriers)/sizeof(theParentTag))) {
              if(nsHTMLElement::IsResidualStyleTag(theChildTag)) {
                mBodyContext->RemoveStyle(theChildTag); // fix bug 77746
              }
            }

            // If the bit kHandleStrayTag is set then we automatically open up a matching
            // start tag ( compatibility ).  Currently this bit is set on P tag.
            // This also fixes Bug: 22623
            if(gHTMLElements[theChildTag].HasSpecialProperty(kHandleStrayTag) &&
               mDTDMode != eDTDMode_full_standards &&
               mDTDMode != eDTDMode_almost_standards) {
              // Oh boy!! we found a "stray" tag. Nav4.x and IE introduce line break in
              // such cases. So, let's simulate that effect for compatibility.
              // Ex. <html><body>Hello</P>There</body></html>
              PRBool theParentContains=-1; //set to -1 to force canomit to recompute.
              if(!CanOmit(theParentTag,theChildTag,theParentContains)) {
                IF_HOLD(aToken);
                mTokenizer->PushTokenFront(aToken); //put this end token back...
                CHTMLToken* theToken = NS_STATIC_CAST(CHTMLToken*,mTokenAllocator->CreateTokenOfType(eToken_start,theChildTag));
                mTokenizer->PushTokenFront(theToken); //put this new token onto stack...
              }
            }
            return result;
          }
          if(result==NS_OK) {
            eHTMLTags theTarget=FindAutoCloseTargetForEndTag(theChildTag,*mBodyContext,mDTDMode);
            if(eHTMLTag_unknown!=theTarget) {
              if (nsHTMLElement::IsResidualStyleTag(theChildTag)) {
                result=OpenTransientStyles(theChildTag); 
                if(NS_FAILED(result)) {
                  return result;
                }
              }
              result=CloseContainersTo(theTarget,PR_FALSE);
            }
          }
        }
      }
      break;
  }

  return result;
}

/**
 * This method will be triggered when the end of a table is
 * encountered.  Its primary purpose is to process all the
 * bad-contents pertaining a particular table. The position
 * of the table is the token bank ID.
 *
 * @update harishd 03/24/99
 * @param  aTag - This ought to be a table tag
 *
 */
nsresult CNavDTD::HandleSavedTokens(PRInt32 anIndex) {
    NS_PRECONDITION(mBodyContext != nsnull && mBodyContext->GetCount() > 0,"invalid context");

    nsresult  result      = NS_OK;

    if(anIndex>kNotFound) {
      PRInt32  theBadTokenCount   = mMisplacedContent.GetSize();

      if(theBadTokenCount > 0) {
        
        if(mTempContext==nsnull) mTempContext=new nsDTDContext();

        CToken*   theToken;
        eHTMLTags theTag;
        PRInt32   attrCount;
        PRInt32   theTopIndex = anIndex + 1;
        PRInt32   theTagCount = mBodyContext->GetCount();
               
        if (mSink && mSink->IsFormOnStack()) {
          // Do this to synchronize dtd stack and the sink stack.
          // Note: FORM is never on the dtd stack because its always 
          // considered as a leaf. However, in the sink FORM can either
          // be a container or a leaf. Therefore, we have to check
          // with the sink -- Ref: Bug 20087.
          ++anIndex;
        }

        STOP_TIMER()
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleSavedTokensAbove(), this=%p\n", this));     
        // Pause the main context and switch to the new context.
        mSink->BeginContext(anIndex);
        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleSavedTokensAbove(), this=%p\n", this));
        START_TIMER()
      
        // The body context should contain contents only upto the marked position.  
        mBodyContext->MoveEntries(*mTempContext, theTagCount - theTopIndex);
     
        // Now flush out all the bad contents.
        while(theBadTokenCount-- > 0){
          theToken=(CToken*)mMisplacedContent.PopFront();
          if(theToken) {
            theTag       = (eHTMLTags)theToken->GetTypeID();
            attrCount    = (gHTMLElements[theTag].mSkipTarget)? 0:theToken->GetAttributeCount();
            // Put back attributes, which once got popped out, into the tokenizer
            for(PRInt32 j=0;j<attrCount; ++j){
              CToken* theAttrToken = (CToken*)mMisplacedContent.PopFront();
              if(theAttrToken) {
                mTokenizer->PushTokenFront(theAttrToken);
              }
              theBadTokenCount--;
            }
            
            if(eToken_end==theToken->GetTokenType()) {
              // Ref: Bug 25202
              // Make sure that the BeginContext() is ended only by the call to
              // EndContext(). Ex: <center><table><a></center>.
              // In the Ex. above </center> should not close <center> above table.
              // Doing so will cause the current context to get closed prematurely. 
              PRInt32 theIndex=mBodyContext->LastOf(theTag);
              
              if(theIndex!=kNotFound && theIndex<=mBodyContext->mContextTopIndex) {
                IF_FREE(theToken, mTokenAllocator);
                continue;
              }
            }
            result=HandleToken(theToken,mParser);
          }
        }//while
        if(theTopIndex != mBodyContext->GetCount()) {
           CloseContainersTo(theTopIndex,mBodyContext->TagAt(theTopIndex),PR_TRUE);
        }
     
        // Bad-contents were successfully processed. Now, itz time to get
        // back to the original body context state.
        mTempContext->MoveEntries(*mBodyContext, theTagCount - theTopIndex);

        STOP_TIMER()
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleSavedTokensAbove(), this=%p\n", this));     
        // Terminate the new context and switch back to the main context
        mSink->EndContext(anIndex);
        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleSavedTokensAbove(), this=%p\n", this));
        START_TIMER()
      }
    }
    return result;
}


/**
 *  This method gets called when an entity token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleEntityToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult  result=NS_OK;

  const nsAString& theStr = aToken->GetStringValue();

  if((kHashsign!=theStr.First()) && (-1==nsHTMLEntities::EntityToUnicode(theStr))){
    CToken *theToken=0;
#ifdef DEBUG
    //before we just toss this away as a bogus entity, let's check...
    CNamedEntity *theEntity=mBodyContext->GetEntity(theStr);
    if(theEntity) {
      theToken = NS_STATIC_CAST(CTextToken*,mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,theEntity->mValue));
    }
    else {
#endif
      //if you're here we have a bogus entity.
      //convert it into a text token.
      nsAutoString entityName;
      entityName.Assign(NS_LITERAL_STRING("&"));
      entityName.Append(theStr); //should append the entity name; fix bug 51161.
      theToken = mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,entityName);
#ifdef DEBUG
    }
#endif
    return HandleToken(theToken,mParser); //theToken should get recycled automagically...
  }

  eHTMLTags theParentTag=mBodyContext->Last();

  nsCParserNode* theNode=mNodeAllocator.CreateNode(aToken, mTokenAllocator);
  if(theNode) {
    PRBool theParentContains=-1; //set to -1 to force CanOmit to recompute...
    if(CanOmit(theParentTag,eHTMLTag_entity,theParentContains)) {
      eHTMLTags theCurrTag=(eHTMLTags)aToken->GetTypeID();
      result=HandleOmittedTag(aToken,theCurrTag,theParentTag,theNode);
    }
    else {
      result=AddLeaf(theNode);
    }
    IF_FREE(theNode, &mNodeAllocator);
  }  
  return result;
}

/**
 *  This method gets called when a comment token has been 
 *  encountered in the parse process. After making sure
 *  we're somewhere in the body, we handle the comment
 *  in the same code that we use for text.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleCommentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  
  nsresult  result=NS_OK;

  nsCParserNode* theNode=mNodeAllocator.CreateNode(aToken, mTokenAllocator);

  if(theNode) {

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleCommentToken(), this=%p\n", this));

    result=(mSink) ? mSink->AddComment(*theNode) : NS_OK;  

    IF_FREE(theNode, &mNodeAllocator);

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleCommentToken(), this=%p\n", this));
    START_TIMER();
  }

  return result;
}


/**
 *  This method gets called when an attribute token has been 
 *  encountered in the parse process. This is an error, since
 *  all attributes should have been accounted for in the prior
 *  start or end tokens
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleAttributeToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  NS_ERROR("attribute encountered -- this shouldn't happen unless the attribute was not part of a start tag!");

  return NS_OK;
}

/**
 *  This method gets called when a script token has been 
 *  encountered in the parse process. n
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleScriptToken(const nsIParserNode *aNode) {
  // PRInt32 attrCount=aNode.GetAttributeCount(PR_TRUE);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleScriptToken(), this=%p\n", this));

  nsresult result=AddLeaf(aNode);

  mParser->SetCanInterrupt(PR_FALSE);

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleScriptToken(), this=%p\n", this));
  START_TIMER();

  return result;
}


/**
 *  This method gets called when an "instruction" token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleProcessingInstructionToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult  result=NS_OK;

  nsCParserNode* theNode=mNodeAllocator.CreateNode(aToken, mTokenAllocator);
  if(theNode) {

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleProcessingInstructionToken(), this=%p\n", this));

    result=(mSink) ? mSink->AddProcessingInstruction(*theNode) : NS_OK; 

    IF_FREE(theNode, &mNodeAllocator);

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleProcessingInstructionToken(), this=%p\n", this));
    START_TIMER();

  }
  return result;
}

/**
 *  This method gets called when a DOCTYPE token has been 
 *  encountered in the parse process. 
 *  
 *  @update  harishd 09/02/99
 *  @param   aToken -- The very first token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleDocTypeDeclToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult result=NS_OK;

  CDoctypeDeclToken* theToken = NS_STATIC_CAST(CDoctypeDeclToken*,aToken);
  nsAutoString docTypeStr(theToken->GetStringValue());
  mLineNumber += docTypeStr.CountChar(kNewLine);
  
  PRInt32 len=docTypeStr.Length();
  PRInt32 pos=docTypeStr.RFindChar(kGreaterThan);
  if(pos>-1) {
    docTypeStr.Cut(pos,len-pos);// First remove '>' from the end.
  }
  docTypeStr.Cut(0,2); // Now remove "<!" from the begining
  theToken->SetStringValue(docTypeStr);

  nsCParserNode* theNode=mNodeAllocator.CreateNode(aToken, mTokenAllocator);
  if(theNode) {

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleDocTypeDeclToken(), this=%p\n", this));
  
  result = (mSink)? mSink->AddDocTypeDecl(*theNode):NS_OK;
    
  IF_FREE(theNode, &mNodeAllocator);
  
  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleDocTypeDeclToken(), this=%p\n", this));
  START_TIMER();

  }
  return result;
}

/**
 * Retrieve the attributes for this node, and add then into
 * the node.
 *
 * @update  gess4/22/98
 * @param   aNode is the node you want to collect attributes for
 * @param   aCount is the # of attributes you're expecting
 * @return error code (should be 0)
 */
nsresult CNavDTD::CollectAttributes(nsIParserNode& aNode,eHTMLTags aTag,PRInt32 aCount){
  int attr=0;

  nsresult result=NS_OK;
  int theAvailTokenCount=mTokenizer->GetCount() + mSkippedContent.GetSize();
  if(aCount<=theAvailTokenCount) {
    CToken* theToken=0;
    eHTMLTags theSkipTarget=gHTMLElements[aTag].mSkipTarget;
    for(attr=0;attr<aCount;++attr){
      if((eHTMLTag_unknown!=theSkipTarget) && mSkippedContent.GetSize())
        theToken=NS_STATIC_CAST(CToken*,mSkippedContent.PopFront());
      else 
        theToken=mTokenizer->PopToken();
      if(theToken)  {
        eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
        if(theType!=eToken_attribute) {
          // If you're here then it means that the token does not
          // belong to this node. Put the token back into the tokenizer
          // and let it go thro' the regular path. Bug: 59189.
          mTokenizer->PushTokenFront(theToken);
          break;
        }
        // Sanitize the key for it might contain some non-alpha-non-digit characters
        // at its end.  Ex. <OPTION SELECTED/> - This will be tokenized as "<" "OPTION",
        // "SELECTED/", and ">". In this case the "SELECTED/" key will be sanitized to
        // a legitimate "SELECTED" key.
        ((CAttributeToken*)theToken)->SanitizeKey();
        mLineNumber += theToken->GetNewlineCount();

        aNode.AddAttribute(theToken);
      }
    }
  }
  else {
    result=kEOF;
  }
  return result;
}


/**
 * Causes the next skipped-content token (if any) to
 * be consumed by this node.
 *
 * @update  gess 4Sep2000
 * @param   node to consume skipped-content
 * @param   holds the number of skipped content elements encountered
 * @return  Error condition.
 */
NS_IMETHODIMP
CNavDTD::CollectSkippedContent(PRInt32 aTag, nsAString& aContent, PRInt32 &aLineNo) {

  NS_ASSERTION(aTag >= eHTMLTag_unknown && aTag <= NS_HTML_TAG_MAX, "tag array out of bounds");

  aContent.Truncate();

  NS_ASSERTION(eHTMLTag_unknown != gHTMLElements[aTag].mSkipTarget, "cannot collect content for this tag");
  if (eHTMLTag_unknown == gHTMLElements[aTag].mSkipTarget) {
    // This tag doesn't support skipped content.
    aLineNo = -1;
    return NS_OK;
  }
  
  aLineNo = mLineNumber;
  mScratch.Truncate();
  PRInt32 i = 0;
  PRInt32 tagCount = mSkippedContent.GetSize();
  for (i = 0; i< tagCount; ++i){
    CHTMLToken* theNextToken = (CHTMLToken*)mSkippedContent.PopFront();
      
    if (theNextToken) {
      eHTMLTokenTypes theTokenType = (eHTMLTokenTypes)theNextToken->GetTokenType();

      // Dont worry about attributes here because it's already stored in 
      // the start token as mTrailing content and will get appended in 
      // start token's GetSource();
      if (eToken_attribute!=theTokenType) {
        if ((eToken_entity==theTokenType) &&
           ((eHTMLTag_textarea == aTag) || (eHTMLTag_title == aTag))) {
            mScratch.Truncate();
            ((CEntityToken*)theNextToken)->TranslateToUnicodeStr(mScratch);
            if (!mScratch.IsEmpty()){
              aContent.Append(mScratch);
            }
            else {
              // We thought it was an entity but it is not! - bug 79492
              aContent.Append(PRUnichar('&'));
              aContent.Append(theNextToken->GetStringValue());
            }
          }
        else theNextToken->AppendSourceTo(aContent);
      }
    }
    IF_FREE(theNextToken, mTokenAllocator);
  }
  
  InPlaceConvertLineEndings(aContent);

  // Note: TEXTAREA content is PCDATA and hence the newlines are already accounted for.
  mLineNumber += (aTag != eHTMLTag_textarea) ? aContent.CountChar(kNewLine) : 0;
  
  return NS_OK;
}

 /***********************************************************************************
   The preceeding tables determine the set of elements each tag can contain...
  ***********************************************************************************/
     
/** 
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CNavDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const 
{
  PRBool result=gHTMLElements[aParent].CanContain((eHTMLTags)aChild,mDTDMode);

#ifdef ALLOW_TR_AS_CHILD_OF_TABLE
  if(!result) {
      //XXX This vile hack is here to support bug 30378, which allows
      //table to contain tr directly in an html32 document.
    if((eHTMLTag_tr==aChild) && (eHTMLTag_table==aParent)) {
      result=PR_TRUE;
    }
  }
#endif

  if(eHTMLTag_nobr==aChild) {
    if(IsInlineElement(aParent,aParent)){
      if(HasOpenContainer((eHTMLTags)aChild)) {
        return PR_FALSE;
      }
    }
  }

  return result;
} 

/**
 * Give rest of world access to our tag enums, so that CanContain(), etc,
 * become useful.
 */
NS_IMETHODIMP CNavDTD::StringTagToIntTag(const nsAString &aTag,
                                         PRInt32* aIntTag) const
{
  *aIntTag = nsHTMLTags::LookupTag(aTag);

  return NS_OK;
}

NS_IMETHODIMP_(const PRUnichar *)
CNavDTD::IntTagToStringTag(PRInt32 aIntTag) const
{
  const PRUnichar *str_ptr = nsHTMLTags::GetStringValue((nsHTMLTag)aIntTag);

  NS_ASSERTION(str_ptr, "Bad tag enum passed to CNavDTD::IntTagToStringTag()"
               "!!");

  return str_ptr;
}

NS_IMETHODIMP_(nsIAtom *)
CNavDTD::IntTagToAtom(PRInt32 aIntTag) const
{
  nsIAtom *atom = nsHTMLTags::GetAtom((nsHTMLTag)aIntTag);

  NS_ASSERTION(atom, "Bad tag enum passed to CNavDTD::IntTagToAtom()"
               "!!");

  return atom;
}

/**
 *  This method is called to determine whether or not
 *  the given childtag is a block element.
 *
 *  @update  gess 6June2000
 *  @param   aChildID -- tag id of child 
 *  @param   aParentID -- tag id of parent (or eHTMLTag_unknown)
 *  @return  PR_TRUE if this tag is a block tag
 */
PRBool CNavDTD::IsBlockElement(PRInt32 aTagID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;
  eHTMLTags theTag=(eHTMLTags)aTagID;

  if((theTag>eHTMLTag_unknown) && (theTag<eHTMLTag_userdefined)) {
    result=((gHTMLElements[theTag].IsMemberOf(kBlock))       || 
            (gHTMLElements[theTag].IsMemberOf(kBlockEntity)) || 
            (gHTMLElements[theTag].IsMemberOf(kHeading))     || 
            (gHTMLElements[theTag].IsMemberOf(kPreformatted))|| 
            (gHTMLElements[theTag].IsMemberOf(kList))); 
  }

  return result;
}

/**
 *  This method is called to determine whether or not
 *  the given childtag is an inline element.
 *
 *  @update  gess 6June2000
 *  @param   aChildID -- tag id of child 
 *  @param   aParentID -- tag id of parent (or eHTMLTag_unknown)
 *  @return  PR_TRUE if this tag is an inline tag
 */
PRBool CNavDTD::IsInlineElement(PRInt32 aTagID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;
  eHTMLTags theTag=(eHTMLTags)aTagID;

  if((theTag>eHTMLTag_unknown) && (theTag<eHTMLTag_userdefined)) {
    result=((gHTMLElements[theTag].IsMemberOf(kInlineEntity))|| 
            (gHTMLElements[theTag].IsMemberOf(kFontStyle))   || 
            (gHTMLElements[theTag].IsMemberOf(kPhrase))      || 
            (gHTMLElements[theTag].IsMemberOf(kSpecial))     || 
            (gHTMLElements[theTag].IsMemberOf(kFormControl)));
  }

  return result;
}

/**
 *  This method is called to determine whether or not
 *  the necessary intermediate tags should be propagated
 *  between the given parent and given child.
 *
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if propagation should occur
 */
PRBool CNavDTD::CanPropagate(eHTMLTags aParent,eHTMLTags aChild,PRBool aParentContains)  {
  PRBool    result=PR_FALSE;
  PRBool    theParentContains=(-1==aParentContains) ? CanContain(aParent,aChild) : aParentContains;

  if(aParent==aChild) {
    return result;
  }

  if(nsHTMLElement::IsContainer(aChild)){
    mScratch.Truncate();
    if(!gHTMLElements[aChild].HasSpecialProperty(kNoPropagate)){
      if(nsHTMLElement::IsBlockParent(aParent) || (gHTMLElements[aParent].GetSpecialChildren())) {

        result=ForwardPropagate(mScratch,aParent,aChild);

        if(PR_FALSE==result){

          if(eHTMLTag_unknown!=aParent) {
            if(aParent!=aChild) //dont even bother if we're already inside a similar element...
              result=BackwardPropagate(mScratch,aParent,aChild);
          } //if
          else result=BackwardPropagate(mScratch,eHTMLTag_html,aChild);

        } //elseif

      }//if
    }//if
    if(mScratch.Length()-1>gHTMLElements[aParent].mPropagateRange)
      result=PR_FALSE;
  }//if
  else result=theParentContains;


  return result;
}


/**
 *  This method gets called to determine whether a given 
 *  tag can be omitted from opening. Most cannot.
 *  
 *  @update  gess 3/25/98
 *  @param   aParent
 *  @param   aChild
 *  @param   aParentContains
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::CanOmit(eHTMLTags aParent,eHTMLTags aChild,PRBool& aParentContains)  {

  eHTMLTags theAncestor=gHTMLElements[aChild].mExcludingAncestor;
  if (eHTMLTag_unknown!=theAncestor){
    if (HasOpenContainer(theAncestor)) {
      return PR_TRUE;
    }
  }

  theAncestor=gHTMLElements[aChild].mRequiredAncestor;
  if(eHTMLTag_unknown!=theAncestor){
    if(!HasOpenContainer(theAncestor)) {
      if(!CanPropagate(aParent,aChild,aParentContains)) {
        return PR_TRUE;
      }
    }
    return PR_FALSE;
  }


  if(gHTMLElements[aParent].CanExclude(aChild)){
    return PR_TRUE;
  }

    //Now the obvious test: if the parent can contain the child, don't omit.
  if(-1==aParentContains)
    aParentContains=CanContain(aParent,aChild);

  if(aParentContains || (aChild==aParent)){
    return PR_FALSE;
  }

  if(gHTMLElements[aParent].IsBlockEntity()) {
    if(nsHTMLElement::IsInlineEntity(aChild)) {  //feel free to drop inlines that a block doesn't contain.
      return PR_TRUE;
    }
  }

  if(gHTMLElements[aParent].HasSpecialProperty(kBadContentWatch)) {

    if(-1==aParentContains) {    
      //we need to compute parent containment here, since it wasn't given...
      if(!gHTMLElements[aParent].CanContain(aChild,mDTDMode)){
        return PR_TRUE;
      }
    }
    else if (!aParentContains) {
      if(!gHTMLElements[aChild].HasSpecialProperty(kBadContentWatch)) {
        return PR_TRUE; 
      }
      return PR_FALSE; // Ref. Bug 25658
    }
  }

  if(gHTMLElements[aParent].HasSpecialProperty(kSaveMisplaced)) {
    return PR_TRUE;
  }

  return PR_FALSE;
}
     

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 4/8/98
 *  @param   aTag -- tag to test as a container
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::IsContainer(PRInt32 aTag) const {
  return nsHTMLElement::IsContainer((eHTMLTags)aTag);
}


/**
 * This method tries to design a context vector (without actually
 * changing our parser state) from the parent down to the
 * child. 
 * 
 * @update  gess4/6/98
 * @param   aVector is the string where we store our output vector
 *          in bottom-up order.
 * @param   aParent -- tag type of parent
 * @param   aChild -- tag type of child
 * @return  TRUE if propagation closes; false otherwise
 */
PRBool CNavDTD::ForwardPropagate(nsString& aSequence,eHTMLTags aParent,eHTMLTags aChild)  {
  PRBool result=PR_FALSE;

  switch(aParent) {
    case eHTMLTag_table:
      {
        if((eHTMLTag_tr==aChild) || (eHTMLTag_td==aChild)) {
          return BackwardPropagate(aSequence,aParent,aChild);
        }
      }
      //otherwise, intentionally fall through...

    case eHTMLTag_tr:
      {  
        PRBool theCanContainResult=CanContain(eHTMLTag_td,aChild);
        if(PR_TRUE==theCanContainResult) {
          aSequence.Append((PRUnichar)eHTMLTag_td);
          result=BackwardPropagate(aSequence,aParent,eHTMLTag_td);
        }
      }
      break;

    case eHTMLTag_th:
      break;

    default:
      break;
  }//switch
  return result;
}


/**
 * This method tries to design a context map (without actually
 * changing our parser state) from the child up to the parent.
 *
 * @update  gess4/6/98
 * @param   aVector is the string where we store our output vector
 *          in bottom-up order.
 * @param   aParent -- tag type of parent
 * @param   aChild -- tag type of child
 * @return  TRUE if propagation closes; false otherwise
 */
PRBool CNavDTD::BackwardPropagate(nsString& aSequence,eHTMLTags aParent,eHTMLTags aChild) const {

  eHTMLTags theParent=aParent; //just init to get past first condition...

  do {
    const TagList* theRootTags=gHTMLElements[aChild].GetRootTags();
    if(theRootTags) {
      theParent=theRootTags->mTags[0];
      if(CanContain(theParent,aChild)) {
        //we've found a complete sequence, so push the parent...
        aChild=theParent;
        aSequence.Append((PRUnichar)theParent);
      }
    }
    else break;
  } 
  while((theParent!=eHTMLTag_unknown) && (theParent!=aParent));

  return PRBool(aParent==theParent);
}


/**
 *  This method allows the caller to determine if a given container
 *  is currently open
 *  
 *  @update  gess 11/9/98
 *  @param   
 *  @return  
 */
PRBool CNavDTD::HasOpenContainer(eHTMLTags aContainer) const {
  PRBool result=PR_FALSE;

  switch(aContainer) {
    case eHTMLTag_form:
      result= !(~mFlags & NS_DTD_FLAG_HAS_OPEN_FORM); break;
    case eHTMLTag_map: 
      result=mOpenMapCount>0; break; 
    default:
      result=mBodyContext->HasOpenContainer(aContainer);
      break;
  }
  return result;
}

/**
 *  This method allows the caller to determine if a any member
 *  in a set of tags is currently open
 *  
 *  @update  gess 11/9/98
 *  @param   
 *  @return  
 */
PRBool CNavDTD::HasOpenContainer(const eHTMLTags aTagSet[],PRInt32 aCount) const {

  int theIndex; 
  int theTopIndex=mBodyContext->GetCount()-1;

  for(theIndex=theTopIndex;theIndex>0;theIndex--){
    if(FindTagInSet((*mBodyContext)[theIndex],aTagSet,aCount))
      return PR_TRUE;
  }
  return PR_FALSE;
}

/**
 *  This method retrieves the HTMLTag type of the topmost
 *  container on the stack.
 *  
 *  @update  gess 4/2/98
 *  @return  tag id of topmost node in contextstack
 */
eHTMLTags CNavDTD::GetTopNode() const {
  return mBodyContext->Last();
}

/*********************************************
  Here comes code that handles the interface
  to our content sink.
 *********************************************/
 

/**
 * It is with great trepidation that I offer this method (privately of course).
 * The gets called whenever a container gets opened. This methods job is to 
 * take a look at the (transient) style stack, and open any style containers that
 * are there. Of course, we shouldn't bother to open styles that are incompatible
 * with our parent container.
 *
 * @update  gess6/4/98
 * @param   tag of the container just opened
 * @return  0 (for now)
 */
nsresult CNavDTD::OpenTransientStyles(eHTMLTags aChildTag){
  nsresult result=NS_OK;

  // No need to open transient styles in head context - Fix for 41427
  if((mFlags & NS_DTD_FLAG_ENABLE_RESIDUAL_STYLE) && 
     eHTMLTag_newline!=aChildTag && 
     !(mFlags & NS_DTD_FLAG_HAS_OPEN_HEAD)) {

#ifdef  ENABLE_RESIDUALSTYLE

    if(CanContain(eHTMLTag_font,aChildTag)) {

      PRUint32 theCount=mBodyContext->GetCount();
      PRUint32 theLevel=theCount;

        //this first loop is used to determine how far up the containment
        //hierarchy we go looking for residual styles.
      while ( 1<theLevel) {
        eHTMLTags theParentTag = mBodyContext->TagAt(--theLevel);
        if(gHTMLElements[theParentTag].HasSpecialProperty(kNoStyleLeaksIn)) {
          break;
        }
      }

      mFlags &= ~NS_DTD_FLAG_ENABLE_RESIDUAL_STYLE;
      for(;theLevel<theCount;++theLevel){
        nsEntryStack* theStack=mBodyContext->GetStylesAt(theLevel);
        if(theStack){

          PRInt32 sindex=0;

          nsTagEntry *theEntry=theStack->mEntries;
          for(sindex=0;sindex<theStack->mCount;++sindex){            
            nsCParserNode* theNode=(nsCParserNode*)theEntry->mNode;
            if(1==theNode->mUseCount) {
              eHTMLTags theNodeTag=(eHTMLTags)theNode->GetNodeType();
              if(gHTMLElements[theNodeTag].CanContain(aChildTag,mDTDMode)) {
                theEntry->mParent = theStack;  //we do this too, because this entry differs from the new one we're pushing...
                if(gHTMLElements[mBodyContext->Last()].IsMemberOf(kHeading)) {
                  // Bug 77352
                  // The style system needs to identify residual style tags
                  // within heading tags so that heading tags' size can take
                  // precedence over the residual style tags' size info.. 
                  // *Note: Make sure that this attribute is transient since it
                  // should not get carried over to cases other than heading.
                  CAttributeToken theAttrToken(NS_LITERAL_STRING("_moz-rs-heading"), EmptyString());
                  theNode->AddAttribute(&theAttrToken);
                  result = OpenContainer(theNode,theNodeTag,PR_FALSE,theStack);
                  theNode->PopAttributeToken();
                }
                else { 
                  result = OpenContainer(theNode,theNodeTag,PR_FALSE,theStack);
                }
              }
              else {
                //if the node tag can't contain the child tag, then remove the child tag from the style stack
                nsCParserNode* node=theStack->Remove(sindex,theNodeTag);
                IF_FREE(node, &mNodeAllocator);
                --theEntry; //back up by one
              }
            } //if
            ++theEntry;
          } //for
        } //if
      } //for
      mFlags |= NS_DTD_FLAG_ENABLE_RESIDUAL_STYLE;

    } //if

#endif
  }//if
  return result;
}

/**
 * It is with great trepidation that I offer this method (privately of course).
 * The gets called just prior when a container gets opened. This methods job is to 
 * take a look at the (transient) style stack, and <i>close</i> any style containers 
 * that are there. Of course, we shouldn't bother to open styles that are incompatible
 * with our parent container.
 * SEE THE TOP OF THIS FILE for more information about how the transient style stack works.
 *
 * @update  gess6/4/98
 * @param   tag of the container just opened
 * @return  0 (for now)
 */
nsresult CNavDTD::CloseTransientStyles(eHTMLTags aChildTag){
  return NS_OK;
}

/**
 * This method gets called when an explicit style close-tag is encountered.
 * It results in the style tag id being popped from our internal style stack.
 *
 * @update  gess6/4/98
 * @param 
 * @return  0 if all went well (which it always does)
 */
nsresult CNavDTD::PopStyle(eHTMLTags aTag){
  nsresult result=0;

  if(mFlags & NS_DTD_FLAG_ENABLE_RESIDUAL_STYLE) {
#ifdef  ENABLE_RESIDUALSTYLE
    if(nsHTMLElement::IsResidualStyleTag(aTag)) {
      nsCParserNode* node=mBodyContext->PopStyle(aTag);
      IF_FREE(node, &mNodeAllocator);  
    }
#endif
  } //if
  return result;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * 
 * @update  gess4/22/98
 * @param   aNode -- next node to be added to model
 */
nsresult CNavDTD::OpenHTML(const nsCParserNode *aNode){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenHTML(), this=%p\n", this));

  nsresult result = (mSink) ? mSink->OpenHTML(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenHTML(), this=%p\n", this));
  START_TIMER();

  // Don't push more than one HTML tag into the stack...
  if (mBodyContext->GetCount() == 0) 
    mBodyContext->Push(NS_CONST_CAST(nsCParserNode*, aNode), 0, PR_FALSE); 

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 *
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::CloseHTML(){

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseHTML(), this=%p\n", this));

  nsresult result = (mSink) ? mSink->CloseHTML() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseHTML(), this=%p\n", this));
  START_TIMER();

  return result;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::OpenHead(const nsIParserNode *aNode)
{
  nsresult result = NS_OK;

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenHead(), this=%p\n", this));

  if (!(mFlags & NS_DTD_FLAG_HAS_OPEN_HEAD)) {
    mFlags |= NS_DTD_FLAG_HAS_OPEN_HEAD;
    result = mSink ? mSink->OpenHead(*aNode) : NS_OK;
  }

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenHead(), this=%p\n", this));
  START_TIMER();

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::CloseHead()
{
  nsresult result = NS_OK;

  if (mFlags & NS_DTD_FLAG_HAS_OPEN_HEAD) {
    mFlags &= ~NS_DTD_FLAG_HAS_OPEN_HEAD;

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseHead(), this=%p\n", this));

    result = mSink ? mSink->CloseHead() : NS_OK;

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseHead(), this=%p\n", this));
    START_TIMER();
  }

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::OpenBody(const nsCParserNode *aNode)
{
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  nsresult result = NS_OK;
  
  if (!(mFlags & NS_DTD_FLAG_HAD_FRAMESET)) {

    mFlags |= NS_DTD_FLAG_HAD_BODY;

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenBody(), this=%p\n", this));

    result = (mSink) ? mSink->OpenBody(*aNode) : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenBody(), this=%p\n", this));
    START_TIMER();

    if (!HasOpenContainer(eHTMLTag_body)) {
      mBodyContext->Push(NS_CONST_CAST(nsCParserNode*, aNode), 0, PR_FALSE);
      mTokenizer->PrependTokens(mMisplacedContent);
    }
  }

  return result;
}

/**
 * This method does two things: 1st, help close
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::CloseBody()
{
  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseBody(), this=%p\n", this));

  nsresult result= (mSink) ? mSink->CloseBody() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseBody(), this=%p\n", this));
  START_TIMER();

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::OpenForm(const nsIParserNode *aNode)
{
  nsresult result = NS_OK;
  if (!(mFlags & NS_DTD_FLAG_HAS_OPEN_FORM)) { // discard nested forms - bug 72639
    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenForm(), this=%p\n", this));

    result = (mSink) ? mSink->OpenForm(*aNode) : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenForm(), this=%p\n", this));
    START_TIMER();
    if (NS_OK == result) {
      mFlags |= NS_DTD_FLAG_HAS_OPEN_FORM;
    }
  }

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::CloseForm()
{
  nsresult result = NS_OK;
  if (mFlags & NS_DTD_FLAG_HAS_OPEN_FORM) {
    mFlags &= ~NS_DTD_FLAG_HAS_OPEN_FORM;
    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseForm(), this=%p\n", this));

    result = (mSink) ? mSink->CloseForm() : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseForm(), this=%p\n", this));
    START_TIMER();
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::OpenMap(const nsCParserNode *aNode)
{
  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenMap(), this=%p\n", this));

  nsresult result = (mSink) ? mSink->OpenMap(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenMap(), this=%p\n", this));
  START_TIMER();

  if (NS_OK == result) {
    mBodyContext->Push(NS_CONST_CAST(nsCParserNode*, aNode), 0, PR_FALSE);
    ++mOpenMapCount;
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::CloseMap()
{
  nsresult result = NS_OK;
  if (mOpenMapCount) {
    mOpenMapCount--;
    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseMap(), this=%p\n", this));

    result = (mSink) ? mSink->CloseMap() : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseMap(), this=%p\n", this));
    START_TIMER();
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::OpenFrameset(const nsCParserNode *aNode)
{
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  mFlags |= NS_DTD_FLAG_HAD_FRAMESET;
  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenFrameset(), this=%p\n", this));

  nsresult result =( mSink) ? mSink->OpenFrameset(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenFrameset(), this=%p\n", this));
  START_TIMER();
  mBodyContext->Push(NS_CONST_CAST(nsCParserNode*, aNode), 0, PR_FALSE);

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::CloseFrameset()
{
  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseFrameset(), this=%p\n", this));

  nsresult result = (mSink) ? mSink->CloseFrameset() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseFrameset(), this=%p\n", this));
  START_TIMER();

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @param   aClosedByStartTag -- ONLY TRUE if the container is being closed by opening of another container.
 * @return  TRUE if ok, FALSE if error
 */
nsresult
CNavDTD::OpenContainer(const nsCParserNode *aNode,
                       eHTMLTags aTag,
                       PRBool aClosedByStartTag,
                       nsEntryStack* aStyleStack)
{
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);
  
  nsresult   result = NS_OK; 
  PRBool     done   = PR_TRUE;
  PRBool     rs_tag = nsHTMLElement::IsResidualStyleTag(aTag);

  if (rs_tag) {
    /***********************************************************************
     *  Here's an interesting problem:
     *
     *  If there's an <a> on the RS-stack, and you're trying to open 
     *  another <a>, the one on the RS-stack should be discarded.
     *
     *  I'm updating OpenTransientStyles to throw old <a>'s away.
     *
     ***********************************************************************/

    OpenTransientStyles(aTag); 
  }

#ifdef ENABLE_CRC
  #define K_OPENOP 100
  CRCStruct theStruct(aTag,K_OPENOP);
  mComputedCRC32=AccumulateCRC(mComputedCRC32,(char*)&theStruct,sizeof(theStruct));
#endif

  switch (aTag) {
    case eHTMLTag_html:
      result=OpenHTML(aNode); break;

    case eHTMLTag_head:
      result=OpenHead(aNode); 
      break;

    case eHTMLTag_body:
      {
        eHTMLTags theParent=mBodyContext->Last();
        if (!gHTMLElements[aTag].IsSpecialParent(theParent)) {
          mFlags |= NS_DTD_FLAG_HAS_OPEN_BODY;
          result = OpenBody(aNode); 
        }
        else {
          done = PR_FALSE;
        }
      }
      break;

    case eHTMLTag_counter: //drop it on the floor.
      break;

    case eHTMLTag_style:
    case eHTMLTag_title:
      break;

    case eHTMLTag_textarea:
      result = AddLeaf(aNode);
      break;

    case eHTMLTag_map:
      result = OpenMap(aNode);
      break;

    case eHTMLTag_form:
      result = OpenForm(aNode); 
      break;

    case eHTMLTag_frameset:
      result = OpenFrameset(aNode); 
      break;

    case eHTMLTag_script:
      result = HandleScriptToken(aNode);
      break;
    
    case eHTMLTag_noscript:
      // we want to make sure that OpenContainer gets called below since we're
      // not doing it here
      done=PR_FALSE;
      // If the script is disabled noscript should not be
      // in the content model until the layout can somehow
      // turn noscript's display property to block <-- bug 67899
      if(mFlags & NS_DTD_FLAG_SCRIPT_ENABLED) {
        mScratch.Truncate();
        mFlags |= NS_DTD_FLAG_ALTERNATE_CONTENT;
      }
      break;
       
    case eHTMLTag_iframe: // Bug 84491 
    case eHTMLTag_noframes:
      done=PR_FALSE;
      if(mFlags & NS_DTD_FLAG_FRAMES_ENABLED) {
        mScratch.Truncate();
        mFlags |= NS_DTD_FLAG_ALTERNATE_CONTENT;
      }
      break;

    default:
      done=PR_FALSE;
      break;
  }

  if (!done) {
    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenContainer(), this=%p\n", this));

    result=(mSink) ? mSink->OpenContainer(*aNode) : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenContainer(), this=%p\n", this));
    START_TIMER();
    // For residual style tags rs_tag will be true and hence
    // the body context will hold an extra reference to the node.
    mBodyContext->Push(NS_CONST_CAST(nsCParserNode*, aNode), aStyleStack, rs_tag); 
  }

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @param   aTag  -- id of tag to be closed
 * @param   aClosedByStartTag -- ONLY TRUE if the container is being closed by opening of another container.
 * @return  TRUE if ok, FALSE if error
 */
nsresult
CNavDTD::CloseContainer(const eHTMLTags aTag, eHTMLTags aTarget,PRBool aClosedByStartTag)
{
  nsresult   result = NS_OK;
#ifdef ENABLE_CRC
  #define K_CLOSEOP 200
  CRCStruct theStruct(nodeType,K_CLOSEOP);
  mComputedCRC32=AccumulateCRC(mComputedCRC32,(char*)&theStruct,sizeof(theStruct));
#endif

  switch (aTag) {

    case eHTMLTag_html:
      result=CloseHTML(); break;

    case eHTMLTag_style:
    case eHTMLTag_textarea:
      break;

    case eHTMLTag_head:
      result=CloseHead(); 
      break;

    case eHTMLTag_body:
      result=CloseBody(); 
      break;

    case eHTMLTag_map:
      result=CloseMap();
      break;

    case eHTMLTag_form:
      result=CloseForm(); 
      break;

    case eHTMLTag_frameset:
      result=CloseFrameset(); 
      break;
    
    case eHTMLTag_iframe:
    case eHTMLTag_noscript:
    case eHTMLTag_noframes:
      // switch from alternate content state to regular state
      mFlags &= ~NS_DTD_FLAG_ALTERNATE_CONTENT;
      // falling thro' intentionally....
    case eHTMLTag_title:
    default:
      STOP_TIMER();
      MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseContainer(), this=%p\n", this));

      result=(mSink) ? mSink->CloseContainer(aTag) : NS_OK; 

      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseContainer(), this=%p\n", this));
      START_TIMER();
      break;
  }

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   anIndex
 * @param   aTag
 * @param   aClosedByStartTag -- if TRUE, then we're closing something because a start tag caused it
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::CloseContainersTo(PRInt32 anIndex,eHTMLTags aTarget, PRBool aClosedByStartTag)
{
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result = NS_OK;
  
  if ((anIndex<mBodyContext->GetCount()) && (anIndex>=0)) {

    PRInt32 count = 0;
    while ((count = mBodyContext->GetCount()) > anIndex) {
      nsEntryStack* theChildStyleStack = 0;      
      eHTMLTags theTag = mBodyContext->Last();
      nsCParserNode* theNode = mBodyContext->Pop(theChildStyleStack);
      result = CloseContainer(theTag, aTarget,aClosedByStartTag);  

#ifdef  ENABLE_RESIDUALSTYLE

        PRBool theTagIsStyle=nsHTMLElement::IsResidualStyleTag(theTag);
        // If the current tag cannot leak out then we shouldn't leak out of the target - Fix 40713
        PRBool theStyleDoesntLeakOut = gHTMLElements[theTag].HasSpecialProperty(kNoStyleLeaksOut);
        if(!theStyleDoesntLeakOut) {
          theStyleDoesntLeakOut = gHTMLElements[aTarget].HasSpecialProperty(kNoStyleLeaksOut);
        }
      
        /*************************************************************
          Do not invoke residual style handling when dealing with
          alternate content. This fixes bug 25214.
         *************************************************************/

        if(theTagIsStyle && !(mFlags & NS_DTD_FLAG_ALTERNATE_CONTENT)) {
          NS_ASSERTION(theNode, "residual style node should not be null");
          if (!theNode) {
            if (theChildStyleStack)
              mBodyContext->PushStyles(theChildStyleStack);
            return NS_OK;
          }
          PRBool theTargetTagIsStyle = nsHTMLElement::IsResidualStyleTag(aTarget);
          if(aClosedByStartTag) { 

            /***********************************************************
              Handle closure due to new start tag.          

              The cases we're handing here:
                1. <body><b><DIV>       //<b> gets pushed onto <body>.mStyles.
                2. <body><a>text<a>     //in this case, the target matches, so don't push style
             ***************************************************************************/

            if (theNode->mUseCount == 0){
              if (theTag != aTarget) {
                  //don't push if thechild==theTarget
                if (theChildStyleStack)
                  theChildStyleStack->PushFront(theNode);
                else 
                  mBodyContext->PushStyle(theNode);
              }
            } 
            else if (theTag == aTarget && !gHTMLElements[aTarget].CanContainSelf()) {
              //here's a case we missed:  <a><div>text<a>text</a></div>
              //The <div> pushes the 1st <a> onto the rs-stack, then the 2nd <a>
              //pops the 1st <a> from the rs-stack altogether.
              nsCParserNode* node = mBodyContext->PopStyle(theTag);
              IF_FREE(node, &mNodeAllocator);
            }

            if (theChildStyleStack) {
              mBodyContext->PushStyles(theChildStyleStack);
            }
          }
          else { //Handle closure due to another close tag.      

            /***********************************************************             
              if you're here, then we're dealing with the closure of tags
              caused by a close tag (as opposed to an open tag).
              At a minimum, we should consider pushing residual styles up 
              up the stack or popping and recycling displaced nodes.

              Known cases: 
                1. <body><b><div>text</DIV> 
                      Here the <b> will leak into <div> (see case given above), and 
                      when <div> closes the <b> is dropped since it's already residual.

                2. <body><div><b>text</div>
                      Here the <b> will leak out of the <div> and get pushed onto
                      the RS stack for the <body>, since it originated in the <div>.

                3. <body><span><b>text</span>
                      In this case, the the <b> get's pushed onto the style stack.
                      Later we deal with RS styles stored on the <span>

                4. <body><span><b>text</i>
                      Here we the <b>is closed by a (synonymous) style tag. 
                      In this case, the <b> is simply closed.
             ***************************************************************************/

            if (theChildStyleStack) {
              if (!theStyleDoesntLeakOut) {
                if (theTag != aTarget) {
                  if (theNode->mUseCount == 0) {
                    theChildStyleStack->PushFront(theNode);
                  }
                }
                else if (theNode->mUseCount == 1) {
                  // This fixes bug 30885,29626.
                  // Make sure that the node, which is about to
                  // get released does not stay on the style stack...
                  // Also be sure to remove the correct style off the
                  // style stack. -  Ref. bug 94208.
                  // Ex <FONT><B><I></FONT><FONT></B></I></FONT>
                  // Make sure that </B> removes B off the style stack.
                  mBodyContext->RemoveStyle(theTag);
                }
                mBodyContext->PushStyles(theChildStyleStack);
              }
              else{
                IF_DELETE(theChildStyleStack,&mNodeAllocator);
              }
            }
            else if (theNode->mUseCount == 0) {

              //The old version of this only pushed if the targettag wasn't style.
              //But that misses this case: <font><b>text</font>, where the b should leak
              if (aTarget != theTag) {
                mBodyContext->PushStyle(theNode);
              }
            } 
            else {
              //Ah, at last, the final case. If you're here, then we just popped a 
              //style tag that got onto that tag stack from a stylestack somewhere.
              //Pop it from the stylestack if the target is also a style tag.
              //Make sure to remove the matching style. In the following example
              //<FONT><B><I></FONT><FONT color=red></B></I></FONT> make sure that 
              //</I> does not remove <FONT color=red> off the style stack. - bug 94208
              if (theTargetTagIsStyle && theTag == aTarget) {
                mBodyContext->RemoveStyle(theTag);
              }
            }
          }
        } //if
        else { 
          //the tag is not a style tag...
          if (theChildStyleStack) {
            if (theStyleDoesntLeakOut)
              IF_DELETE(theChildStyleStack,&mNodeAllocator);
            else 
              mBodyContext->PushStyles(theChildStyleStack);
          }
        }
#endif
        IF_FREE(theNode, &mNodeAllocator);
    }

  } //if
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aTag --
 * @param   aClosedByStartTag -- ONLY TRUE if the container is being closed by opening of another container.
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::CloseContainersTo(eHTMLTags aTag,PRBool aClosedByStartTag){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);

  PRInt32 pos=mBodyContext->LastOf(aTag);

  if(kNotFound!=pos) {
    //the tag is indeed open, so close it.
    return CloseContainersTo(pos,aTag,aClosedByStartTag);
  }

  eHTMLTags theTopTag=mBodyContext->Last();

  PRBool theTagIsSynonymous=((nsHTMLElement::IsResidualStyleTag(aTag)) && (nsHTMLElement::IsResidualStyleTag(theTopTag)));
  if(!theTagIsSynonymous){
    theTagIsSynonymous=(gHTMLElements[aTag].IsMemberOf(kHeading) && 
                        gHTMLElements[theTopTag].IsMemberOf(kHeading));  
  }

  if(theTagIsSynonymous) {
    //if you're here, it's because we're trying to close one tag,
    //but a different (synonymous) one is actually open. Because this is NAV4x
    //compatibililty mode, we must close the one that's really open.
    aTag=theTopTag;    
    pos=mBodyContext->LastOf(aTag);
    if(kNotFound!=pos) {
      //the tag is indeed open, so close it.
      return CloseContainersTo(pos,aTag,aClosedByStartTag);
    }
  }
  
  nsresult result=NS_OK;
  const TagList* theRootTags=gHTMLElements[aTag].GetRootTags();
  eHTMLTags theParentTag=(theRootTags) ? theRootTags->mTags[0] : eHTMLTag_unknown;
  pos=mBodyContext->LastOf(theParentTag);
  if(kNotFound!=pos) {
    //the parent container is open, so close it instead
    result=CloseContainersTo(pos+1,aTag,aClosedByStartTag);
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  error code; 0 means OK
 */
nsresult CNavDTD::AddLeaf(const nsIParserNode *aNode){
  nsresult result=NS_OK;
  
  if(mSink){
    eHTMLTags theTag=(eHTMLTags)aNode->GetNodeType();
    OpenTransientStyles(theTag); 

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::AddLeaf(), this=%p\n", this));
    
    result=mSink->AddLeaf(*aNode);

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::AddLeaf(), this=%p\n", this));
    START_TIMER();

  }
  return result;
}

/**
 * Call this method ONLY when you want to write a leaf
 * into the head container.
 * 
 * @update  gess 03/14/99
 * @param   aNode -- next node to be added to model
 * @return  error code; 0 means OK
 */
nsresult CNavDTD::AddHeadLeaf(nsIParserNode *aNode){
  nsresult result=NS_OK;

  static eHTMLTags gNoXTags[] = {eHTMLTag_noembed,eHTMLTag_noframes};

  eHTMLTags theTag = (eHTMLTags)aNode->GetNodeType();
  
  // XXX - SCRIPT inside NOTAGS should not get executed unless the pref.
  // says so.  Since we don't have this support yet..lets ignore the
  // SCRIPT inside NOTAGS.  Ref Bug 25880.
  if (eHTMLTag_meta == theTag || eHTMLTag_script == theTag) {
    if (HasOpenContainer(gNoXTags,sizeof(gNoXTags)/sizeof(eHTMLTag_unknown))) {
      return result;
    }
  }
  
  if (mSink) {
    if (eHTMLTag_title == theTag) {
      nsAutoString title;
      PRInt32 lineNo;
      result = CollectSkippedContent(theTag, title, lineNo);
      NS_ENSURE_SUCCESS(result, result);
      
      STOP_TIMER();
      MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::AddHeadLeaf(), this=%p\n", this));

      result = mSink->SetTitle(title);

      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::AddHeadLeaf(), this=%p\n", this));
      START_TIMER();
    }
    else {
      STOP_TIMER();
      MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::AddHeadLeaf(), this=%p\n", this));
      
      result = mSink->AddHeadContent(*aNode);
           
      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::AddHeadLeaf(), this=%p\n", this));
      START_TIMER();
    }
  }
  return result;
}

/**
 *  This method gets called to create a valid context stack
 *  for the given child. We compare the current stack to the
 *  default needs of the child, and push new guys onto the
 *  stack until the child can be properly placed.
 *
 *  @update  gess 4/8/98
 *  @param   aChild is the child for whom we need to 
 *           create a new context vector
 *  @return  true if we succeeded, otherwise false
 */
nsresult CNavDTD::CreateContextStackFor(eHTMLTags aChild){
  
  mScratch.Truncate();
  
  nsresult  result=(nsresult)kContextMismatch;
  eHTMLTags theTop=mBodyContext->Last();
  PRBool    bResult=ForwardPropagate(mScratch,theTop,aChild);
  
  if(PR_FALSE==bResult){

    if(eHTMLTag_unknown!=theTop) {
      if(theTop!=aChild) //dont even bother if we're already inside a similar element...
        bResult=BackwardPropagate(mScratch,theTop,aChild);      
    } //if
    else bResult=BackwardPropagate(mScratch,eHTMLTag_html,aChild);
  } //elseif

  PRInt32   theLen=mScratch.Length();
  eHTMLTags theTag=(eHTMLTags)mScratch[--theLen];

  if((0==mBodyContext->GetCount()) || (mBodyContext->Last()==theTag))
    result=NS_OK;

  //now, build up the stack according to the tags 
  //you have that aren't in the stack...
  if(PR_TRUE==bResult){
    while(theLen) {
      theTag=(eHTMLTags)mScratch[--theLen];

#ifdef ALLOW_TR_AS_CHILD_OF_TABLE
      if((eHTML3_Quirks==mDocType) && (eHTMLTag_tbody==theTag)) {
        //the prev. condition prevents us from emitting tbody in html3.2 docs; fix bug 30378
        continue;
      }
#endif
      CStartToken *theToken=(CStartToken*)mTokenAllocator->CreateTokenOfType(eToken_start,theTag);
      HandleToken(theToken,mParser);  //these should all wind up on contextstack, so don't recycle.
    }
    result=NS_OK;
  }
  return result;
}

/**
 * 
 * @update  gess5/18/98
 * @param 
 * @return
 */
nsresult CNavDTD::WillResumeParse(nsIContentSink* aSink){

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::WillResumeParse(), this=%p\n", this));

  nsresult result=(aSink) ? aSink->WillResume() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::WillResumeParse(), this=%p\n", this));
  START_TIMER();

  return result;
}

/**
 * This method gets called when the parsing process is interrupted
 * due to lack of data (waiting for netlib).
 * @update  gess5/18/98
 * @return  error code
 */
nsresult CNavDTD::WillInterruptParse(nsIContentSink* aSink){

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::WillInterruptParse(), this=%p\n", this));

  nsresult result=(aSink) ? aSink->WillInterrupt() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::WillInterruptParse(), this=%p\n", this));
  START_TIMER();

  return result;
}

