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
#include "nsScanner.h"
#include "nsElementTable.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
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
 * 
 * @param  aParseMode The current mode the document is in (quirks, etc.)
 * @param  aDocType The document type of the current document
 * @param  aCommand What we are trying to do (view-source, parse a fragment, etc.)
 */
nsHTMLTokenizer::nsHTMLTokenizer(nsDTDMode aParseMode,
                                 eParserDocType aDocType,
                                 eParserCommands aCommand,
                                 PRUint32 aFlags)
  : mTokenDeque(0), mFlags(aFlags)
{
  if (aParseMode == eDTDMode_full_standards ||
      aParseMode == eDTDMode_almost_standards) {
    mFlags |= NS_IPARSER_FLAG_STRICT_MODE;
  } else if (aParseMode == eDTDMode_quirks)  {
    mFlags |= NS_IPARSER_FLAG_QUIRKS_MODE;
  } else if (aParseMode == eDTDMode_autodetect) {
    mFlags |= NS_IPARSER_FLAG_AUTO_DETECT_MODE;
  } else {
    mFlags |= NS_IPARSER_FLAG_UNKNOWN_MODE;
  }

  if (aDocType == ePlainText) {
    mFlags |= NS_IPARSER_FLAG_PLAIN_TEXT;
  } else if (aDocType == eXML) {
    mFlags |= NS_IPARSER_FLAG_XML;
  } else if (aDocType == eHTML_Quirks ||
             aDocType == eHTML_Strict) {
    mFlags |= NS_IPARSER_FLAG_HTML;
  }
  
  mFlags |= aCommand == eViewSource
            ? NS_IPARSER_FLAG_VIEW_SOURCE
            : NS_IPARSER_FLAG_VIEW_NORMAL;

  NS_ASSERTION(!(mFlags & NS_IPARSER_FLAG_XML) || 
                (mFlags & NS_IPARSER_FLAG_VIEW_SOURCE),
              "Why isn't this XML document going through our XML parser?");

  mTokenAllocator = nullptr;
  mTokenScanPos = 0;
}

/**
 * The destructor ensures that we don't leak any left over tokens.
 */
nsHTMLTokenizer::~nsHTMLTokenizer()
{
  if (mTokenDeque.GetSize()) {
    CTokenDeallocator theDeallocator(mTokenAllocator->GetArenaPool());
    mTokenDeque.ForEach(theDeallocator);
  }
}

