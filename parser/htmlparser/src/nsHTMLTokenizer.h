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

#include "mozilla/Attributes.h"
#include "nsISupports.h"
#include "nsITokenizer.h"

#ifdef _MSC_VER
#pragma warning( disable : 4275 )
#endif

class nsHTMLTokenizer MOZ_FINAL : public nsITokenizer {
  ~nsHTMLTokenizer() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITOKENIZER
  nsHTMLTokenizer();
};

#endif


