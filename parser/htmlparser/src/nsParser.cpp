/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
  


#include "nsParser.h"
#include "nsIContentSink.h" 
#include "nsString.h"
#include "nsCRT.h"
#include "nsScanner.h"
#include "prenv.h"  //this is here for debug reasons...
#include "plstr.h"
#include <fstream.h>
#include "nsIParserFilter.h"
#include "nshtmlpars.h"
#include "nsWellFormedDTD.h"
#include "nsViewSourceHTML.h" //uncomment this to partially enable viewsource...

#undef rickgdebug
#ifdef  rickgdebug
#include "CRtfDTD.h"
#endif


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kClassIID, NS_PARSER_IID); 
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);

static const char* kNullURL = "Error: Null URL given";
static const char* kOnStartNotCalled = "Error: OnStartBinding() must be called before OnDataAvailable()";
static const char* kOnStopNotCalled  = "Error: OnStopBinding() must be called upon termination of netlib process";
static const char* kBadListenerInit  = "Error: Parser's IStreamListener API was not setup correctly in constructor.";
static nsString    kUnknownFilename("unknown");
static nsString    kEmptyString("unknown");

static const int  gTransferBufferSize=4096;  //size of the buffer used in moving data from iistream


class CTokenDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    CToken* aToken = (CToken*)anObject;
    delete aToken;
    return 0;
  }
};

CTokenDeallocator gTokenDeallocator2;

class CDTDDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    nsIDTD* aDTD =(nsIDTD*)anObject;
    NS_RELEASE(aDTD);
    return 0;
  }
};

class CDTDFinder: public nsDequeFunctor{
public:
  CDTDFinder(nsIDTD* aDTD) {
    NS_IF_ADDREF(mTargetDTD=aDTD);
  }
  virtual ~CDTDFinder() {
    NS_IF_RELEASE(mTargetDTD);
  }
  virtual void* operator()(void* anObject) {
    return (anObject==(void*)mTargetDTD) ? anObject : 0;
  }
  nsIDTD* mTargetDTD;
};

class CSharedParserObjects {
public:

  CSharedParserObjects() : mDeallocator(), mDTDDeque(mDeallocator) {
    nsIDTD* theDTD;
    NS_NewWellFormed_DTD(&theDTD);
    RegisterDTD(theDTD);

    NS_NewViewSourceHTML(&theDTD);
    RegisterDTD(theDTD);
  }

  ~CSharedParserObjects() {
  }

  void RegisterDTD(nsIDTD* aDTD){
    CDTDFinder theFinder(aDTD);
    if(!mDTDDeque.FirstThat(theFinder))
      mDTDDeque.Push(aDTD);
  }
  
  nsIDTD*  FindDTD(nsIDTD* aDTD){
    return 0;
  }

  CDTDDeallocator mDeallocator;
  nsDeque mDTDDeque;
};

CSharedParserObjects gSharedParserObjects;

//----------------------------------------

#define NOT_USED 0xfffd

static PRUint16 PA_HackTable[] = {
	NOT_USED,
	NOT_USED,
	0x201a,  /* SINGLE LOW-9 QUOTATION MARK */
	0x0192,  /* LATIN SMALL LETTER F WITH HOOK */
	0x201e,  /* DOUBLE LOW-9 QUOTATION MARK */
	0x2026,  /* HORIZONTAL ELLIPSIS */
	0x2020,  /* DAGGER */
	0x2021,  /* DOUBLE DAGGER */
	0x02c6,  /* MODIFIER LETTER CIRCUMFLEX ACCENT */
	0x2030,  /* PER MILLE SIGN */
	0x0160,  /* LATIN CAPITAL LETTER S WITH CARON */
	0x2039,  /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
	0x0152,  /* LATIN CAPITAL LIGATURE OE */
	NOT_USED,
	NOT_USED,
	NOT_USED,

	NOT_USED,
	0x2018,  /* LEFT SINGLE QUOTATION MARK */
	0x2019,  /* RIGHT SINGLE QUOTATION MARK */
	0x201c,  /* LEFT DOUBLE QUOTATION MARK */
	0x201d,  /* RIGHT DOUBLE QUOTATION MARK */
	0x2022,  /* BULLET */
	0x2013,  /* EN DASH */
	0x2014,  /* EM DASH */
	0x02dc,  /* SMALL TILDE */
	0x2122,  /* TRADE MARK SIGN */
	0x0161,  /* LATIN SMALL LETTER S WITH CARON */
	0x203a,  /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
	0x0153,  /* LATIN SMALL LIGATURE OE */
	NOT_USED,
	NOT_USED,
	0x0178   /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
};