/*static*/ PRUint32
nsHTMLTokenizer::GetFlags(const nsIContentSink* aSink)
{
  PRUint32 flags = 0;
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

/*******************************************************************
  Here begins the real working methods for the tokenizer.
 *******************************************************************/

/**
 * Adds a token onto the end of the deque if aResult is a successful result.
 * Otherwise, this function frees aToken and sets it to nullptr.
 *
 * @param aToken The token that wants to be added.
 * @param aResult The error code that will be used to determine if we actually
 *                want to push this token.
 * @param aDeque The deque we want to push aToken onto.
 * @param aTokenAllocator The allocator we use to free aToken in case aResult
 *                        is not a success code.
 */
/* static */
void
nsHTMLTokenizer::AddToken(CToken*& aToken,
                          nsresult aResult,
                          nsDeque* aDeque,
                          nsTokenAllocator* aTokenAllocator)
{
  if (aToken && aDeque) {
    if (NS_SUCCEEDED(aResult)) {
      aDeque->Push(aToken);
    } else {
      IF_FREE(aToken, aTokenAllocator);
    }
  }
}

/**
 * Retrieve a pointer to the global token recycler...
 *
 * @return Pointer to recycler (or null)
 */
nsTokenAllocator*
nsHTMLTokenizer::GetTokenAllocator()
{
  return mTokenAllocator;
}

/**
 * This method provides access to the topmost token in the tokenDeque.
 * The token is not really removed from the list.
 *
 * @return Pointer to token
 */
CToken*
nsHTMLTokenizer::PeekToken()
{
  return (CToken*)mTokenDeque.PeekFront();
}

/**
 * This method provides access to the topmost token in the tokenDeque.
 * The token is really removed from the list; if the list is empty we return 0.
 *
 * @return Pointer to token or NULL
 */
CToken*
nsHTMLTokenizer::PopToken()
{
  return (CToken*)mTokenDeque.PopFront();
}


/**
 * Pushes a token onto the front of our deque such that the next call to
 * PopToken() or PeekToken() will return that token.
 * 
 * @param theToken The next token to be processed
 * @return theToken
 */
CToken*
nsHTMLTokenizer::PushTokenFront(CToken* theToken)
{
  mTokenDeque.PushFront(theToken);
  return theToken;
}

/**
 * Pushes a token onto the deque.
 * 
 * @param theToken the new token.
 * @return theToken
 */
CToken*
nsHTMLTokenizer::PushToken(CToken* theToken)
{
  mTokenDeque.Push(theToken);
  return theToken;
}

/**
 * Returns the size of the deque.
 *
 * @return The number of remaining tokens.
 */
PRInt32
nsHTMLTokenizer::GetCount()
{
  return mTokenDeque.GetSize();
}

/**
 * Allows access to an arbitrary token in the deque. The accessed token is left
 * in the deque.
 *
 * @param anIndex The index of the target token. Token 0 would be the same as
 *                the result of a call to PeekToken()
 * @return The requested token.
 */
CToken*
nsHTMLTokenizer::GetTokenAt(PRInt32 anIndex)
{
  return (CToken*)mTokenDeque.ObjectAt(anIndex);
}

/**
 * This method is part of the "sandwich" that occurs when we want to tokenize
 * a document. This prepares us to be able to tokenize properly.
 *
 * @param aIsFinalChunk Whether this is the last chunk of data that we will
 *                      get to see.
 * @param aTokenAllocator The token allocator to use for this document.
 * @return Our success in setting up.
 */
nsresult
nsHTMLTokenizer::WillTokenize(bool aIsFinalChunk,
                              nsTokenAllocator* aTokenAllocator)
{
  mTokenAllocator = aTokenAllocator;
  mIsFinalChunk = aIsFinalChunk;

  // Cause ScanDocStructure to search from here for new tokens...
  mTokenScanPos = mTokenDeque.GetSize();
  return NS_OK;
}

/**
 * Pushes all of the tokens in aDeque onto the front of our deque so they
 * get processed before any other tokens.
 *
 * @param aDeque The deque with the tokens in it.
 */
void
nsHTMLTokenizer::PrependTokens(nsDeque& aDeque)
{
  PRInt32 aCount = aDeque.GetSize();
  
  for (PRInt32 anIndex = 0; anIndex < aCount; ++anIndex) {
    CToken* theToken = (CToken*)aDeque.Pop();
    PushTokenFront(theToken);
  }
}

/**
 * Copies the state flags from aTokenizer into this tokenizer. This is used
 * to pass information around between the main tokenizer and tokenizers
 * created for document.write() calls.
 *
 * @param aTokenizer The tokenizer with more information in it.
 * @return NS_OK
 */
nsresult
nsHTMLTokenizer::CopyState(nsITokenizer* aTokenizer)
{
  if (aTokenizer) {
    mFlags = ((nsHTMLTokenizer*)aTokenizer)->mFlags;
  }

  return NS_OK;
}

/**
 * This is a utilty method for ScanDocStructure, which finds a given
 * tag in the stack. The return value is meant to be used with
 * nsDeque::ObjectAt() on aTagStack.
 *
 * @param   aTag -- the ID of the tag we're seeking
 * @param   aTagStack -- the stack to be searched
 * @return  index position of tag in stack if found, otherwise kNotFound
 */
static PRInt32
FindLastIndexOfTag(eHTMLTags aTag, nsDeque &aTagStack)
{
  PRInt32 theCount = aTagStack.GetSize();
  
  while (0 < theCount) {
    CHTMLToken* theToken = (CHTMLToken*)aTagStack.ObjectAt(--theCount);  
    if (theToken) {
      eHTMLTags theTag = (eHTMLTags)theToken->GetTypeID();
      if (theTag == aTag) {
        return theCount;
      }
    }
  }

  return kNotFound;
}

/**
 * This method scans the sequence of tokens to determine whether or not the
 * tag structure of the document is well formed. In well formed cases, we can
 * skip doing residual style handling and allow inlines to contain block-level
 * elements.
 *
 * @param aFinalChunk Is unused.
 * @return Success (currently, this function cannot fail).
 */
nsresult nsHTMLTokenizer::ScanDocStructure(bool aFinalChunk)
{
  nsresult result = NS_OK;
  if (!mTokenDeque.GetSize()) {
    return result;
  }

  CHTMLToken* theToken = (CHTMLToken*)mTokenDeque.ObjectAt(mTokenScanPos);

  // Start by finding the first start tag that hasn't been reviewed.
  while (mTokenScanPos > 0) {
    if (theToken) {
      eHTMLTokenTypes theType = eHTMLTokenTypes(theToken->GetTokenType());  
      if (theType == eToken_start &&
          theToken->GetContainerInfo() == eFormUnknown) {
        break;
      }
    }
    theToken = (CHTMLToken*)mTokenDeque.ObjectAt(--mTokenScanPos);
  }

  // Now that we know where to start, let's walk through the
  // tokens to see which are well-formed. Stop when you run out
  // of fresh tokens.

  nsDeque       theStack(0);
  nsDeque       tempStack(0);
  PRInt32       theStackDepth = 0;
  // Don't bother if we get ridiculously deep.
  static  const PRInt32 theMaxStackDepth = 200;

  while (theToken && theStackDepth < theMaxStackDepth) {
    eHTMLTokenTypes theType = eHTMLTokenTypes(theToken->GetTokenType());
    eHTMLTags       theTag  = (eHTMLTags)theToken->GetTypeID();

    if (nsHTMLElement::IsContainer(theTag)) { // Bug 54117
      bool theTagIsBlock  = gHTMLElements[theTag].IsMemberOf(kBlockEntity);
      bool theTagIsInline = theTagIsBlock
                              ? false
                              : gHTMLElements[theTag].IsMemberOf(kInlineEntity);

      if (theTagIsBlock || theTagIsInline || eHTMLTag_table == theTag) {
        switch(theType) {
          case eToken_start:
            {
              if (gHTMLElements[theTag].ShouldVerifyHierarchy()) {
                PRInt32 earlyPos = FindLastIndexOfTag(theTag, theStack);
                if (earlyPos != kNotFound) {
                  // Uh-oh, we've found a tag that is not allowed to nest at
                  // all. Mark the previous one and all of its children as 
                  // malformed to increase our chances of doing RS handling
                  // on all of them. We want to do this for cases such as:
                  // <a><div><a></a></div></a>.
                  // Note that we have to iterate through all of the chilren
                  // of the original malformed tag to protect against:
                  // <a><font><div><a></a></div></font></a>, so that the <font>
                  // is allowed to contain the <div>.
                  // XXX What about <a><span><a>, where the second <a> closes
                  // the <span>?
                  nsDequeIterator it(theStack, earlyPos), end(theStack.End());
                  while (it < end) {
                    CHTMLToken *theMalformedToken = 
                        static_cast<CHTMLToken*>(it++);
                  
                    theMalformedToken->SetContainerInfo(eMalformed);
                  }
                }
              }

              theStack.Push(theToken);
              ++theStackDepth;
            }
            break;
          case eToken_end: 
            {
              CHTMLToken *theLastToken =
                static_cast<CHTMLToken*>(theStack.Peek());
              if (theLastToken) {
                if (theTag == theLastToken->GetTypeID()) {
                  theStack.Pop(); // Yank it for real 
                  theStackDepth--;
                  theLastToken->SetContainerInfo(eWellFormed);
                } else {
                  // This token wasn't what we expected it to be! We need to
                  // go searching for its real start tag on our stack. Each
                  // tag in between the end tag and start tag must be malformed

                  if (FindLastIndexOfTag(theTag, theStack) != kNotFound) {
                    // Find theTarget in the stack, marking each (malformed!)
                    // tag in our way.
                    theStack.Pop(); // Pop off theLastToken for real.
                    do {
                      theLastToken->SetContainerInfo(eMalformed);
                      tempStack.Push(theLastToken);
                      theLastToken = static_cast<CHTMLToken*>(theStack.Pop());
                    } while (theLastToken && theTag != theLastToken->GetTypeID());
                    // XXX The above test can confuse two different userdefined 
                    // tags.

                    NS_ASSERTION(theLastToken,
                                 "FindLastIndexOfTag lied to us!"
                                 " We couldn't find theTag on theStack");
                    theLastToken->SetContainerInfo(eMalformed);

                    // Great, now push all of the other tokens back onto the
                    // stack to preserve the general structure of the document.
                    // Note that we don't push the target token back onto the
                    // the stack (since it was just closed).
                    while (tempStack.GetSize() != 0) {
                      theStack.Push(tempStack.Pop());
                    }
                  }
                }
              }
            }
            break;
          default:
            break; 
        }
      }
    }

    theToken = (CHTMLToken*)mTokenDeque.ObjectAt(++mTokenScanPos);
  }

  return result;
}

/**
 * This method is called after we're done tokenizing a chunk of data.
 *
 * @param aFinalChunk Tells us if this was the last chunk of data.
 * @return Error result.
 */
nsresult
nsHTMLTokenizer::DidTokenize(bool aFinalChunk)
{
  return ScanDocStructure(aFinalChunk);
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
  PRUnichar theChar;
  CToken* theToken = nullptr;

  nsresult result = aScanner.Peek(theChar);

  switch(result) {
    case kEOF:
      // Tell our caller that'we finished.
      return result;

    case NS_OK:
    default:
      if (!(mFlags & NS_IPARSER_FLAG_PLAIN_TEXT)) {
        if (kLessThan == theChar) {
          return ConsumeTag(theChar, theToken, aScanner, aFlushTokens);
        } else if (kAmpersand == theChar) {
          return ConsumeEntity(theChar, theToken, aScanner);
        }
      }

      if (kCR == theChar || kLF == theChar) {
        return ConsumeNewline(theChar, theToken, aScanner);
      } else {
        if (!nsCRT::IsAsciiSpace(theChar)) {
          if (theChar != '\0') {
            result = ConsumeText(theToken, aScanner);
          } else {
            // Skip the embedded null char. Fix bug 64098.
            aScanner.GetChar(theChar);
          }
          break;
        }
        result = ConsumeWhitespace(theChar, theToken, aScanner);
      }
      break;
  }

  return result;
}

/**
 * This method is called just after a "<" has been consumed 
 * and we know we're at the start of some kind of tagged 
 * element. We don't know yet if it's a tag or a comment.
 * 
 * @param   aChar is the last char read
 * @param   aToken is the out arg holding our new token (the function allocates
 *                 the return token using mTokenAllocator).
 * @param   aScanner represents our input source
 * @param   aFlushTokens is an OUT parameter use to tell consumers to flush
 *                       the current tokens after processing the current one.
 * @return  error code.
 */
nsresult
nsHTMLTokenizer::ConsumeTag(PRUnichar aChar,
                            CToken*& aToken,
                            nsScanner& aScanner,
                            bool& aFlushTokens)
{
  PRUnichar theNextChar, oldChar;
  nsresult result = aScanner.Peek(aChar, 1);

  if (NS_OK == result) {
    switch (aChar) {
      case kForwardSlash:
        result = aScanner.Peek(theNextChar, 2);

        if (NS_OK == result) {
          // Get the original "<" (we've already seen it with a Peek)
          aScanner.GetChar(oldChar);

          // XML allows non ASCII tag names, consume this as an end tag. This
          // is needed to make XML view source work
          bool isXML = !!(mFlags & NS_IPARSER_FLAG_XML);
          if (nsCRT::IsAsciiAlpha(theNextChar) ||
              kGreaterThan == theNextChar      ||
              (isXML && !nsCRT::IsAscii(theNextChar))) {
            result = ConsumeEndTag(aChar, aToken, aScanner);
          } else {
            result = ConsumeComment(aChar, aToken, aScanner);
          }
        }

        break;

      case kExclamation:
        result = aScanner.Peek(theNextChar, 2);

        if (NS_OK == result) {
          // Get the original "<" (we've already seen it with a Peek)
          aScanner.GetChar(oldChar);

          if (kMinus == theNextChar || kGreaterThan == theNextChar) {
            result = ConsumeComment(aChar, aToken, aScanner);
          } else {
            result = ConsumeSpecialMarkup(aChar, aToken, aScanner);
          }
        }
        break;

      case kQuestionMark:
        // It must be a processing instruction...
        // Get the original "<" (we've already seen it with a Peek)
        aScanner.GetChar(oldChar);
        result = ConsumeProcessingInstruction(aChar, aToken, aScanner);
        break;

      default:
        // XML allows non ASCII tag names, consume this as a start tag.
        bool isXML = !!(mFlags & NS_IPARSER_FLAG_XML);
        if (nsCRT::IsAsciiAlpha(aChar) ||
            (isXML && !nsCRT::IsAscii(aChar))) {
          // Get the original "<" (we've already seen it with a Peek)
          aScanner.GetChar(oldChar);
          result = ConsumeStartTag(aChar, aToken, aScanner, aFlushTokens);
        } else {
          // We are not dealing with a tag. So, don't consume the original
          // char and leave the decision to ConsumeText().
          result = ConsumeText(aToken, aScanner);
        }
    }
  }

  // Last ditch attempt to make sure we don't lose data.
  if (kEOF == result && !aScanner.IsIncremental()) {
    // Whoops, we don't want to lose any data! Consume the rest as text.
    // This normally happens for either a trailing < or </
    result = ConsumeText(aToken, aScanner);
  }

  return result;
}

/**
 * This method is called just after we've consumed a start or end
 * tag, and we now have to consume its attributes.
 * 
 * @param   aChar is the last char read
 * @param   aToken is the start or end tag that "owns" these attributes.
 * @param   aScanner represents our input source
 * @return  Error result.
 */
nsresult
nsHTMLTokenizer::ConsumeAttributes(PRUnichar aChar,
                                   CToken* aToken,
                                   nsScanner& aScanner)
{
  bool done = false;
  nsresult result = NS_OK;
  PRInt16 theAttrCount = 0;

  nsTokenAllocator* theAllocator = this->GetTokenAllocator();

  while (!done && result == NS_OK) {
    CAttributeToken* theToken =
      static_cast<CAttributeToken*>
                 (theAllocator->CreateTokenOfType(eToken_attribute,
                                                     eHTMLTag_unknown));
    if (NS_LIKELY(theToken != nullptr)) {
      // Tell the new token to finish consuming text...
      result = theToken->Consume(aChar, aScanner, mFlags);

      if (NS_SUCCEEDED(result)) {
        ++theAttrCount;
        AddToken((CToken*&)theToken, result, &mTokenDeque, theAllocator);
      } else {
        IF_FREE(theToken, mTokenAllocator);
        // Bad attribute returns shouldn't propagate out.
        if (NS_ERROR_HTMLPARSER_BADATTRIBUTE == result) {
          result = NS_OK;
        }
      }
    }
    else {
      result = NS_ERROR_OUT_OF_MEMORY;
    }

#ifdef DEBUG
    if (NS_SUCCEEDED(result)) {
      PRInt32 newline = 0;
      aScanner.SkipWhitespace(newline);
      NS_ASSERTION(newline == 0,
          "CAttribute::Consume() failed to collect all the newlines!");
    }
#endif
    if (NS_SUCCEEDED(result)) {
      result = aScanner.Peek(aChar);
      if (NS_SUCCEEDED(result)) {
        if (aChar == kGreaterThan) { // You just ate the '>'
          aScanner.GetChar(aChar); // Skip the '>'
          done = true;
        } else if (aChar == kLessThan) {
          aToken->SetInError(true);
          done = true;
        }
      }
    }
  }

  if (NS_FAILED(result)) {
    aToken->SetInError(true);

    if (!aScanner.IsIncremental()) {
      result = NS_OK;
    }
  }

  aToken->SetAttributeCount(theAttrCount);
  return result;
}

/**
 * This method consumes a start tag and all of its attributes.
 *
 * @param aChar The last character read from the scanner.
 * @param aToken The OUT parameter that holds our resulting token. (allocated
 *               by the function using mTokenAllocator
 * @param aScanner Our source of data
 * @param aFlushTokens is an OUT parameter use to tell consumers to flush
 *                     the current tokens after processing the current one.
 * @return Error result.
 */
nsresult
nsHTMLTokenizer::ConsumeStartTag(PRUnichar aChar,
                                 CToken*& aToken,
                                 nsScanner& aScanner,
                                 bool& aFlushTokens)
{
  // Remember this for later in case you have to unwind...
  PRInt32 theDequeSize = mTokenDeque.GetSize();
  nsresult result = NS_OK;

  nsTokenAllocator* theAllocator = this->GetTokenAllocator();
  aToken = theAllocator->CreateTokenOfType(eToken_start, eHTMLTag_unknown);
  NS_ENSURE_TRUE(aToken, NS_ERROR_OUT_OF_MEMORY);

  // Tell the new token to finish consuming text...
  result = aToken->Consume(aChar, aScanner, mFlags);

  if (NS_SUCCEEDED(result)) {
    AddToken(aToken, result, &mTokenDeque, theAllocator);

    eHTMLTags theTag = (eHTMLTags)aToken->GetTypeID();

    // Good. Now, let's see if the next char is ">".
    // If so, we have a complete tag, otherwise, we have attributes.
    result = aScanner.Peek(aChar);
    if (NS_FAILED(result)) {
      aToken->SetInError(true);

      // Don't return early here so we can create a text and end token for
      // the special <iframe>, <script> and similar tags down below.
      result = NS_OK;
    } else {
      if (kGreaterThan != aChar) { // Look for a '>'
        result = ConsumeAttributes(aChar, aToken, aScanner);
      } else {
        aScanner.GetChar(aChar);
      }
    }

    /*  Now that that's over with, we have one more problem to solve.
        In the case that we just read a <SCRIPT> or <STYLE> tags, we should go and
        consume all the content itself.
        But XML doesn't treat these tags differently, so we shouldn't if the
        document is XML.
     */
    if (NS_SUCCEEDED(result) && !(mFlags & NS_IPARSER_FLAG_XML)) {
      bool isCDATA = gHTMLElements[theTag].CanContainType(kCDATA);
      bool isPCDATA = eHTMLTag_textarea == theTag ||
                        eHTMLTag_title    == theTag;

      // XXX This is an evil hack, we should be able to handle these properly
      // in the DTD.
      if ((eHTMLTag_iframe == theTag &&
            (mFlags & NS_IPARSER_FLAG_FRAMES_ENABLED)) ||
          (eHTMLTag_noframes == theTag &&
            (mFlags & NS_IPARSER_FLAG_FRAMES_ENABLED)) ||
          (eHTMLTag_noscript == theTag &&
            (mFlags & NS_IPARSER_FLAG_SCRIPT_ENABLED)) ||
          (eHTMLTag_noembed == theTag)) {
        isCDATA = true;
      }

      // Plaintext contains CDATA, but it's special, so we handle it
      // differently than the other CDATA elements
      if (eHTMLTag_plaintext == theTag) {
        isCDATA = false;

        // Note: We check in ConsumeToken() for this flag, and if we see it
        // we only construct text tokens (which is what we want).
        mFlags |= NS_IPARSER_FLAG_PLAIN_TEXT;
      }


      if (isCDATA || isPCDATA) {
        bool done = false;
        nsDependentString endTagName(nsHTMLTags::GetStringValue(theTag)); 

        CToken* text =
            theAllocator->CreateTokenOfType(eToken_text, eHTMLTag_text);
        NS_ENSURE_TRUE(text, NS_ERROR_OUT_OF_MEMORY);

        CTextToken* textToken = static_cast<CTextToken*>(text);

        if (isCDATA) {
          result = textToken->ConsumeCharacterData(theTag != eHTMLTag_script,
                                                   aScanner,
                                                   endTagName,
                                                   mFlags,
                                                   done);

          // Only flush tokens for <script>, to give ourselves more of a
          // chance of allowing inlines to contain blocks.
          aFlushTokens = done && theTag == eHTMLTag_script;
        } else if (isPCDATA) {
          // Title is consumed conservatively in order to not regress
          // bug 42945
          result = textToken->ConsumeParsedCharacterData(
                                                  theTag == eHTMLTag_textarea,
                                                  theTag == eHTMLTag_title,
                                                  aScanner,
                                                  endTagName,
                                                  mFlags,
                                                  done);

          // Note: we *don't* set aFlushTokens here.
        }

        // We want to do this unless result is kEOF, in which case we will
        // simply unwind our stack and wait for more data anyway.
        if (kEOF != result) {
          AddToken(text, NS_OK, &mTokenDeque, theAllocator);
          CToken* endToken = nullptr;

          if (NS_SUCCEEDED(result) && done) {
            PRUnichar theChar;
            // Get the <
            result = aScanner.GetChar(theChar);
            NS_ASSERTION(NS_SUCCEEDED(result) && theChar == kLessThan,
                         "CTextToken::Consume*Data is broken!");
#ifdef DEBUG
            // Ensure we have a /
            PRUnichar tempChar;  // Don't change non-debug vars in debug-only code
            result = aScanner.Peek(tempChar);
            NS_ASSERTION(NS_SUCCEEDED(result) && tempChar == kForwardSlash,
                         "CTextToken::Consume*Data is broken!");
#endif
            result = ConsumeEndTag(PRUnichar('/'), endToken, aScanner);
            if (!(mFlags & NS_IPARSER_FLAG_VIEW_SOURCE) &&
                NS_SUCCEEDED(result)) {
              // If ConsumeCharacterData returned a success result (and
              // we're not in view source), then we want to make sure that
              // we're going to execute this script (since the result means
              // that we've found an end tag that satisfies all of the right
              // conditions).
              endToken->SetInError(false);
            }
          } else if (result == kFakeEndTag &&
                    !(mFlags & NS_IPARSER_FLAG_VIEW_SOURCE)) {
            result = NS_OK;
            endToken = theAllocator->CreateTokenOfType(eToken_end, theTag,
                                                       endTagName);
            AddToken(endToken, result, &mTokenDeque, theAllocator);
            if (NS_LIKELY(endToken != nullptr)) {
              endToken->SetInError(true);
            }
            else {
              result = NS_ERROR_OUT_OF_MEMORY;
            }
          } else if (result == kFakeEndTag) {
            // If we are here, we are both faking having seen the end tag
            // and are in view-source.
            result = NS_OK;
          }
        } else {
          IF_FREE(text, mTokenAllocator);
        }
      }
    }

    // This code is confusing, so pay attention.
    // If you're here, it's because we were in the midst of consuming a start
    // tag but ran out of data (not in the stream, but in this *part* of the
    // stream. For simplicity, we have to unwind our input. Therefore, we pop
    // and discard any new tokens we've queued this round. Later we can get
    // smarter about this.
    if (NS_FAILED(result)) {
      while (mTokenDeque.GetSize()>theDequeSize) {
        CToken* theToken = (CToken*)mTokenDeque.Pop();
        IF_FREE(theToken, mTokenAllocator);
      }
    }
  } else {
    IF_FREE(aToken, mTokenAllocator);
  }

  return result;
}

/**
 * This method consumes an end tag and any "attributes" that may come after it.
 *
 * @param aChar The last character read from the scanner.
 * @param aToken The OUT parameter that holds our resulting token.
 * @param aScanner Our source of data
 * @return Error result
 */
nsresult
nsHTMLTokenizer::ConsumeEndTag(PRUnichar aChar,
                               CToken*& aToken,
                               nsScanner& aScanner)
{
  // Get the "/" (we've already seen it with a Peek)
  aScanner.GetChar(aChar);

  nsTokenAllocator* theAllocator = this->GetTokenAllocator();
  aToken = theAllocator->CreateTokenOfType(eToken_end, eHTMLTag_unknown);
  NS_ENSURE_TRUE(aToken, NS_ERROR_OUT_OF_MEMORY);

  // Remember this for later in case you have to unwind...
  PRInt32 theDequeSize = mTokenDeque.GetSize();
  nsresult result = NS_OK;

  // Tell the new token to finish consuming text...
  result = aToken->Consume(aChar, aScanner, mFlags);
  AddToken(aToken, result, &mTokenDeque, theAllocator);
  if (NS_FAILED(result)) {
    // Note that this early-return here is safe because we have not yet
    // added any of our tokens to the queue (AddToken only adds the token if
    // result is a success), so we don't need to fall through.
    return result;
  }

  result = aScanner.Peek(aChar);
  if (NS_FAILED(result)) {
    aToken->SetInError(true);

    // Note: We know here that the scanner is not incremental since if
    // this peek fails, then we've already masked over a kEOF coming from
    // the Consume() call above.
    return NS_OK;
  }

  if (kGreaterThan != aChar) {
    result = ConsumeAttributes(aChar, aToken, aScanner);
  } else {
    aScanner.GetChar(aChar);
  }

  // Do the same thing as we do in ConsumeStartTag. Basically, if we've run
  // out of room in this *section* of the document, pop all of the tokens
  // we've consumed this round and wait for more data.
  if (NS_FAILED(result)) {
    while (mTokenDeque.GetSize() > theDequeSize) {
      CToken* theToken = (CToken*)mTokenDeque.Pop();
      IF_FREE(theToken, mTokenAllocator);
    }
  }

  return result;
}

/**
 *  This method is called just after a "&" has been consumed 
 *  and we know we're at the start of an entity.  
 *  
 * @param aChar The last character read from the scanner.
 * @param aToken The OUT parameter that holds our resulting token.
 * @param aScanner Our source of data
 * @return Error result. 
 */
nsresult
nsHTMLTokenizer::ConsumeEntity(PRUnichar aChar,
                               CToken*& aToken,
                               nsScanner& aScanner)
{
  PRUnichar  theChar;
  nsresult result = aScanner.Peek(theChar, 1);

  nsTokenAllocator* theAllocator = this->GetTokenAllocator();
  if (NS_SUCCEEDED(result)) {
    if (nsCRT::IsAsciiAlpha(theChar) || theChar == kHashsign) {
      aToken = theAllocator->CreateTokenOfType(eToken_entity, eHTMLTag_entity);
      NS_ENSURE_TRUE(aToken, NS_ERROR_OUT_OF_MEMORY);
      result = aToken->Consume(theChar, aScanner, mFlags);

      if (result == NS_HTMLTOKENS_NOT_AN_ENTITY) {
        IF_FREE(aToken, mTokenAllocator);
      } else {
        if (result == kEOF && !aScanner.IsIncremental()) {
          result = NS_OK; // Use as much of the entity as you can get.
        }

        AddToken(aToken, result, &mTokenDeque, theAllocator);
        return result;
      }
    }

    // Oops, we're actually looking at plain text...
    result = ConsumeText(aToken, aScanner);
  } else if (result == kEOF && !aScanner.IsIncremental()) {
    // If the last character in the file is an &, consume it as text.
    result = ConsumeText(aToken, aScanner);
    if (aToken) {
      aToken->SetInError(true);
    }
  }

  return result;
}


/**
 *  This method is called just after whitespace has been 
 *  consumed and we know we're at the start a whitespace run.  
 *  
 * @param aChar The last character read from the scanner.
 * @param aToken The OUT parameter that holds our resulting token.
 * @param aScanner Our source of data
 * @return Error result.
 */
nsresult
nsHTMLTokenizer::ConsumeWhitespace(PRUnichar aChar,
                                   CToken*& aToken,
                                   nsScanner& aScanner)
{
  // Get the whitespace character
  aScanner.GetChar(aChar);

  nsTokenAllocator* theAllocator = this->GetTokenAllocator();
  aToken = theAllocator->CreateTokenOfType(eToken_whitespace,
                                           eHTMLTag_whitespace);
  nsresult result = NS_OK;
  if (aToken) {
    result = aToken->Consume(aChar, aScanner, mFlags);
    AddToken(aToken, result, &mTokenDeque, theAllocator);
  }

  return result;
}

/**
 *  This method is called just after a "<!" has been consumed 
 *  and we know we're at the start of a comment.  
 *  
 * @param aChar The last character read from the scanner.
 * @param aToken The OUT parameter that holds our resulting token.
 * @param aScanner Our source of data
 * @return Error result.
 */
nsresult
nsHTMLTokenizer::ConsumeComment(PRUnichar aChar,
                                CToken*& aToken,
                                nsScanner& aScanner)
{
  // Get the "!"
  aScanner.GetChar(aChar);

  nsTokenAllocator* theAllocator = this->GetTokenAllocator();
  aToken = theAllocator->CreateTokenOfType(eToken_comment, eHTMLTag_comment);
  nsresult result = NS_OK;
  if (aToken) {
    result = aToken->Consume(aChar, aScanner, mFlags);
    AddToken(aToken, result, &mTokenDeque, theAllocator);
  }

  if (kNotAComment == result) {
    // AddToken has IF_FREE()'d our token, so...
    result = ConsumeText(aToken, aScanner);
  }

  return result;
}

/**
 * This method is called just after a known text char has
 * been consumed and we should read a text run. Note: we actually ignore the
 * first character of the text run so that we can consume invalid markup 
 * as text.
 *  
 * @param aToken The OUT parameter that holds our resulting token.
 * @param aScanner Our source of data
 * @return Error result.
 */ 
nsresult
nsHTMLTokenizer::ConsumeText(CToken*& aToken, nsScanner& aScanner)
{
  nsresult result = NS_OK;
  nsTokenAllocator* theAllocator = this->GetTokenAllocator();
  CTextToken* theToken =
    (CTextToken*)theAllocator->CreateTokenOfType(eToken_text, eHTMLTag_text);
  if (theToken) {
    PRUnichar ch = '\0';
    result = theToken->Consume(ch, aScanner, mFlags);
    if (NS_FAILED(result)) {
      if (0 == theToken->GetTextLength()) {
        IF_FREE(aToken, mTokenAllocator);
        aToken = nullptr;
      } else {
        result = NS_OK;
      }
    }

    aToken = theToken;
    AddToken(aToken, result, &mTokenDeque, theAllocator);
  }

  return result;
}

/**
 * This method is called just after a "<!" has been consumed.
 * NOTE: Here we might consume DOCTYPE and "special" markups. 
 * 
 * @param aChar The last character read from the scanner.
 * @param aToken The OUT parameter that holds our resulting token.
 * @param aScanner Our source of data
 * @return Error result.
 */
nsresult
nsHTMLTokenizer::ConsumeSpecialMarkup(PRUnichar aChar,
                                      CToken*& aToken,
                                      nsScanner& aScanner)
{
  // Get the "!"
  aScanner.GetChar(aChar);

  nsresult result = NS_OK;
  nsAutoString theBufCopy;
  aScanner.Peek(theBufCopy, 20);
  ToUpperCase(theBufCopy);
  PRInt32 theIndex = theBufCopy.Find("DOCTYPE", false, 0, 0);
  nsTokenAllocator* theAllocator = this->GetTokenAllocator();

  if (theIndex == kNotFound) {
    if ('[' == theBufCopy.CharAt(0)) {
      aToken = theAllocator->CreateTokenOfType(eToken_cdatasection,
                                               eHTMLTag_comment);
    } else if (StringBeginsWith(theBufCopy, NS_LITERAL_STRING("ELEMENT")) ||
               StringBeginsWith(theBufCopy, NS_LITERAL_STRING("ATTLIST")) ||
               StringBeginsWith(theBufCopy, NS_LITERAL_STRING("ENTITY"))  ||
               StringBeginsWith(theBufCopy, NS_LITERAL_STRING("NOTATION"))) {
      aToken = theAllocator->CreateTokenOfType(eToken_markupDecl,
                                               eHTMLTag_markupDecl);
    } else {
      aToken = theAllocator->CreateTokenOfType(eToken_comment,
                                               eHTMLTag_comment);
    }
  } else {
    aToken = theAllocator->CreateTokenOfType(eToken_doctypeDecl,
                                             eHTMLTag_doctypeDecl);
  }

  if (aToken) {
    result = aToken->Consume(aChar, aScanner, mFlags);
    AddToken(aToken, result, &mTokenDeque, theAllocator);
  }

  if (result == kNotAComment) {
    result = ConsumeText(aToken, aScanner);
  }

  return result;
}

/**
 * This method is called just after a newline has been consumed. 
 *  
 * @param aChar The last character read from the scanner.
 * @param aToken The OUT parameter that holds our resulting token.
 * @param aScanner Our source of data
 * @return Error result.
 */
nsresult
nsHTMLTokenizer::ConsumeNewline(PRUnichar aChar,
                                CToken*& aToken,
                                nsScanner& aScanner)
{
  // Get the newline character
  aScanner.GetChar(aChar);

  nsTokenAllocator* theAllocator = this->GetTokenAllocator();
  aToken = theAllocator->CreateTokenOfType(eToken_newline, eHTMLTag_newline);
  nsresult result = NS_OK;
  if (aToken) {
    result = aToken->Consume(aChar, aScanner, mFlags);
    AddToken(aToken, result, &mTokenDeque, theAllocator);
  }

  return result;
}


/**
 * This method is called just after a <? has been consumed. 
 *  
 * @param aChar The last character read from the scanner.
 * @param aToken The OUT parameter that holds our resulting token.
 * @param aScanner Our source of data
 * @return Error result.
 */
nsresult
nsHTMLTokenizer::ConsumeProcessingInstruction(PRUnichar aChar,
                                              CToken*& aToken,
                                              nsScanner& aScanner)
{
  // Get the "?"
  aScanner.GetChar(aChar);

  nsTokenAllocator* theAllocator = this->GetTokenAllocator();
  aToken = theAllocator->CreateTokenOfType(eToken_instruction,
                                           eHTMLTag_unknown);
  nsresult result = NS_OK;
  if (aToken) {
    result = aToken->Consume(aChar, aScanner, mFlags);
    AddToken(aToken, result, &mTokenDeque, theAllocator);
  }

  return result;
}
