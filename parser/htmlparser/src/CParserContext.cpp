/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "CParserContext.h"
#include "nsToken.h"
#include "prenv.h"  

MOZ_DECL_CTOR_COUNTER(CParserContext);

/**
 * Your friendly little constructor. Ok, it's not the friendly, but the only guy
 * using it is the parser.
 * @update	gess7/23/98
 * @param   aScanner
 * @param   aKey
 * @param   aListener
 */
CParserContext::CParserContext(nsScanner* aScanner, 
                               void *aKey, 
                               eParserCommands aCommand,
                               nsIStreamObserver* aListener, 
                               nsIDTD *aDTD, 
                               eAutoDetectResult aStatus, 
                               PRBool aCopyUnused)
{ 
  MOZ_COUNT_CTOR(CParserContext); 

  mScanner=aScanner; 
  mKey=aKey; 
  mPrevContext=0; 
  mListener=aListener; 
  NS_IF_ADDREF(mListener); 
  mParseMode=eParseMode_unknown; 
  mAutoDetectStatus=aStatus; 
  mTransferBuffer=0; 
  mDTD=aDTD; 
  NS_IF_ADDREF(mDTD); 
  mTransferBufferSize=eTransferBufferSize; 
  mParserEnabled=PR_TRUE; 
  mStreamListenerState=eNone; 
  mMultipart=PR_TRUE; 
  mContextType=eCTNone; 
  mCopyUnused=aCopyUnused; 
  mParserCommand=aCommand;
  mChannel=0;
} 

/**
 * Your friendly little constructor. Ok, it's not the friendly, but the only guy
 * using it is the parser.
 * @update	gess7/23/98
 * @param   aScanner
 * @param   aKey
 * @param   aListener
 */
CParserContext::CParserContext(const CParserContext &aContext) : mMimeType() {
  MOZ_COUNT_CTOR(CParserContext);

  mScanner=aContext.mScanner;
  mKey=aContext.mKey;
  mPrevContext=0;
  mListener=aContext.mListener;
  NS_IF_ADDREF(mListener);

  mParseMode=aContext.mParseMode;
  mAutoDetectStatus=aContext.mAutoDetectStatus;
  mTransferBuffer=aContext.mTransferBuffer;
  mDTD=aContext.mDTD;
  NS_IF_ADDREF(mDTD);

  mTransferBufferSize=eTransferBufferSize;
  mParserEnabled=aContext.mParserEnabled;
  mStreamListenerState=aContext.mStreamListenerState;
  mMultipart=aContext.mMultipart;
  mContextType=aContext.mContextType;
  mChannel=aContext.mChannel;
  mParserCommand=aContext.mParserCommand;
  SetMimeType(aContext.mMimeType);
}


/**
 * Destructor for parser context
 * NOTE: DO NOT destroy the dtd here.
 * @update	gess7/11/98
 */
CParserContext::~CParserContext(){

  MOZ_COUNT_DTOR(CParserContext);

  if(mScanner)
    delete mScanner;

  if(mTransferBuffer)
    delete [] mTransferBuffer;

  NS_IF_RELEASE(mDTD);

  //Remember that it's ok to simply ingore the PrevContext.

}


/**
 * Set's the mimetype for this context
 * @update	rickg 03.18.2000
 */
void CParserContext::SetMimeType(const nsString& aMimeType){
  mMimeType.Assign(aMimeType);

  mDocType=ePlainText;

  if(mMimeType.EqualsWithConversion(kHTMLTextContentType))
    mDocType=eHTML4Text;
  else if(mMimeType.EqualsWithConversion(kXMLTextContentType))
    mDocType=eXMLText;
  else if(mMimeType.EqualsWithConversion(kXULTextContentType))
    mDocType=eXMLText;
  else if(mMimeType.EqualsWithConversion(kRDFTextContentType))
    mDocType=eXMLText;
  else if(mMimeType.EqualsWithConversion(kXIFTextContentType))
    mDocType=eXMLText;
}
 
/**
 *  This is called when it's time to find out 
 *  what mode the parser/DTD should run for this document.
 *  (Each parsercontext can have it's own mode).
 *  
 *  @update  gess 02/17/00
 *  @return  parsermode (define in nsIParser.h)
 */
