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

/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */

#ifndef __CParserContext
#define __CParserContext

#include "nsIParser.h"
#include "nsIURL.h"
#include "nsIDTD.h"
#include "nsScanner.h"
#include "nsIStreamListener.h"
#include "nsString.h"
#include "nshtmlpars.h"

/**
 * Note that the parser is given FULL access to all
 * data in a parsercontext. Hey, that what it's for!
 */

class CParserContext {

public:

    enum {eTransferBufferSize=4096};
    enum eContextType {eCTNone,eCTURL,eCTString,eCTStream};

   CParserContext(  nsScanner* aScanner,
                    void* aKey=0, 
                    eParserCommands aCommand=eViewNormal,
                    nsIStreamObserver* aListener=0, 
                    nsIDTD *aDTD=0, 
                    eAutoDetectResult aStatus=eUnknownDetect, 
                    PRBool aCopyUnused=PR_FALSE); 
    
    CParserContext( const CParserContext& aContext);
    ~CParserContext();

    void  SetMimeType(const nsString& aMimeType);

    CParserContext*     mPrevContext;
    nsDTDMode           mDTDMode;
    eParserDocType      mDocType;
    nsAutoString        mMimeType;

    eStreamState        mStreamListenerState; //this is really only here for debug purposes.
    PRBool              mMultipart;
    eContextType        mContextType;
    eAutoDetectResult   mAutoDetectStatus;
    eParserCommands     mParserCommand;   //tells us to viewcontent/viewsource/viewerrors...
    nsIChannel*         mChannel; // provided by necko to differnciate different input streams

    nsScanner*          mScanner;
    nsIDTD*             mDTD;
    nsIDTD*             mValidator;

    char*               mTransferBuffer;
    nsIStreamObserver*  mListener;

    void*               mKey;
    PRUint32            mTransferBufferSize;
    PRBool              mCopyUnused;
};



#endif


