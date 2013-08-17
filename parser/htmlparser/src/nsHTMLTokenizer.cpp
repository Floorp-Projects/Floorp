/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * @file nsHTMLTokenizer.cpp
 * This is an implementation of the nsITokenizer interface.
 * This file contains the implementation of a tokenizer to tokenize an HTML
 * document. It attempts to do so, making tradeoffs between compatibility with
 * older parsers and the SGML specification. Note that most of the real
 * "tokenization" takes place in nsHTMLTokens.cpp.
 */

#include "nsHTMLTokenizer.h"
#include "nsIParser.h"
#include "nsParserConstants.h"

/************************************************************************
  And now for the main class -- nsHTMLTokenizer...
 ************************************************************************/

/**
 * Satisfy the nsISupports interface.
 */
NS_IMPL_ISUPPORTS1(nsHTMLTokenizer, nsITokenizer)

/**
 * Default constructor
 */
nsHTMLTokenizer::nsHTMLTokenizer()
{
  // TODO Assert about:blank-ness.
}

nsresult
nsHTMLTokenizer::WillTokenize(bool aIsFinalChunk)
{
  return NS_OK;
}

/**
 * This method is repeatedly called by the tokenizer. 
 * Each time, we determine the kind of token we're about to 
 * read, and then we call the appropriate method to handle
 * that token type.
 *  
 * @param  aScanner The source of our input.
 * @param  aFlushTokens An OUT parameter to tell the caller whether it should
 *                      process our queued tokens up to now (e.g., when we
 *                      reach a <script>).
 * @return Success or error
 */
nsresult
nsHTMLTokenizer::ConsumeToken(nsScanner& aScanner, bool& aFlushTokens)
{
  return kEOF;
}