static PRUnichar gToUCS2[256];

static void
MakeConversionTable()
{
  static PRBool firstTime = PR_TRUE;
  if (firstTime) {
    firstTime = PR_FALSE;
    PRUnichar* cp = gToUCS2;
    PRInt32 i;
    for (i = 0; i < 256; i++) {
      *cp++ = PRUnichar(i);
    }
    cp = gToUCS2;
    for (i = 0; i < 32; i++) {
      cp[0x80 + i] = PA_HackTable[i];
    }
  }
}

//----------------------------------------

/**
 *  default constructor
 *  
 *  @update  vidur 12/11/98
 *  @param   
 *  @return  
 */
nsParser::nsParser() : mCommand("") {
  NS_INIT_REFCNT();
  mStreamListenerState=eNone;
  mParserFilter = 0;
  mObserver = 0;
  mSink=0;
  mParserContext=0;
  mDTDVerification=PR_FALSE;
  mParserEnabled=PR_TRUE;
  MakeConversionTable();
}


/**
 *  Default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsParser::~nsParser() {
  NS_IF_RELEASE(mObserver);
  NS_RELEASE(mSink);

  NS_POSTCONDITION(eOnStop==mStreamListenerState,kOnStopNotCalled);
  //don't forget to add code here to delete 
  //what may be several contexts...
  delete mParserContext;
}


NS_IMPL_ADDREF(nsParser)
NS_IMPL_RELEASE(nsParser)
//NS_IMPL_ISUPPORTS(nsParser,NS_IHTML_PARSER_IID)


/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 3/25/98
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
  else if(aIID.Equals(kIStreamListenerIID)) {  //do IStreamListener base class...
    *aInstancePtr = (nsIStreamListener*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsParser*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}


/**
 * 
 * @update	gess6/18/98
 * @param 
 * @return
 */
nsIParserFilter * nsParser::SetParserFilter(nsIParserFilter * aFilter)
{
  nsIParserFilter* old=mParserFilter;
  if(old)
    NS_RELEASE(old);
  if(aFilter) {
    mParserFilter=aFilter;
    NS_ADDREF(aFilter);
  }
  return old;
}

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about the command which caused the parser to be constructed. For example,
 *  this allows us to select a DTD which can do, say, view-source.
 *  
 *  @update  gess 3/25/98
 *  @param   aContentSink -- ptr to content sink that will receive output
 *  @return	 ptr to previously set contentsink (usually null)  
 */
