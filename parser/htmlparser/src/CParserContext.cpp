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

MOZ_DECL_CTOR_COUNTER(CParserContext)

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
  mDTDMode=eDTDMode_unknown; 
  mAutoDetectStatus=aStatus; 
  mTransferBuffer=0; 
  mDTD=aDTD; 
  NS_IF_ADDREF(mDTD); 
  mTransferBufferSize=eTransferBufferSize; 
  mStreamListenerState=eNone; 
  mMultipart=PR_TRUE; 
  mContextType=eCTNone; 
  mCopyUnused=aCopyUnused; 
  mParserCommand=aCommand;
  mChannel=0;
  mValidator=0;
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

  mDTDMode=aContext.mDTDMode;
  mAutoDetectStatus=aContext.mAutoDetectStatus;
  mTransferBuffer=aContext.mTransferBuffer;
  mDTD=aContext.mDTD;
  NS_IF_ADDREF(mDTD);

  mTransferBufferSize=eTransferBufferSize;
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

  if(mScanner) {
    delete mScanner;
    mScanner=nsnull;
  }

  if(mTransferBuffer)
    delete [] mTransferBuffer;

  NS_IF_RELEASE(mDTD);
  NS_IF_RELEASE(mListener);

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


