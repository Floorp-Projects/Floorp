/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */

#ifndef __NSHTMLTOKENIZER
#define __NSHTMLTOKENIZER

#include "nsISupports.h"
#include "nsITokenizer.h"
#include "nsIDTD.h"
#include "prtypes.h"
#include "nsDeque.h"
#include "nsScanner.h"

#ifdef _MSC_VER
#pragma warning( disable : 4275 )
#endif

class nsHTMLTokenizer : public nsITokenizer {
public:
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSITOKENIZER
  nsHTMLTokenizer(nsDTDMode aParseMode = eDTDMode_quirks,
                  eParserDocType aDocType = eHTML_Quirks,
                  eParserCommands aCommand = eViewNormal,
                  uint32_t aFlags = 0);
  virtual ~nsHTMLTokenizer();

  static uint32_t GetFlags(const nsIContentSink* aSink);
};

#endif


