/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsIStreamListener.h"
#include "nsIRequest.h"
#include "nsScanner.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

/**
 * Note that the parser is given FULL access to all
 * data in a parsercontext. Hey, that what it's for!
 */

class CParserContext {
public:
   enum eContextType {eCTNone,eCTURL,eCTString,eCTStream};

   CParserContext(CParserContext* aPrevContext,
                  nsScanner* aScanner,
                  void* aKey = 0,
                  eParserCommands aCommand = eViewNormal,
                  nsIRequestObserver* aListener = 0,
                  eAutoDetectResult aStatus = eUnknownDetect,
                  bool aCopyUnused = false);

    ~CParserContext();

    nsresult GetTokenizer(nsIDTD* aDTD,
                          nsIContentSink* aSink,
                          nsITokenizer*& aTokenizer);
    void  SetMimeType(const nsACString& aMimeType);

    nsCOMPtr<nsIRequest> mRequest; // provided by necko to differnciate different input streams
                                   // why is mRequest strongly referenced? see bug 102376.
    nsCOMPtr<nsIRequestObserver> mListener;
    void* const          mKey;
    nsCOMPtr<nsITokenizer> mTokenizer;
    CParserContext* const mPrevContext;
    nsAutoPtr<nsScanner> mScanner;

    nsCString            mMimeType;
    nsDTDMode            mDTDMode;

    eParserDocType       mDocType;
    eStreamState         mStreamListenerState;
    eContextType         mContextType;
    eAutoDetectResult    mAutoDetectStatus;
    eParserCommands      mParserCommand;

    bool                 mMultipart;
    bool                 mCopyUnused;
};

#endif
