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

#include "nsIAtom.h"
#include "nsHTMLTokenizer.h"
#include "nsParserConstants.h"
#include "nsIHTMLContentSink.h"

/************************************************************************
  And now for the main class -- nsHTMLTokenizer...
 ************************************************************************/

/**
 * Satisfy the nsISupports interface.
 */
NS_IMPL_ISUPPORTS1(nsHTMLTokenizer, nsITokenizer)

/**
 * Default constructor
 * 
 * @param  aParseMode The current mode the document is in (quirks, etc.)
 * @param  aDocType The document type of the current document
 * @param  aCommand What we are trying to do (view-source, parse a fragment, etc.)
 */
nsHTMLTokenizer::nsHTMLTokenizer(nsDTDMode aParseMode,
                                 eParserDocType aDocType,
                                 eParserCommands aCommand,
                                 uint32_t aFlags)
{
  // TODO Assert about:blank-ness.
  MOZ_ASSERT(!(aFlags & NS_IPARSER_FLAG_XML));
}

/**
 * The destructor ensures that we don't leak any left over tokens.
 */
nsHTMLTokenizer::~nsHTMLTokenizer()
{
}

/*static*/ uint32_t
nsHTMLTokenizer::GetFlags(const nsIContentSink* aSink)
{
  uint32_t flags = 0;
  nsCOMPtr<nsIHTMLContentSink> sink =
    do_QueryInterface(const_cast<nsIContentSink*>(aSink));
  if (sink) {
    bool enabled = true;
    sink->IsEnabled(eHTMLTag_frameset, &enabled);
    if (enabled) {
      flags |= NS_IPARSER_FLAG_FRAMES_ENABLED;
    }
    sink->IsEnabled(eHTMLTag_script, &enabled);
    if (enabled) {
      flags |= NS_IPARSER_FLAG_SCRIPT_ENABLED;
    }
  }
  return flags;
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
