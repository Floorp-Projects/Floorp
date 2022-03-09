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

#include "mozilla/UniquePtr.h"
#include "nsIParser.h"
#include "nsIDTD.h"
#include "nsIRequest.h"
#include "nsScanner.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsITokenizer;

/**
 * Note that the parser is given FULL access to all
 * data in a parsercontext. Hey, that what it's for!
 */

class CParserContext {
 public:
  enum eContextType { eCTURL, eCTString };

  CParserContext(nsIURI* aURI, eParserCommands aCommand);
  CParserContext(const nsAString& aBuffer, eParserCommands aCommand,
                 bool aLastBuffer);

  ~CParserContext();

  void SetMimeType(const nsACString& aMimeType);

  nsCOMPtr<nsIRequest>
      mRequest;  // provided by necko to differnciate different input streams
                 // why is mRequest strongly referenced? see bug 102376.
  nsScanner mScanner;

  nsCString mMimeType;
  nsDTDMode mDTDMode;

  eParserDocType mDocType;
  eStreamState mStreamListenerState;
  eContextType mContextType;
  eAutoDetectResult mAutoDetectStatus = eUnknownDetect;
  eParserCommands mParserCommand;

  bool mMultipart;
  bool mCopyUnused;
};

#endif