void nsParser::SetCommand(const char* aCommand){
  mCommand=aCommand;
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *  
 *  @update  gess 3/25/98
 *  @param   nsIContentSink interface for node receiver
 *  @return  
 */
nsIContentSink* nsParser::SetContentSink(nsIContentSink* aSink) {
  NS_PRECONDITION(0!=aSink,"sink cannot be null!");
  nsIContentSink* old=mSink;
  if(old)
    NS_RELEASE(old);
  if(aSink) {
    mSink=aSink;
    NS_ADDREF(aSink);
    mSink->SetParser(this);
  }
  return old;
}

/**
 * retrive the sink set into the parser 
 * @update	gess5/11/98
 * @param   aSink is the new sink to be used by parser
 * @return  old sink, or NULL
 */
nsIContentSink* nsParser::GetContentSink(void){
  return mSink;
}

/**
 *  Call this static method when you want to
 *  register your dynamic DTD's with the parser.
 *  
 *  @update  gess 6/9/98
 *  @param   aDTD  is the object to be registered.
 *  @return  nothing.
 */
void nsParser::RegisterDTD(nsIDTD* aDTD){

#ifdef rickgdebug
  nsIDTD* rv=0;
  NS_NewRTF_DTD(&rv);
  gSharedParserObjects.RegisterDTD(rv);
  NS_NewWellFormed_DTD(&rv);
  gSharedParserObjects.RegisterDTD(rv);
#endif

  gSharedParserObjects.RegisterDTD(aDTD);
}

/**
 *  Retrieve scanner from topmost parsecontext
 *  
 *  @update  gess 6/9/98
 *  @return  ptr to internal scanner
 */
CScanner* nsParser::GetScanner(void){
  if(mParserContext)
    return mParserContext->mScanner;
  return 0;
}


/**
 *  Retrieve parsemode from topmost parser context
 *  
 *  @update  gess 6/9/98
 *  @return  parsemode
 */
eParseMode nsParser::GetParseMode(void){
  if(mParserContext)
    return mParserContext->mParseMode;
  return eParseMode_unknown;
}



/**
 *  
 *  
 *  @update  gess 5/13/98
 *  @param   
 *  @return  
 */
PRBool FindSuitableDTD( CParserContext& aParserContext,nsString& aCommand) {

    //Let's start by tring the defaultDTD, if one exists...
  if(aParserContext.mDTD)
    if(aParserContext.mDTD->CanParse(aParserContext.mSourceType,aCommand,0))
      return PR_TRUE;

  PRBool  result=PR_FALSE;

  nsDequeIterator b=gSharedParserObjects.mDTDDeque.Begin(); 
  nsDequeIterator e=gSharedParserObjects.mDTDDeque.End(); 

  while(b<e){
    nsIDTD* theDTD=(nsIDTD*)b.GetCurrent();
    if(theDTD) {
      result=theDTD->CanParse(aParserContext.mSourceType,aCommand,0); 
      if(result){
        theDTD->CreateNewInstance(&aParserContext.mDTD);
        break;
      }
    }
    b++;
  } 

  return result;
}

/**
 * Call this method if you want the known DTD's to try 
 * to detect the document type based through analysis
 * of the underlying stream.
 *
 * @update	gess6/22/98
 * @param   aBuffer -- nsString containing sample data to be analyzed.
 * @param   aType -- may hold typename given from netlib; will hold result given by DTD's.
 * @return  auto-detect result: eValid, eInvalid, eUnknown
 */
eAutoDetectResult nsParser::AutoDetectContentType(nsString& aBuffer,nsString& aType) {

    //The process:
    //  You should go out and ask each DTD if they
    //  recognize the content in the scanner.
    //  Somebody should say yes, or we can't continue.

    //This method may change mSourceType and mParserContext->mDTD.
    //It absolutely changes mParserContext->mAutoDetectStatus

  nsDequeIterator b=gSharedParserObjects.mDTDDeque.Begin(); 
  nsDequeIterator e=gSharedParserObjects.mDTDDeque.End(); 

  mParserContext->mAutoDetectStatus=eUnknownDetect;
  while(b<e){
    nsIDTD* theDTD=(nsIDTD*)b.GetCurrent();
    if(theDTD) {
      mParserContext->mAutoDetectStatus=theDTD->AutoDetectContentType(aBuffer,aType);
      if(eValidDetect==mParserContext->mAutoDetectStatus)
        break;
    }
    b++;
  } 

  return mParserContext->mAutoDetectStatus;  
}


/**
 *  This is called (by willBuildModel) when it's time to find out 
 *  what mode the parser/DTD should run for this document.
 *  (Each parsercontext can have it's own mode).
 *  
 *  @update  gess 5/13/98
 *  @return  parsermode (define in nsIParser.h)
 */
eParseMode DetermineParseMode(nsParser& aParser) {
  const char* theModeStr= PR_GetEnv("PARSE_MODE");
  const char* other="other";
  
  CScanner* theScanner=aParser.GetScanner();
  if(theScanner){
    nsString& theBuffer=theScanner->GetBuffer();
    PRInt32 theIndex=theBuffer.Find("HTML 4.0");
    if(kNotFound==theIndex)
      theIndex=theBuffer.Find("html 4.0");
    if(kNotFound<theIndex)
      return eParseMode_raptor;
    else {
      PRInt32 theIndex=theBuffer.Find("noquirks");
      if(kNotFound==theIndex)
        theIndex=theBuffer.Find("NOQUIRKS");
      if(kNotFound<theIndex)
        return eParseMode_noquirks;
    }
  }

  if(theModeStr) 
    if(0==nsCRT::strcasecmp(other,theModeStr))
      return eParseMode_other;    
  return eParseMode_navigator;
}


/**
 * This gets called just prior to the model actually
 * being constructed. It's important to make this the
 * last thing that happens right before parsing, so we
 * can delay until the last moment the resolution of
 * which DTD to use (unless of course we're assigned one).
 *
 * @update	gess5/18/98
 * @param 
 * @return  error code -- 0 if ok, non-zero if error.
 */
PRInt32 nsParser::WillBuildModel(nsString& aFilename,nsIDTD* aDefaultDTD){

  mMajorIteration=-1; 
  mMinorIteration=-1; 
  PRInt32 result=kNoError;
  if(mParserContext){
    mParserContext->mParseMode=DetermineParseMode(*this);  
    mParserContext->mDTD=aDefaultDTD;
    if(PR_TRUE==FindSuitableDTD(*mParserContext,mCommand)) {
      //mParserContext->mDTD->SetContentSink(mSink);
      mParserContext->mDTD->WillBuildModel(aFilename,PRBool(0==mParserContext->mPrevContext),this);
    }
  }
  else result=kInvalidParserContext;
  return result;
}

/**
 * This gets called when the parser is done with its input.
 * Note that the parser may have been called recursively, so we
 * have to check for a prev. context before closing out the DTD/sink.
 * @update	gess5/18/98
 * @param 
 * @return  error code -- 0 if ok, non-zero if error.
 */
PRInt32 nsParser::DidBuildModel(PRInt32 anErrorCode) {
  //One last thing...close any open containers.
  PRInt32 result=anErrorCode;

  if((!mParserContext->mPrevContext) && (mParserContext->mDTD)) {
    result=mParserContext->mDTD->DidBuildModel(anErrorCode,PRBool(0==mParserContext->mPrevContext),this);

    //Now recycle any tokens that are still hanging around.
    //Come to think of it, there really shouldn't be any.
    nsDeque&  theDeque=mParserContext->mTokenDeque;
    nsITokenRecycler* theRecycler=mParserContext->mDTD->GetTokenRecycler();
    if(theRecycler) {
      CToken* theToken=(CToken*)theDeque.Pop();
      while(theToken) {
        theRecycler->RecycleToken(theToken);
        theToken=(CToken*)theDeque.Pop();
      }
    }
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
CParserContext* nsParser::PopContext() {
  CParserContext* oldContext=mParserContext;
  if(oldContext) {
    mParserContext=oldContext->mPrevContext;
  }
  return oldContext;
}

/**
 *  Call this when you want control whether or not the parser will parse
 *  and tokenize input (TRUE), or whether it just caches input to be 
 *  parsed later (FALSE).
 *  
 *  @update  vidur 12/11/98
 *  @param   aState determines whether we parse/tokenize or just cache.
 *  @return  current state
 */
PRBool nsParser::EnableParser(PRBool aState){
  nsIParser* me = nsnull;

  // If the stream has already finished, there's a good chance
  // that we might start closing things down when the parser
  // is reenabled. To make sure that we're not deleted across
  // the reenabling process, hold a reference to ourselves.
  if (eOnStop == mStreamListenerState) {
    me = this;
    NS_ADDREF(me);
  }    

  // If we're reenabling the parser
  if ((PR_FALSE == mParserEnabled) && aState) {
    mParserEnabled = PR_TRUE;
    ResumeParse();
    // If the stream has already closed, finish out the parsing
    // process. Note if the parser was disabled when we resumed
    // parsing, then we have to wait till its reenabled before
    // finishing.
    if ((eOnStop == mStreamListenerState) && mParserEnabled) {
      DidBuildModel(mStreamStatus);
    }
  }
  else {
    mParserEnabled=aState;
  }

  // Release reference if we added one at the top of this routine
  NS_IF_RELEASE(me);

  return aState;
}


/**
 *  This is the main controlling routine in the parsing process. 
 *  Note that it may get called multiple times for the same scanner, 
 *  since this is a pushed based system, and all the tokens may 
 *  not have been consumed by the scanner during a given invocation 
 *  of this method. 
 *
 *  @update  gess 3/25/98
 *  @param   aFilename -- const char* containing file to be parsed.
 *  @return  error code -- 0 if ok, non-zero if error.
 */
PRInt32 nsParser::Parse(nsIURL* aURL,nsIStreamObserver* aListener,PRBool aVerifyEnabled) {
  NS_PRECONDITION(0!=aURL,kNullURL);

  PRInt32 status=kBadURL;
  mDTDVerification=aVerifyEnabled;
 
  if(aURL) {
    nsAutoString theName(aURL->GetSpec());
    CParserContext* cp=new CParserContext(new CScanner(theName,PR_FALSE),aURL,aListener);
    PushContext(*cp);
    status=NS_OK;
  }
  return status;
}


/**
 * Cause parser to parse input from given stream 
 * @update	vidur 12/11/98
 * @param   aStream is the i/o source
 * @return  error code -- 0 if ok, non-zero if error.
 */
PRInt32 nsParser::Parse(fstream& aStream,PRBool aVerifyEnabled){

  PRInt32 status=kNoError;
  mDTDVerification=aVerifyEnabled;
  
  //ok, time to create our tokenizer and begin the process
  CParserContext* pc=new CParserContext(new CScanner(kUnknownFilename,aStream,PR_FALSE),&aStream,0);
  PushContext(*pc);
  pc->mSourceType="text/html";
  mParserContext->mScanner->Eof();
  if(eValidDetect==AutoDetectContentType(mParserContext->mScanner->GetBuffer(),
                                         mParserContext->mSourceType)) {
    WillBuildModel(mParserContext->mScanner->GetFilename(),0);
    status=ResumeParse();
    DidBuildModel(status);
  } //if

  pc=PopContext();
  delete pc;

  return status;
}


/**
 * Call this method if all you want to do is parse 1 string full of HTML text.
 * In particular, this method should be called by the DOM when it has an HTML
 * string to feed to the parser in real-time.
 *
 * @update	gess5/11/98
 * @param   aSourceBuffer contains a string-full of real content
 * @param   anHTMLString tells us whether we should assume the content is HTML (usually true)
 * @return  error code -- 0 if ok, non-zero if error.
 */
PRInt32 nsParser::Parse(nsString& aSourceBuffer,PRBool anHTMLString,PRBool aVerifyEnabled){
  PRInt32 result=kNoError;
  mDTDVerification=aVerifyEnabled;

  if(0<aSourceBuffer.Length()){
    CParserContext* pc=new CParserContext(new CScanner(aSourceBuffer),&aSourceBuffer,0);

    nsIDTD* thePrevDTD=(mParserContext) ? mParserContext->mDTD: 0;
      
    PushContext(*pc);
    if(PR_TRUE==anHTMLString)
      pc->mSourceType="text/html";
    if(eValidDetect==AutoDetectContentType(aSourceBuffer,mParserContext->mSourceType)) {
      WillBuildModel(mParserContext->mScanner->GetFilename(),thePrevDTD);
      result=ResumeParse();
      DidBuildModel(result);
    }
    mParserContext->mDTD=0; 
    pc=PopContext();
    delete pc;
  }
  return result;
}


/**
 *  This routine is called to cause the parser to continue
 *  parsing it's underlying stream. This call allows the
 *  parse process to happen in chunks, such as when the
 *  content is push based, and we need to parse in pieces.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
PRInt32 nsParser::ResumeParse() {
  PRInt32 result=kNoError;
  PRInt32 buildResult=kNoError;

  mParserContext->mDTD->WillResumeParse();
  if(kNoError==result) {
    result=Tokenize();
    buildResult=BuildModel();
    if(kInterrupted==result)
      mParserContext->mDTD->WillInterruptParse();
    // If we're told to block the parser, we disable
    // all further parsing (and cache any data coming
    // in) until the parser is enabled.
    if(NS_ERROR_HTMLPARSER_BLOCK==buildResult) {
      EnableParser(PR_FALSE);
    }
  }
  return result;
}

/**
 *  This is where we loop over the tokens created in the 
 *  tokenization phase, and try to make sense out of them. 
 *
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
PRInt32 nsParser::BuildModel() {
  
  nsDequeIterator e=mParserContext->mTokenDeque.End(); 
  nsDequeIterator theMarkPos(e);

  if(!mParserContext->mCurrentPos)
    mParserContext->mCurrentPos=new nsDequeIterator(mParserContext->mTokenDeque.Begin());

    //Get the root DTD for use in model building...
  CParserContext* theRootContext=mParserContext;
  while(theRootContext->mPrevContext)
    theRootContext=theRootContext->mPrevContext;

  nsIDTD* theRootDTD=theRootContext->mDTD;
  
  PRInt32 result=kNoError;
  while((kNoError==result) && ((*mParserContext->mCurrentPos<e))){
    mMinorIteration++;
    CToken* theToken=(CToken*)mParserContext->mCurrentPos->GetCurrent();
    theMarkPos=*mParserContext->mCurrentPos;
    ++(*mParserContext->mCurrentPos);    
    result=theRootDTD->HandleToken(theToken,this);
    if(mDTDVerification)
      theRootDTD->Verify(kEmptyString,this);
  }

    //Now it's time to recycle our used tokens.
    //The current context has a deque full of them,
    //and the ones that preceed currentpos are no
    //longer needed. Let's recycle them.
  nsITokenRecycler* theRecycler=theRootDTD->GetTokenRecycler();
  if(theRecycler) {
    nsDeque&  theDeque=mParserContext->mTokenDeque;
    CToken* theCurrentToken=(CToken*)mParserContext->mCurrentPos->GetCurrent();
    for(;;) {
      CToken* theToken=(CToken*)theDeque.Peek();
      if(theToken && (theToken!=theCurrentToken)){
        theDeque.Pop();
        theRecycler->RecycleToken(theToken);
      }
      else break;
    }
    mParserContext->mCurrentPos->First();
  }

  return result;
}


/**
 * This method provides access to the topmost token in the tokenDeque.
 * The token is not really removed from the list.
 * @update	gess8/2/98
 * @return  ptr to token
 */
CToken* nsParser::PeekToken() {
  CToken* theToken=0;
  if(mParserContext)
    if(mParserContext->mCurrentPos)
      theToken=(CToken*)mParserContext->mCurrentPos->GetCurrent();
  return theToken;
}


/**
 * This method provides access to the topmost token in the tokenDeque.
 * The token is really removed from the list; if the list is empty we return 0.
 * @update	gess8/2/98
 * @return  ptr to token or NULL
 */
CToken* nsParser::PopToken() {
  CToken* theToken=0;
  if(mParserContext)
    if(mParserContext->mCurrentPos) {
      theToken=(CToken*)mParserContext->mCurrentPos->GetCurrent();
      ++(*mParserContext->mCurrentPos);
    }
  return theToken;
}


/**
 * 
 * @update	gess8/2/98
 * @param 
 * @return
 */
CToken* nsParser::PushToken(CToken* theToken) {
  mParserContext->mTokenDeque.Push(theToken);
	return theToken;
}

/*******************************************************************
  These methods are used to talk to the netlib system...
 *******************************************************************/

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::GetBindInfo(nsIURL* aURL){
  nsresult result=0;
  return result;
}

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult
nsParser::OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
  nsresult result=0;
  if (nsnull != mObserver) {
    mObserver->OnProgress(aURL, aProgress, aProgressMax);
  }
  return result;
}

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult
nsParser::OnStatus(nsIURL* aURL, const nsString &aMsg)
{
  nsresult result=0;
  if (nsnull != mObserver) {
    mObserver->OnStatus(aURL, aMsg);
  }
  return result;
}

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::OnStartBinding(nsIURL* aURL, const char *aSourceType){
  NS_PRECONDITION((eNone==mStreamListenerState),kBadListenerInit);

  if (nsnull != mObserver) {
    mObserver->OnStartBinding(aURL, aSourceType);
  }
  mStreamListenerState=eOnStart;
  mParserContext->mAutoDetectStatus=eUnknownDetect;
  mParserContext->mDTD=0;
  mParserContext->mSourceType=aSourceType;
  return kNoError;
}

/**
 *  
 *  
 *  @update  vidur 12/11/98
 *  @param   pIStream contains the input chars
 *  @param   length is the number of bytes waiting input
 *  @return  error code (usually 0)
 */
nsresult nsParser::OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRInt32 length){
/*  if (nsnull != mListener) {
      //Rick potts removed this.
      //Does it need to be here?
    mListener->OnDataAvailable(pIStream, length);
  }
*/
  NS_PRECONDITION(((eOnStart==mStreamListenerState)||(eOnDataAvail==mStreamListenerState)),kOnStartNotCalled);

  mStreamListenerState=eOnDataAvail;  
  if(eInvalidDetect==mParserContext->mAutoDetectStatus) {
    if(mParserContext->mScanner) {
      mParserContext->mScanner->GetBuffer().Truncate();
    }
  }

  int len=1; //init to a non-zero value

  if(!mParserContext->mTransferBuffer)
    mParserContext->mTransferBuffer = new char[CParserContext::eTransferBufferSize+1];

  while (len > 0) {
    nsresult rv = pIStream->Read(mParserContext->mTransferBuffer, 0,
                                 mParserContext->eTransferBufferSize, &len);
    if((rv == NS_OK) && (len>0)) {
      if(mParserFilter)
         mParserFilter->RawBuffer(mParserContext->mTransferBuffer, &len);

      // XXX kipp was here: this is a temporary piece of code that
      // fixes up the data in the transfer buffer so that the 8 bit
      // ascii is mapped to ucs2 properly. The problem is that for the
      // default character set, some web pages use illegal codes (0x80
      // to 0x9f, inclusive); we already have code to map entities
      // properly in this range. This code maps raw stream data the
      // same way.
      PRUnichar buf[CParserContext::eTransferBufferSize];
      PRUnichar* dst = buf;
      const PRUnichar* table = gToUCS2;
      const char* src = mParserContext->mTransferBuffer;
      const char* end = src + len;
      while (src < end) {
        unsigned char ch = *(unsigned char*)src;
        *dst++ = table[ch];
        src++;
      }

      mParserContext->mScanner->Append(buf, len);

      if(eUnknownDetect==mParserContext->mAutoDetectStatus) {
        if(eValidDetect==AutoDetectContentType(mParserContext->mScanner->GetBuffer(),mParserContext->mSourceType)) {
          WillBuildModel(mParserContext->mScanner->GetFilename(),0);
        } //if
      }
    } //if
  }

  if (mParserEnabled) {
    return ResumeParse();
  }
  else {
    return NS_OK;
  }
}

/**
 *  
 *  
 *  @update  vidur 12/11/98
 *  @param   
 *  @return  
 */
nsresult nsParser::OnStopBinding(nsIURL* aURL, PRInt32 status, const nsString& aMsg){
  mStreamListenerState=eOnStop;
  mStreamStatus=status;
  nsresult result;
  // If the parser isn't enabled, we don't finish parsing till
  // it is reenabled.
  if (mParserEnabled) {
    result=DidBuildModel(status);
  }
  // XXX Should we wait to notify our observers as well if the
  // parser isn't yet enabled?
  if (nsnull != mObserver) {
    mObserver->OnStopBinding(aURL, status, aMsg);
  }
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
 *  @update  gess 3/25/98
 *  @param   
 *  @return  TRUE if it's ok to proceed
 */
PRBool nsParser::WillTokenize(){
  PRBool result=PR_TRUE;
  return result;
}


/**
 *  This is the primary control routine to consume tokens. 
 *	It iteratively consumes tokens until an error occurs or 
 *	you run out of data.
 *  
 *  @update  gess 3/25/98
 *  @return  error code -- 0 if ok, non-zero if error.
 */
PRInt32 nsParser::Tokenize(){
  CToken* theToken=0;
  PRInt32 result=kNoError;

  ++mMajorIteration;

  WillTokenize();
  while(kNoError==result) {
    mParserContext->mScanner->Mark();
    result=mParserContext->mDTD->ConsumeToken(theToken,this);
    if(kNoError==result) {
      if(theToken) {

  #ifdef VERBOSE_DEBUG
          theToken->DebugDumpToken(cout);
  #endif
        mParserContext->mTokenDeque.Push(theToken);
      }

    }
    else {
      if(theToken)
        delete theToken;
      mParserContext->mScanner->RewindToMark();
    }
  } 
  if(kProcessComplete==result)
    result=NS_OK;
  DidTokenize();
  return result;
}

/**
 *  This is the tail-end of the code sandwich for the
 *  tokenization process. It gets called once tokenziation
 *  has completed.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  TRUE if all went well
 */
PRBool nsParser::DidTokenize(){
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  This debug routine is used to cause the tokenizer to
 *  iterate its token list, asking each token to dump its
 *  contents to the given output stream.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void nsParser::DebugDumpTokens(ostream& out) {
  nsDequeIterator b=mParserContext->mTokenDeque.Begin();
  nsDequeIterator e=mParserContext->mTokenDeque.End();

  CToken* theToken;
  while(b!=e) {
    theToken=(CToken*)(b++);
    theToken->DebugDumpToken(out);
  }
}


/**
 *  This debug routine is used to cause the tokenizer to
 *  iterate its token list, asking each token to dump its
 *  contents to the given output stream.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void nsParser::DebugDumpSource(ostream& out) {
  nsDequeIterator b=mParserContext->mTokenDeque.Begin();
  nsDequeIterator e=mParserContext->mTokenDeque.End();

  CToken* theToken;
  while(b!=e) {
    theToken=(CToken*)(b++);
    theToken->DebugDumpSource(out);
  }

}


