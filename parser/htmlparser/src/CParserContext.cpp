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


#include "nsIAtom.h"
#include "CParserContext.h"
#include "nsToken.h"
#include "prenv.h"  
#include "nsHTMLTokenizer.h"
#include "nsExpatDriver.h"

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
                               nsIRequestObserver* aListener, 
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
  mTokenizer = 0;
  mTransferBufferSize=eTransferBufferSize; 
  mStreamListenerState=eNone; 
  mMultipart=PR_TRUE; 
  mContextType=eCTNone; 
  mCopyUnused=aCopyUnused; 
  mParserCommand=aCommand;
  mRequest=0;
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

  mTokenizer = aContext.mTokenizer;
  NS_IF_ADDREF(mTokenizer);

  mTransferBufferSize=eTransferBufferSize;
  mStreamListenerState=aContext.mStreamListenerState;
  mMultipart=aContext.mMultipart;
  mContextType=aContext.mContextType;
  mRequest=aContext.mRequest;
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
  NS_IF_RELEASE(mTokenizer);

  //Remember that it's ok to simply ingore the PrevContext.

}


/**
 * Set's the mimetype for this context
 * @update	rickg 03.18.2000
 */
void CParserContext::SetMimeType(const nsACString& aMimeType){
  mMimeType.Assign(aMimeType);

  mDocType=ePlainText;

  if(mMimeType.EqualsLiteral(kHTMLTextContentType))
    mDocType=eHTML_Strict;
  else if (mMimeType.EqualsLiteral(kXMLTextContentType)          ||
           mMimeType.EqualsLiteral(kXMLApplicationContentType)   ||
           mMimeType.EqualsLiteral(kXHTMLApplicationContentType) ||
           mMimeType.EqualsLiteral(kXULTextContentType)          ||
#ifdef MOZ_SVG
           mMimeType.EqualsLiteral(kSVGTextContentType)          ||
#endif
           mMimeType.EqualsLiteral(kRDFApplicationContentType)   ||
           mMimeType.EqualsLiteral(kRDFTextContentType))
    mDocType=eXML;
}

nsresult
CParserContext::GetTokenizer(PRInt32 aType, nsITokenizer*& aTokenizer) {
  nsresult result = NS_OK;
  
  if(!mTokenizer) {
    if (aType == NS_IPARSER_FLAG_HTML || mParserCommand == eViewSource) {
      result = NS_NewHTMLTokenizer(&mTokenizer,mDTDMode,mDocType,mParserCommand);
      // Propagate tokenizer state so that information is preserved
      // between document.write. This fixes bug 99467
      if (mTokenizer && mPrevContext)
        mTokenizer->CopyState(mPrevContext->mTokenizer);
    }
    else if (aType == NS_IPARSER_FLAG_XML)
    {
      result = CallQueryInterface(mDTD, &mTokenizer);
    }
  }
  
  aTokenizer = mTokenizer;
  return result;
}