eParseMode CParserContext::DetermineParseMode(const nsString& theBuffer) {
  const char* theModeStr= PR_GetEnv("PARSE_MODE");

  mParseMode = eParseMode_unknown;
    
  PRInt32 theIndex=theBuffer.Find("<!",PR_FALSE,-1);
  if(kNotFound<theIndex)
    theIndex=theBuffer.Find("DOCTYPE",PR_TRUE,theIndex+1,10);

  if(kNotFound<theIndex) {
  
    //good, we found "DOCTYPE" -- now go find it's end delimiter '>'
    PRInt32 theGTPos=theBuffer.FindChar(kGreaterThan,theIndex+1);
    PRInt32 theEnd=(kNotFound==theGTPos) ? 512 : MinInt(512,theGTPos);
    PRInt32 theSubIndex=theBuffer.Find("//DTD",PR_TRUE,theIndex+8,theEnd-(theIndex+8));
    PRInt32 theErr=0;
    PRInt32 theMajorVersion=3;


    //note that if we don't find '>', then we just scan the first 512 bytes.

    if(0<=theSubIndex) {
      PRInt32 theStartPos=theSubIndex+5;
      PRInt32 theCount=theEnd-theStartPos;

      if(kNotFound<theSubIndex) {

        theSubIndex=theBuffer.Find("XHTML",PR_TRUE,theStartPos,theCount);
        if(0<=theSubIndex) {
          mDocType=eXHTMLText;
          mParseMode=eParseMode_strict;
          theMajorVersion=1;
        }
        else {
          theSubIndex=theBuffer.Find("ISO/IEC 15445:",PR_TRUE,theIndex+8,theEnd-(theIndex+8));
          if(0<=theSubIndex) {
            mDocType=eHTML4Text;
            mParseMode=eParseMode_strict;
            theMajorVersion=4;
            theSubIndex+=15;
          }
          else {
            theSubIndex=theBuffer.Find("HTML",PR_TRUE,theStartPos,theCount);
            if(0<=theSubIndex) {
              mDocType=eHTML4Text;
              mParseMode=eParseMode_strict;
              theMajorVersion=3;
            }
            else {
              theSubIndex=theBuffer.Find("HYPERTEXT MARKUP LANGUAGE",PR_TRUE,theStartPos,theCount);
              if(0<=theSubIndex) {
                mDocType=eHTML3Text;
                mParseMode=eParseMode_quirks;
                theSubIndex+=20;
              }
            } 
          }
        }
      }

      theStartPos=theSubIndex+5;
      theCount=theEnd-theStartPos;
      nsAutoString theNum;

      theStartPos=theBuffer.FindCharInSet("123456789",theStartPos);
      if(0<=theStartPos) {
        theBuffer.Mid(theNum,theStartPos-1,3);
        theMajorVersion=theNum.ToInteger(&theErr);
      }
      
        //get the next substring from the buffer, which should be a number.
        //now see what the version number is...

      theStartPos+=3;
      theCount=theEnd-theStartPos;
      if((theBuffer.Find("TRANSITIONAL",PR_TRUE,theStartPos,theCount)>kNotFound)||
         (theBuffer.Find("LOOSE",PR_TRUE,theStartPos,theCount)>kNotFound)       ||
         (theBuffer.Find("FRAMESET",PR_TRUE,theStartPos,theCount)>kNotFound)    ||
         (theBuffer.Find("LATIN1", PR_TRUE,theStartPos,theCount) >kNotFound)    ||
         (theBuffer.Find("SYMBOLS",PR_TRUE,theStartPos,theCount) >kNotFound)    ||
         (theBuffer.Find("SPECIAL",PR_TRUE,theStartPos,theCount) >kNotFound)) {
        mParseMode=eParseMode_noquirks;
      }

      if(eXHTMLText!=mDocType) {
        if (0==theErr){
          switch(theMajorVersion) {
            case 0: case 1: case 2: case 3:
              if(mDocType!=eXHTMLText){
                mParseMode=eParseMode_quirks; //be as backward compatible as possible
                mDocType=eHTML3Text;
              }
              break;
    
            default:
              if(5<theMajorVersion) {
                mParseMode=eParseMode_noquirks;
              }
              break;
          } //switch
        }
      }

    } //if
    else {
      PRInt32 thePos=theBuffer.Find("HTML",PR_TRUE,1,50);
      if(kNotFound!=thePos) {
        mDocType=eHTML4Text;
        PRInt32 theIDPos=theBuffer.Find("PublicID",thePos);
        if(kNotFound==theIDPos)
          theIDPos=theBuffer.Find("SystemID",thePos);
        mParseMode=(kNotFound==theIDPos) ? eParseMode_quirks : eParseMode_strict;
      }
    }
  }
  else if(kNotFound<(theIndex=theBuffer.Find("?XML",PR_TRUE,0,128))) {
      mParseMode=eParseMode_noquirks;
  }
  else {
      //this is debug only, and will go away by the time we ship...
    theIndex=theBuffer.Find("NOQUIRKS",PR_TRUE,0,128);
    mDocType=eHTML4Text;
    if(kNotFound<theIndex) {
      mParseMode=eParseMode_noquirks;
    }
  }

  if(theModeStr) {
    if(0==nsCRT::strcasecmp(theModeStr,"strict"))
      mParseMode=eParseMode_strict;    
  }
  else mParseMode = (eParseMode_unknown==mParseMode)? eParseMode_quirks : mParseMode;

  return mParseMode;
}
