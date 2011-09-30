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

    PRUint32             mNumConsumed;
};

#endif
