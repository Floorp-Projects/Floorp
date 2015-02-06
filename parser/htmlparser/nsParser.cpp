/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAtom.h"
#include "nsParser.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsScanner.h"
#include "plstr.h"
#include "nsIStringStream.h"
#include "nsIChannel.h"
#include "nsICachingChannel.h"
#include "nsIInputStream.h"
#include "CNavDTD.h"
#include "prenv.h"
#include "prlock.h"
#include "prcvar.h"
#include "nsParserCIID.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsExpatDriver.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIFragmentContentSink.h"
#include "nsStreamUtils.h"
#include "nsHTMLTokenizer.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsDataHashtable.h"
#include "nsXPCOMCIDInternal.h"
#include "nsMimeTypes.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "nsParserConstants.h"
#include "nsCharsetSource.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsIHTMLContentSink.h"

#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/BinarySearch.h"

using namespace mozilla;
using mozilla::dom::EncodingUtils;

#define NS_PARSER_FLAG_PARSER_ENABLED         0x00000002
#define NS_PARSER_FLAG_OBSERVERS_ENABLED      0x00000004
#define NS_PARSER_FLAG_PENDING_CONTINUE_EVENT 0x00000008
#define NS_PARSER_FLAG_FLUSH_TOKENS           0x00000020
#define NS_PARSER_FLAG_CAN_TOKENIZE           0x00000040

//-------------- Begin ParseContinue Event Definition ------------------------
/*
The parser can be explicitly interrupted by passing a return value of
NS_ERROR_HTMLPARSER_INTERRUPTED from BuildModel on the DTD. This will cause
the parser to stop processing and allow the application to return to the event
loop. The data which was left at the time of interruption will be processed
the next time OnDataAvailable is called. If the parser has received its final
chunk of data then OnDataAvailable will no longer be called by the networking
module, so the parser will schedule a nsParserContinueEvent which will call
the parser to process the remaining data after returning to the event loop.
If the parser is interrupted while processing the remaining data it will
schedule another ParseContinueEvent. The processing of data followed by
scheduling of the continue events will proceed until either:

  1) All of the remaining data can be processed without interrupting
  2) The parser has been cancelled.


This capability is currently used in CNavDTD and nsHTMLContentSink. The
nsHTMLContentSink is notified by CNavDTD when a chunk of tokens is going to be
processed and when each token is processed. The nsHTML content sink records
the time when the chunk has started processing and will return
NS_ERROR_HTMLPARSER_INTERRUPTED if the token processing time has exceeded a
threshold called max tokenizing processing time. This allows the content sink
to limit how much data is processed in a single chunk which in turn gates how
much time is spent away from the event loop. Processing smaller chunks of data
also reduces the time spent in subsequent reflows.

This capability is most apparent when loading large documents. If the maximum
token processing time is set small enough the application will remain
responsive during document load.

A side-effect of this capability is that document load is not complete when
the last chunk of data is passed to OnDataAvailable since  the parser may have
been interrupted when the last chunk of data arrived. The document is complete
when all of the document has been tokenized and there aren't any pending
nsParserContinueEvents. This can cause problems if the application assumes
that it can monitor the load requests to determine when the document load has
been completed. This is what happens in Mozilla. The document is considered
completely loaded when all of the load requests have been satisfied. To delay
the document load until all of the parsing has been completed the
nsHTMLContentSink adds a dummy parser load request which is not removed until
the nsHTMLContentSink's DidBuildModel is called. The CNavDTD will not call
DidBuildModel until the final chunk of data has been passed to the parser
through the OnDataAvailable and there aren't any pending
nsParserContineEvents.

Currently the parser is ignores requests to be interrupted during the
processing of script.  This is because a document.write followed by JavaScript
calls to manipulate the DOM may fail if the parser was interrupted during the
document.write.

For more details @see bugzilla bug 76722
*/


class nsParserContinueEvent : public nsRunnable
{
public:
  nsRefPtr<nsParser> mParser;

  explicit nsParserContinueEvent(nsParser* aParser)
    : mParser(aParser)
  {}

  NS_IMETHOD Run()
  {
    mParser->HandleParserContinueEvent(this);
    return NS_OK;
  }
};

//-------------- End ParseContinue Event Definition ------------------------

/**
 *  default constructor
 */
nsParser::nsParser()
{
  Initialize(true);
}

nsParser::~nsParser()
{
  Cleanup();
}

void
nsParser::Initialize(bool aConstructor)
{
  if (aConstructor) {
    // Raw pointer
    mParserContext = 0;
  }
  else {
    // nsCOMPtrs
    mObserver = nullptr;
    mUnusedInput.Truncate();
  }

  mContinueEvent = nullptr;
  mCharsetSource = kCharsetUninitialized;
  mCharset.AssignLiteral("ISO-8859-1");
  mInternalState = NS_OK;
  mStreamStatus = NS_OK;
  mCommand = eViewNormal;
  mFlags = NS_PARSER_FLAG_OBSERVERS_ENABLED |
           NS_PARSER_FLAG_PARSER_ENABLED |
           NS_PARSER_FLAG_CAN_TOKENIZE;

  mProcessingNetworkData = false;
  mIsAboutBlank = false;
}

void
nsParser::Cleanup()
{
#ifdef DEBUG
  if (mParserContext && mParserContext->mPrevContext) {
    NS_WARNING("Extra parser contexts still on the parser stack");
  }
#endif

  while (mParserContext) {
    CParserContext *pc = mParserContext->mPrevContext;
    delete mParserContext;
    mParserContext = pc;
  }

  // It should not be possible for this flag to be set when we are getting
  // destroyed since this flag implies a pending nsParserContinueEvent, which
  // has an owning reference to |this|.
  NS_ASSERTION(!(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT), "bad");
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsParser)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsParser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDTD)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mObserver)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDTD)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObserver)
  CParserContext *pc = tmp->mParserContext;
  while (pc) {
    cb.NoteXPCOMChild(pc->mTokenizer);
    pc = pc->mPrevContext;
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsParser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsParser)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsParser)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIParser)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIParser)
NS_INTERFACE_MAP_END

// The parser continue event is posted only if
// all of the data to parse has been passed to ::OnDataAvailable
// and the parser has been interrupted by the content sink
// because the processing of tokens took too long.

nsresult
nsParser::PostContinueEvent()
{
  if (!(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT)) {
    // If this flag isn't set, then there shouldn't be a live continue event!
    NS_ASSERTION(!mContinueEvent, "bad");

    // This creates a reference cycle between this and the event that is
    // broken when the event fires.
    nsCOMPtr<nsIRunnable> event = new nsParserContinueEvent(this);
    if (NS_FAILED(NS_DispatchToCurrentThread(event))) {
        NS_WARNING("failed to dispatch parser continuation event");
    } else {
        mFlags |= NS_PARSER_FLAG_PENDING_CONTINUE_EVENT;
        mContinueEvent = event;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsParser::GetCommand(nsCString& aCommand)
{
  aCommand = mCommandStr;
}

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about the command which caused the parser to be constructed. For example,
 *  this allows us to select a DTD which can do, say, view-source.
 *
 *  @param   aCommand the command string to set
 */
NS_IMETHODIMP_(void)
nsParser::SetCommand(const char* aCommand)
{
  mCommandStr.Assign(aCommand);
  if (mCommandStr.EqualsLiteral("view-source")) {
    mCommand = eViewSource;
  } else if (mCommandStr.EqualsLiteral("view-fragment")) {
    mCommand = eViewFragment;
  } else {
    mCommand = eViewNormal;
  }
}

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about the command which caused the parser to be constructed. For example,
 *  this allows us to select a DTD which can do, say, view-source.
 *
 *  @param   aParserCommand the command to set
 */
NS_IMETHODIMP_(void)
nsParser::SetCommand(eParserCommands aParserCommand)
{
  mCommand = aParserCommand;
}

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about what charset to load
 *
 *  @param   aCharset- the charset of a document
 *  @param   aCharsetSource- the source of the charset
 */
NS_IMETHODIMP_(void)
nsParser::SetDocumentCharset(const nsACString& aCharset, int32_t aCharsetSource)
{
  mCharset = aCharset;
  mCharsetSource = aCharsetSource;
  if (mParserContext && mParserContext->mScanner) {
     mParserContext->mScanner->SetDocumentCharset(aCharset, aCharsetSource);
  }
}

void
nsParser::SetSinkCharset(nsACString& aCharset)
{
  if (mSink) {
    mSink->SetDocumentCharset(aCharset);
  }
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *
 *  @param   nsIContentSink interface for node receiver
 */
NS_IMETHODIMP_(void)
nsParser::SetContentSink(nsIContentSink* aSink)
{
  NS_PRECONDITION(aSink, "sink cannot be null!");
  mSink = aSink;

  if (mSink) {
    mSink->SetParser(this);
    nsCOMPtr<nsIHTMLContentSink> htmlSink = do_QueryInterface(mSink);
    if (htmlSink) {
      mIsAboutBlank = true;
    }
  }
}

/**
 * retrieve the sink set into the parser
 * @return  current sink
 */
NS_IMETHODIMP_(nsIContentSink*)
nsParser::GetContentSink()
{
  return mSink;
}

/**
 * Determine what DTD mode (and thus what layout nsCompatibility mode)
 * to use for this document based on the first chunk of data received
 * from the network (each parsercontext can have its own mode).  (No,
 * this is not an optimal solution -- we really don't need to know until
 * after we've received the DOCTYPE, and this could easily be part of
 * the regular parsing process if the parser were designed in a way that
 * made such modifications easy.)
 */

// Parse the PS production in the SGML spec (excluding the part dealing
// with entity references) starting at theIndex into theBuffer, and
// return the first index after the end of the production.
static int32_t
ParsePS(const nsString& aBuffer, int32_t aIndex)
{
  for (;;) {
    char16_t ch = aBuffer.CharAt(aIndex);
    if ((ch == char16_t(' ')) || (ch == char16_t('\t')) ||
        (ch == char16_t('\n')) || (ch == char16_t('\r'))) {
      ++aIndex;
    } else if (ch == char16_t('-')) {
      int32_t tmpIndex;
      if (aBuffer.CharAt(aIndex+1) == char16_t('-') &&
          kNotFound != (tmpIndex=aBuffer.Find("--",false,aIndex+2,-1))) {
        aIndex = tmpIndex + 2;
      } else {
        return aIndex;
      }
    } else {
      return aIndex;
    }
  }
}

#define PARSE_DTD_HAVE_DOCTYPE          (1<<0)
#define PARSE_DTD_HAVE_PUBLIC_ID        (1<<1)
#define PARSE_DTD_HAVE_SYSTEM_ID        (1<<2)
#define PARSE_DTD_HAVE_INTERNAL_SUBSET  (1<<3)

// return true on success (includes not present), false on failure
static bool
ParseDocTypeDecl(const nsString &aBuffer,
                 int32_t *aResultFlags,
                 nsString &aPublicID,
                 nsString &aSystemID)
{
  bool haveDoctype = false;
  *aResultFlags = 0;

  // Skip through any comments and processing instructions
  // The PI-skipping is a bit of a hack.
  int32_t theIndex = 0;
  do {
    theIndex = aBuffer.FindChar('<', theIndex);
    if (theIndex == kNotFound) break;
    char16_t nextChar = aBuffer.CharAt(theIndex+1);
    if (nextChar == char16_t('!')) {
      int32_t tmpIndex = theIndex + 2;
      if (kNotFound !=
          (theIndex=aBuffer.Find("DOCTYPE", true, tmpIndex, 0))) {
        haveDoctype = true;
        theIndex += 7; // skip "DOCTYPE"
        break;
      }
      theIndex = ParsePS(aBuffer, tmpIndex);
      theIndex = aBuffer.FindChar('>', theIndex);
    } else if (nextChar == char16_t('?')) {
      theIndex = aBuffer.FindChar('>', theIndex);
    } else {
      break;
    }
  } while (theIndex != kNotFound);

  if (!haveDoctype)
    return true;
  *aResultFlags |= PARSE_DTD_HAVE_DOCTYPE;

  theIndex = ParsePS(aBuffer, theIndex);
  theIndex = aBuffer.Find("HTML", true, theIndex, 0);
  if (kNotFound == theIndex)
    return false;
  theIndex = ParsePS(aBuffer, theIndex+4);
  int32_t tmpIndex = aBuffer.Find("PUBLIC", true, theIndex, 0);

  if (kNotFound != tmpIndex) {
    theIndex = ParsePS(aBuffer, tmpIndex+6);

    // We get here only if we've read <!DOCTYPE HTML PUBLIC
    // (not case sensitive) possibly with comments within.

    // Now find the beginning and end of the public identifier
    // and the system identifier (if present).

    char16_t lit = aBuffer.CharAt(theIndex);
    if ((lit != char16_t('\"')) && (lit != char16_t('\'')))
      return false;

    // Start is the first character, excluding the quote, and End is
    // the final quote, so there are (end-start) characters.

    int32_t PublicIDStart = theIndex + 1;
    int32_t PublicIDEnd = aBuffer.FindChar(lit, PublicIDStart);
    if (kNotFound == PublicIDEnd)
      return false;
    theIndex = ParsePS(aBuffer, PublicIDEnd + 1);
    char16_t next = aBuffer.CharAt(theIndex);
    if (next == char16_t('>')) {
      // There was a public identifier, but no system
      // identifier,
      // so do nothing.
      // This is needed to avoid the else at the end, and it's
      // also the most common case.
    } else if ((next == char16_t('\"')) ||
               (next == char16_t('\''))) {
      // We found a system identifier.
      *aResultFlags |= PARSE_DTD_HAVE_SYSTEM_ID;
      int32_t SystemIDStart = theIndex + 1;
      int32_t SystemIDEnd = aBuffer.FindChar(next, SystemIDStart);
      if (kNotFound == SystemIDEnd)
        return false;
      aSystemID =
        Substring(aBuffer, SystemIDStart, SystemIDEnd - SystemIDStart);
    } else if (next == char16_t('[')) {
      // We found an internal subset.
      *aResultFlags |= PARSE_DTD_HAVE_INTERNAL_SUBSET;
    } else {
      // Something's wrong.
      return false;
    }

    // Since a public ID is a minimum literal, we must trim
    // and collapse whitespace
    aPublicID = Substring(aBuffer, PublicIDStart, PublicIDEnd - PublicIDStart);
    aPublicID.CompressWhitespace(true, true);
    *aResultFlags |= PARSE_DTD_HAVE_PUBLIC_ID;
  } else {
    tmpIndex=aBuffer.Find("SYSTEM", true, theIndex, 0);
    if (kNotFound != tmpIndex) {
      // DOCTYPES with system ID but no Public ID
      *aResultFlags |= PARSE_DTD_HAVE_SYSTEM_ID;

      theIndex = ParsePS(aBuffer, tmpIndex+6);
      char16_t next = aBuffer.CharAt(theIndex);
      if (next != char16_t('\"') && next != char16_t('\''))
        return false;

      int32_t SystemIDStart = theIndex + 1;
      int32_t SystemIDEnd = aBuffer.FindChar(next, SystemIDStart);

      if (kNotFound == SystemIDEnd)
        return false;
      aSystemID =
        Substring(aBuffer, SystemIDStart, SystemIDEnd - SystemIDStart);
      theIndex = ParsePS(aBuffer, SystemIDEnd + 1);
    }

    char16_t nextChar = aBuffer.CharAt(theIndex);
    if (nextChar == char16_t('['))
      *aResultFlags |= PARSE_DTD_HAVE_INTERNAL_SUBSET;
    else if (nextChar != char16_t('>'))
      return false;
  }
  return true;
}

struct PubIDInfo
{
  enum eMode {
    eQuirks,         /* always quirks mode, unless there's an internal subset */
    eAlmostStandards,/* eCompatibility_AlmostStandards */
    eFullStandards   /* eCompatibility_FullStandards */
      /*
       * public IDs that should trigger strict mode are not listed
       * since we want all future public IDs to trigger strict mode as
       * well
       */
  };

  const char* name;
  eMode mode_if_no_sysid;
  eMode mode_if_sysid;
};

#define ELEMENTS_OF(array_) (sizeof(array_)/sizeof(array_[0]))

// These must be in nsCRT::strcmp order so binary-search can be used.
// This is verified, |#ifdef DEBUG|, below.

// Even though public identifiers should be case sensitive, we will do
// all comparisons after converting to lower case in order to do
// case-insensitive comparison since there are a number of existing web
// sites that use the incorrect case.  Therefore all of the public
// identifiers below are in lower case (with the correct case following,
// in comments).  The case is verified, |#ifdef DEBUG|, below.
static const PubIDInfo kPublicIDs[] = {
  {"+//silmaril//dtd html pro v0r11 19970101//en" /* "+//Silmaril//dtd html Pro v0r11 19970101//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//advasoft ltd//dtd html 3.0 aswedit + extensions//en" /* "-//AdvaSoft Ltd//DTD HTML 3.0 asWedit + extensions//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//as//dtd html 3.0 aswedit + extensions//en" /* "-//AS//DTD HTML 3.0 asWedit + extensions//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0 level 1//en" /* "-//IETF//DTD HTML 2.0 Level 1//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0 level 2//en" /* "-//IETF//DTD HTML 2.0 Level 2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0 strict level 1//en" /* "-//IETF//DTD HTML 2.0 Strict Level 1//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0 strict level 2//en" /* "-//IETF//DTD HTML 2.0 Strict Level 2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0 strict//en" /* "-//IETF//DTD HTML 2.0 Strict//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0//en" /* "-//IETF//DTD HTML 2.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.1e//en" /* "-//IETF//DTD HTML 2.1E//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 3.0//en" /* "-//IETF//DTD HTML 3.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 3.0//en//" /* "-//IETF//DTD HTML 3.0//EN//" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 3.2 final//en" /* "-//IETF//DTD HTML 3.2 Final//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 3.2//en" /* "-//IETF//DTD HTML 3.2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 3//en" /* "-//IETF//DTD HTML 3//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 0//en" /* "-//IETF//DTD HTML Level 0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 0//en//2.0" /* "-//IETF//DTD HTML Level 0//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 1//en" /* "-//IETF//DTD HTML Level 1//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 1//en//2.0" /* "-//IETF//DTD HTML Level 1//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 2//en" /* "-//IETF//DTD HTML Level 2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 2//en//2.0" /* "-//IETF//DTD HTML Level 2//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 3//en" /* "-//IETF//DTD HTML Level 3//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 3//en//3.0" /* "-//IETF//DTD HTML Level 3//EN//3.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 0//en" /* "-//IETF//DTD HTML Strict Level 0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 0//en//2.0" /* "-//IETF//DTD HTML Strict Level 0//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 1//en" /* "-//IETF//DTD HTML Strict Level 1//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 1//en//2.0" /* "-//IETF//DTD HTML Strict Level 1//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 2//en" /* "-//IETF//DTD HTML Strict Level 2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 2//en//2.0" /* "-//IETF//DTD HTML Strict Level 2//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 3//en" /* "-//IETF//DTD HTML Strict Level 3//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 3//en//3.0" /* "-//IETF//DTD HTML Strict Level 3//EN//3.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict//en" /* "-//IETF//DTD HTML Strict//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict//en//2.0" /* "-//IETF//DTD HTML Strict//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict//en//3.0" /* "-//IETF//DTD HTML Strict//EN//3.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html//en" /* "-//IETF//DTD HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html//en//2.0" /* "-//IETF//DTD HTML//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html//en//3.0" /* "-//IETF//DTD HTML//EN//3.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//metrius//dtd metrius presentational//en" /* "-//Metrius//DTD Metrius Presentational//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 2.0 html strict//en" /* "-//Microsoft//DTD Internet Explorer 2.0 HTML Strict//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 2.0 html//en" /* "-//Microsoft//DTD Internet Explorer 2.0 HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 2.0 tables//en" /* "-//Microsoft//DTD Internet Explorer 2.0 Tables//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 3.0 html strict//en" /* "-//Microsoft//DTD Internet Explorer 3.0 HTML Strict//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 3.0 html//en" /* "-//Microsoft//DTD Internet Explorer 3.0 HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 3.0 tables//en" /* "-//Microsoft//DTD Internet Explorer 3.0 Tables//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//netscape comm. corp.//dtd html//en" /* "-//Netscape Comm. Corp.//DTD HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//netscape comm. corp.//dtd strict html//en" /* "-//Netscape Comm. Corp.//DTD Strict HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//o'reilly and associates//dtd html 2.0//en" /* "-//O'Reilly and Associates//DTD HTML 2.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//o'reilly and associates//dtd html extended 1.0//en" /* "-//O'Reilly and Associates//DTD HTML Extended 1.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//o'reilly and associates//dtd html extended relaxed 1.0//en" /* "-//O'Reilly and Associates//DTD HTML Extended Relaxed 1.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//en" /* "-//SoftQuad Software//DTD HoTMetaL PRO 6.0::19990601::extensions to HTML 4.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//en" /* "-//SoftQuad//DTD HoTMetaL PRO 4.0::19971010::extensions to HTML 4.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//spyglass//dtd html 2.0 extended//en" /* "-//Spyglass//DTD HTML 2.0 Extended//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//sq//dtd html 2.0 hotmetal + extensions//en" /* "-//SQ//DTD HTML 2.0 HoTMetaL + extensions//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//sun microsystems corp.//dtd hotjava html//en" /* "-//Sun Microsystems Corp.//DTD HotJava HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//sun microsystems corp.//dtd hotjava strict html//en" /* "-//Sun Microsystems Corp.//DTD HotJava Strict HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 3 1995-03-24//en" /* "-//W3C//DTD HTML 3 1995-03-24//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 3.2 draft//en" /* "-//W3C//DTD HTML 3.2 Draft//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 3.2 final//en" /* "-//W3C//DTD HTML 3.2 Final//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 3.2//en" /* "-//W3C//DTD HTML 3.2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 3.2s draft//en" /* "-//W3C//DTD HTML 3.2S Draft//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 4.0 frameset//en" /* "-//W3C//DTD HTML 4.0 Frameset//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 4.0 transitional//en" /* "-//W3C//DTD HTML 4.0 Transitional//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 4.01 frameset//en" /* "-//W3C//DTD HTML 4.01 Frameset//EN" */, PubIDInfo::eQuirks, PubIDInfo::eAlmostStandards},
  {"-//w3c//dtd html 4.01 transitional//en" /* "-//W3C//DTD HTML 4.01 Transitional//EN" */, PubIDInfo::eQuirks, PubIDInfo::eAlmostStandards},
  {"-//w3c//dtd html experimental 19960712//en" /* "-//W3C//DTD HTML Experimental 19960712//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html experimental 970421//en" /* "-//W3C//DTD HTML Experimental 970421//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd w3 html//en" /* "-//W3C//DTD W3 HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd xhtml 1.0 frameset//en" /* "-//W3C//DTD XHTML 1.0 Frameset//EN" */, PubIDInfo::eAlmostStandards, PubIDInfo::eAlmostStandards},
  {"-//w3c//dtd xhtml 1.0 transitional//en" /* "-//W3C//DTD XHTML 1.0 Transitional//EN" */, PubIDInfo::eAlmostStandards, PubIDInfo::eAlmostStandards},
  {"-//w3o//dtd w3 html 3.0//en" /* "-//W3O//DTD W3 HTML 3.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3o//dtd w3 html 3.0//en//" /* "-//W3O//DTD W3 HTML 3.0//EN//" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3o//dtd w3 html strict 3.0//en//" /* "-//W3O//DTD W3 HTML Strict 3.0//EN//" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//webtechs//dtd mozilla html 2.0//en" /* "-//WebTechs//DTD Mozilla HTML 2.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//webtechs//dtd mozilla html//en" /* "-//WebTechs//DTD Mozilla HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-/w3c/dtd html 4.0 transitional/en" /* "-/W3C/DTD HTML 4.0 Transitional/EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"html" /* "HTML" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
};

#ifdef DEBUG
static void
VerifyPublicIDs()
{
  static bool gVerified = false;
  if (!gVerified) {
    gVerified = true;
    uint32_t i;
    for (i = 0; i < ELEMENTS_OF(kPublicIDs) - 1; ++i) {
      if (nsCRT::strcmp(kPublicIDs[i].name, kPublicIDs[i+1].name) >= 0) {
        NS_NOTREACHED("doctypes out of order");
        printf("Doctypes %s and %s out of order.\n",
               kPublicIDs[i].name, kPublicIDs[i+1].name);
      }
    }
    for (i = 0; i < ELEMENTS_OF(kPublicIDs); ++i) {
      nsAutoCString lcPubID(kPublicIDs[i].name);
      ToLowerCase(lcPubID);
      if (nsCRT::strcmp(kPublicIDs[i].name, lcPubID.get()) != 0) {
        NS_NOTREACHED("doctype not lower case");
        printf("Doctype %s not lower case.\n", kPublicIDs[i].name);
      }
    }
  }
}
#endif

namespace {

struct PublicIdComparator
{
  const nsAutoCString& mPublicId;
  explicit PublicIdComparator(const nsAutoCString& aPublicId)
    : mPublicId(aPublicId) {}
  int operator()(const PubIDInfo& aInfo) const {
    return nsCRT::strcmp(mPublicId.get(), aInfo.name);
  }
};

} // namespace

static void
DetermineHTMLParseMode(const nsString& aBuffer,
                       nsDTDMode& aParseMode,
                       eParserDocType& aDocType)
{
#ifdef DEBUG
  VerifyPublicIDs();
#endif
  int32_t resultFlags;
  nsAutoString publicIDUCS2, sysIDUCS2;
  if (ParseDocTypeDecl(aBuffer, &resultFlags, publicIDUCS2, sysIDUCS2)) {
    if (!(resultFlags & PARSE_DTD_HAVE_DOCTYPE)) {
      // no DOCTYPE
      aParseMode = eDTDMode_quirks;
      aDocType = eHTML_Quirks;
    } else if ((resultFlags & PARSE_DTD_HAVE_INTERNAL_SUBSET) ||
               !(resultFlags & PARSE_DTD_HAVE_PUBLIC_ID)) {
      // A doctype with an internal subset is always full_standards.
      // A doctype without a public ID is always full_standards.
      aDocType = eHTML_Strict;
      aParseMode = eDTDMode_full_standards;

      // Special hack for IBM's custom DOCTYPE.
      if (!(resultFlags & PARSE_DTD_HAVE_INTERNAL_SUBSET) &&
          sysIDUCS2.EqualsLiteral(
               "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd")) {
        aParseMode = eDTDMode_quirks;
        aDocType = eHTML_Quirks;
      }

    } else {
      // We have to check our list of public IDs to see what to do.
      // Yes, we want UCS2 to ASCII lossy conversion.
      nsAutoCString publicID;
      publicID.AssignWithConversion(publicIDUCS2);

      // See comment above definition of kPublicIDs about case
      // sensitivity.
      ToLowerCase(publicID);

      // Binary search to see if we can find the correct public ID.
      size_t index;
      bool found = BinarySearchIf(kPublicIDs, 0, ArrayLength(kPublicIDs),
                                  PublicIdComparator(publicID), &index);
      if (!found) {
        // The DOCTYPE is not in our list, so it must be full_standards.
        aParseMode = eDTDMode_full_standards;
        aDocType = eHTML_Strict;
        return;
      }

      switch ((resultFlags & PARSE_DTD_HAVE_SYSTEM_ID)
                ? kPublicIDs[index].mode_if_sysid
                : kPublicIDs[index].mode_if_no_sysid)
      {
        case PubIDInfo::eQuirks:
          aParseMode = eDTDMode_quirks;
          aDocType = eHTML_Quirks;
          break;
        case PubIDInfo::eAlmostStandards:
          aParseMode = eDTDMode_almost_standards;
          aDocType = eHTML_Strict;
          break;
        case PubIDInfo::eFullStandards:
          aParseMode = eDTDMode_full_standards;
          aDocType = eHTML_Strict;
          break;
        default:
          NS_NOTREACHED("no other cases!");
      }
    }
  } else {
    // badly formed DOCTYPE -> quirks
    aParseMode = eDTDMode_quirks;
    aDocType = eHTML_Quirks;
  }
}

static void
DetermineParseMode(const nsString& aBuffer, nsDTDMode& aParseMode,
                   eParserDocType& aDocType, const nsACString& aMimeType)
{
  if (aMimeType.EqualsLiteral(TEXT_HTML)) {
    DetermineHTMLParseMode(aBuffer, aParseMode, aDocType);
  } else if (nsContentUtils::IsPlainTextType(aMimeType)) {
    aDocType = ePlainText;
    aParseMode = eDTDMode_quirks;
  } else { // Some form of XML
    aDocType = eXML;
    aParseMode = eDTDMode_full_standards;
  }
}

static nsIDTD*
FindSuitableDTD(CParserContext& aParserContext)
{
  // We always find a DTD.
  aParserContext.mAutoDetectStatus = ePrimaryDetect;

  // Quick check for view source.
  NS_ABORT_IF_FALSE(aParserContext.mParserCommand != eViewSource,
    "The old parser is not supposed to be used for View Source anymore.");

  // Now see if we're parsing HTML (which, as far as we're concerned, simply
  // means "not XML").
  if (aParserContext.mDocType != eXML) {
    return new CNavDTD();
  }

  // If we're here, then we'd better be parsing XML.
  NS_ASSERTION(aParserContext.mDocType == eXML, "What are you trying to send me, here?");
  return new nsExpatDriver();
}

NS_IMETHODIMP
nsParser::CancelParsingEvents()
{
  if (mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT) {
    NS_ASSERTION(mContinueEvent, "mContinueEvent is null");
    // Revoke the pending continue parsing event
    mContinueEvent = nullptr;
    mFlags &= ~NS_PARSER_FLAG_PENDING_CONTINUE_EVENT;
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

/**
 * Evalutes EXPR1 and EXPR2 exactly once each, in that order.  Stores the value
 * of EXPR2 in RV is EXPR2 fails, otherwise RV contains the result of EXPR1
 * (which could be success or failure).
 *
 * To understand the motivation for this construct, consider these example
 * methods:
 *
 *   nsresult nsSomething::DoThatThing(nsIWhatever* obj) {
 *     nsresult rv = NS_OK;
 *     ...
 *     return obj->DoThatThing();
 *     NS_ENSURE_SUCCESS(rv, rv);
 *     ...
 *     return rv;
 *   }
 *
 *   void nsCaller::MakeThingsHappen() {
 *     return mSomething->DoThatThing(mWhatever);
 *   }
 *
 * Suppose, for whatever reason*, we want to shift responsibility for calling
 * mWhatever->DoThatThing() from nsSomething::DoThatThing up to
 * nsCaller::MakeThingsHappen.  We might rewrite the two methods as follows:
 *
 *   nsresult nsSomething::DoThatThing() {
 *     nsresult rv = NS_OK;
 *     ...
 *     ...
 *     return rv;
 *   }
 *
 *   void nsCaller::MakeThingsHappen() {
 *     nsresult rv;
 *     PREFER_LATTER_ERROR_CODE(mSomething->DoThatThing(),
 *                              mWhatever->DoThatThing(),
 *                              rv);
 *     return rv;
 *   }
 *
 * *Possible reasons include: nsCaller doesn't want to give mSomething access
 * to mWhatever, nsCaller wants to guarantee that mWhatever->DoThatThing() will
 * be called regardless of how nsSomething::DoThatThing behaves, &c.
 */
#define PREFER_LATTER_ERROR_CODE(EXPR1, EXPR2, RV) {                          \
  nsresult RV##__temp = EXPR1;                                                \
  RV = EXPR2;                                                                 \
  if (NS_FAILED(RV)) {                                                        \
    RV = RV##__temp;                                                          \
  }                                                                           \
}

/**
 * This gets called just prior to the model actually
 * being constructed. It's important to make this the
 * last thing that happens right before parsing, so we
 * can delay until the last moment the resolution of
 * which DTD to use (unless of course we're assigned one).
 */
nsresult
nsParser::WillBuildModel(nsString& aFilename)
{
  if (!mParserContext)
    return kInvalidParserContext;

  if (eUnknownDetect != mParserContext->mAutoDetectStatus)
    return NS_OK;

  if (eDTDMode_unknown == mParserContext->mDTDMode ||
      eDTDMode_autodetect == mParserContext->mDTDMode) {
    char16_t buf[1025];
    nsFixedString theBuffer(buf, 1024, 0);

    // Grab 1024 characters, starting at the first non-whitespace
    // character, to look for the doctype in.
    mParserContext->mScanner->Peek(theBuffer, 1024, mParserContext->mScanner->FirstNonWhitespacePosition());
    DetermineParseMode(theBuffer, mParserContext->mDTDMode,
                       mParserContext->mDocType, mParserContext->mMimeType);
  }

  NS_ASSERTION(!mDTD || !mParserContext->mPrevContext,
               "Clobbering DTD for non-root parser context!");
  mDTD = FindSuitableDTD(*mParserContext);
  NS_ENSURE_TRUE(mDTD, NS_ERROR_OUT_OF_MEMORY);

  nsITokenizer* tokenizer;
  nsresult rv = mParserContext->GetTokenizer(mDTD, mSink, tokenizer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDTD->WillBuildModel(*mParserContext, tokenizer, mSink);
  nsresult sinkResult = mSink->WillBuildModel(mDTD->GetMode());
  // nsIDTD::WillBuildModel used to be responsible for calling
  // nsIContentSink::WillBuildModel, but that obligation isn't expressible
  // in the nsIDTD interface itself, so it's sounder and simpler to give that
  // responsibility back to the parser. The former behavior of the DTD was to
  // NS_ENSURE_SUCCESS the sink WillBuildModel call, so if the sink returns
  // failure we should use sinkResult instead of rv, to preserve the old error
  // handling behavior of the DTD:
  return NS_FAILED(sinkResult) ? sinkResult : rv;
}

/**
 * This gets called when the parser is done with its input.
 * Note that the parser may have been called recursively, so we
 * have to check for a prev. context before closing out the DTD/sink.
 */
nsresult
nsParser::DidBuildModel(nsresult anErrorCode)
{
  nsresult result = anErrorCode;

  if (IsComplete()) {
    if (mParserContext && !mParserContext->mPrevContext) {
      // Let sink know if we're about to end load because we've been terminated.
      // In that case we don't want it to run deferred scripts.
      bool terminated = mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING;
      if (mDTD && mSink) {
        nsresult dtdResult =  mDTD->DidBuildModel(anErrorCode),
                sinkResult = mSink->DidBuildModel(terminated);
        // nsIDTD::DidBuildModel used to be responsible for calling
        // nsIContentSink::DidBuildModel, but that obligation isn't expressible
        // in the nsIDTD interface itself, so it's sounder and simpler to give
        // that responsibility back to the parser. The former behavior of the
        // DTD was to NS_ENSURE_SUCCESS the sink DidBuildModel call, so if the
        // sink returns failure we should use sinkResult instead of dtdResult,
        // to preserve the old error handling behavior of the DTD:
        result = NS_FAILED(sinkResult) ? sinkResult : dtdResult;
      }

      //Ref. to bug 61462.
      mParserContext->mRequest = 0;
    }
  }

  return result;
}

/**
 * This method adds a new parser context to the list,
 * pushing the current one to the next position.
 *
 * @param   ptr to new context
 */
void
nsParser::PushContext(CParserContext& aContext)
{
  NS_ASSERTION(aContext.mPrevContext == mParserContext,
               "Trying to push a context whose previous context differs from "
               "the current parser context.");
  mParserContext = &aContext;
}

/**
 * This method pops the topmost context off the stack,
 * returning it to the user. The next context  (if any)
 * becomes the current context.
 * @update	gess7/22/98
 * @return  prev. context
 */
CParserContext*
nsParser::PopContext()
{
  CParserContext* oldContext = mParserContext;
  if (oldContext) {
    mParserContext = oldContext->mPrevContext;
    if (mParserContext) {
      // If the old context was blocked, propagate the blocked state
      // back to the new one. Also, propagate the stream listener state
      // but don't override onStop state to guarantee the call to DidBuildModel().
      if (mParserContext->mStreamListenerState != eOnStop) {
        mParserContext->mStreamListenerState = oldContext->mStreamListenerState;
      }
    }
  }
  return oldContext;
}

/**
 *  Call this when you want control whether or not the parser will parse
 *  and tokenize input (TRUE), or whether it just caches input to be
 *  parsed later (FALSE).
 *
 *  @param   aState determines whether we parse/tokenize or just cache.
 *  @return  current state
 */
void
nsParser::SetUnusedInput(nsString& aBuffer)
{
  mUnusedInput = aBuffer;
}

/**
 *  Call this when you want to *force* the parser to terminate the
 *  parsing process altogether. This is binary -- so once you terminate
 *  you can't resume without restarting altogether.
 */
NS_IMETHODIMP
nsParser::Terminate(void)
{
  // We should only call DidBuildModel once, so don't do anything if this is
  // the second time that Terminate has been called.
  if (mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING) {
    return NS_OK;
  }

  nsresult result = NS_OK;
  // XXX - [ until we figure out a way to break parser-sink circularity ]
  // Hack - Hold a reference until we are completely done...
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  mInternalState = result = NS_ERROR_HTMLPARSER_STOPPARSING;

  // CancelParsingEvents must be called to avoid leaking the nsParser object
  // @see bug 108049
  // If NS_PARSER_FLAG_PENDING_CONTINUE_EVENT is set then CancelParsingEvents
  // will reset it so DidBuildModel will call DidBuildModel on the DTD. Note:
  // The IsComplete() call inside of DidBuildModel looks at the pendingContinueEvents flag.
  CancelParsingEvents();

  // If we got interrupted in the middle of a document.write, then we might
  // have more than one parser context on our parsercontext stack. This has
  // the effect of making DidBuildModel a no-op, meaning that we never call
  // our sink's DidBuildModel and break the reference cycle, causing a leak.
  // Since we're getting terminated, we manually clean up our context stack.
  while (mParserContext && mParserContext->mPrevContext) {
    CParserContext *prev = mParserContext->mPrevContext;
    delete mParserContext;
    mParserContext = prev;
  }

  if (mDTD) {
    mDTD->Terminate();
    DidBuildModel(result);
  } else if (mSink) {
    // We have no parser context or no DTD yet (so we got terminated before we
    // got any data).  Manually break the reference cycle with the sink.
    result = mSink->DidBuildModel(true);
    NS_ENSURE_SUCCESS(result, result);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsParser::ContinueInterruptedParsing()
{
  // If there are scripts executing, then the content sink is jumping the gun
  // (probably due to a synchronous XMLHttpRequest) and will re-enable us
  // later, see bug 460706.
  if (!IsOkToProcessNetworkData()) {
    return NS_OK;
  }

  // If the stream has already finished, there's a good chance
  // that we might start closing things down when the parser
  // is reenabled. To make sure that we're not deleted across
  // the reenabling process, hold a reference to ourselves.
  nsresult result=NS_OK;
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  nsCOMPtr<nsIContentSink> sinkDeathGrip(mSink);

#ifdef DEBUG
  if (!(mFlags & NS_PARSER_FLAG_PARSER_ENABLED)) {
    NS_WARNING("Don't call ContinueInterruptedParsing on a blocked parser.");
  }
#endif

  bool isFinalChunk = mParserContext &&
                        mParserContext->mStreamListenerState == eOnStop;

  mProcessingNetworkData = true;
  if (mSink) {
    mSink->WillParse();
  }
  result = ResumeParse(true, isFinalChunk); // Ref. bug 57999
  mProcessingNetworkData = false;

  if (result != NS_OK) {
    result=mInternalState;
  }

  return result;
}

/**
 *  Stops parsing temporarily. That's it will prevent the
 *  parser from building up content model.
 */
NS_IMETHODIMP_(void)
nsParser::BlockParser()
{
  mFlags &= ~NS_PARSER_FLAG_PARSER_ENABLED;
}

/**
 *  Open up the parser for tokenization, building up content
 *  model..etc. However, this method does not resume parsing
 *  automatically. It's the callers' responsibility to restart
 *  the parsing engine.
 */
NS_IMETHODIMP_(void)
nsParser::UnblockParser()
{
  if (!(mFlags & NS_PARSER_FLAG_PARSER_ENABLED)) {
    mFlags |= NS_PARSER_FLAG_PARSER_ENABLED;
  } else {
    NS_WARNING("Trying to unblock an unblocked parser.");
  }
}

NS_IMETHODIMP_(void)
nsParser::ContinueInterruptedParsingAsync()
{
  mSink->ContinueInterruptedParsingAsync();
}

/**
 * Call this to query whether the parser is enabled or not.
 */
NS_IMETHODIMP_(bool)
nsParser::IsParserEnabled()
{
  return (mFlags & NS_PARSER_FLAG_PARSER_ENABLED) != 0;
}

/**
 * Call this to query whether the parser thinks it's done with parsing.
 */
NS_IMETHODIMP_(bool)
nsParser::IsComplete()
{
  return !(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT);
}


void nsParser::HandleParserContinueEvent(nsParserContinueEvent *ev)
{
  // Ignore any revoked continue events...
  if (mContinueEvent != ev)
    return;

  mFlags &= ~NS_PARSER_FLAG_PENDING_CONTINUE_EVENT;
  mContinueEvent = nullptr;

  NS_ASSERTION(IsOkToProcessNetworkData(),
               "Interrupted in the middle of a script?");
  ContinueInterruptedParsing();
}

bool
nsParser::IsInsertionPointDefined()
{
  return false;
}

void
nsParser::BeginEvaluatingParserInsertedScript()
{
}

void
nsParser::EndEvaluatingParserInsertedScript()
{
}

void
nsParser::MarkAsNotScriptCreated(const char* aCommand)
{
}

bool
nsParser::IsScriptCreated()
{
  return false;
}

/**
 *  This is the main controlling routine in the parsing process.
 *  Note that it may get called multiple times for the same scanner,
 *  since this is a pushed based system, and all the tokens may
 *  not have been consumed by the scanner during a given invocation
 *  of this method.
 */
NS_IMETHODIMP
nsParser::Parse(nsIURI* aURL,
                nsIRequestObserver* aListener,
                void* aKey,
                nsDTDMode aMode)
{

  NS_PRECONDITION(aURL, "Error: Null URL given");

  nsresult result=kBadURL;
  mObserver = aListener;

  if (aURL) {
    nsAutoCString spec;
    nsresult rv = aURL->GetSpec(spec);
    if (rv != NS_OK) {
      return rv;
    }
    NS_ConvertUTF8toUTF16 theName(spec);

    nsScanner* theScanner = new nsScanner(theName, false);
    CParserContext* pc = new CParserContext(mParserContext, theScanner, aKey,
                                            mCommand, aListener);
    if (pc && theScanner) {
      pc->mMultipart = true;
      pc->mContextType = CParserContext::eCTURL;
      pc->mDTDMode = aMode;
      PushContext(*pc);

      result = NS_OK;
    } else {
      result = mInternalState = NS_ERROR_HTMLPARSER_BADCONTEXT;
    }
  }
  return result;
}

/**
 * Used by XML fragment parsing below.
 *
 * @param   aSourceBuffer contains a string-full of real content
 */
nsresult
nsParser::Parse(const nsAString& aSourceBuffer,
                void* aKey,
                bool aLastCall)
{
  nsresult result = NS_OK;

  // Don't bother if we're never going to parse this.
  if (mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING) {
    return result;
  }

  if (!aLastCall && aSourceBuffer.IsEmpty()) {
    // Nothing is being passed to the parser so return
    // immediately. mUnusedInput will get processed when
    // some data is actually passed in.
    // But if this is the last call, make sure to finish up
    // stuff correctly.
    return result;
  }

  // Maintain a reference to ourselves so we don't go away
  // till we're completely done.
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);

  if (aLastCall || !aSourceBuffer.IsEmpty() || !mUnusedInput.IsEmpty()) {
    // Note: The following code will always find the parser context associated
    // with the given key, even if that context has been suspended (e.g., for
    // another document.write call). This doesn't appear to be exactly what IE
    // does in the case where this happens, but this makes more sense.
    CParserContext* pc = mParserContext;
    while (pc && pc->mKey != aKey) {
      pc = pc->mPrevContext;
    }

    if (!pc) {
      // Only make a new context if we don't have one, OR if we do, but has a
      // different context key.
      nsScanner* theScanner = new nsScanner(mUnusedInput);
      NS_ENSURE_TRUE(theScanner, NS_ERROR_OUT_OF_MEMORY);

      eAutoDetectResult theStatus = eUnknownDetect;

      if (mParserContext &&
          mParserContext->mMimeType.EqualsLiteral("application/xml")) {
        // Ref. Bug 90379
        NS_ASSERTION(mDTD, "How come the DTD is null?");

        if (mParserContext) {
          theStatus = mParserContext->mAutoDetectStatus;
          // Added this to fix bug 32022.
        }
      }

      pc = new CParserContext(mParserContext, theScanner, aKey, mCommand,
                              0, theStatus, aLastCall);
      NS_ENSURE_TRUE(pc, NS_ERROR_OUT_OF_MEMORY);

      PushContext(*pc);

      pc->mMultipart = !aLastCall; // By default
      if (pc->mPrevContext) {
        pc->mMultipart |= pc->mPrevContext->mMultipart;
      }

      // Start fix bug 40143
      if (pc->mMultipart) {
        pc->mStreamListenerState = eOnDataAvail;
        if (pc->mScanner) {
          pc->mScanner->SetIncremental(true);
        }
      } else {
        pc->mStreamListenerState = eOnStop;
        if (pc->mScanner) {
          pc->mScanner->SetIncremental(false);
        }
      }
      // end fix for 40143

      pc->mContextType=CParserContext::eCTString;
      pc->SetMimeType(NS_LITERAL_CSTRING("application/xml"));
      pc->mDTDMode = eDTDMode_full_standards;

      mUnusedInput.Truncate();

      pc->mScanner->Append(aSourceBuffer);
      // Do not interrupt document.write() - bug 95487
      result = ResumeParse(false, false, false);
    } else {
      pc->mScanner->Append(aSourceBuffer);
      if (!pc->mPrevContext) {
        // Set stream listener state to eOnStop, on the final context - Fix 68160,
        // to guarantee DidBuildModel() call - Fix 36148
        if (aLastCall) {
          pc->mStreamListenerState = eOnStop;
          pc->mScanner->SetIncremental(false);
        }

        if (pc == mParserContext) {
          // If pc is not mParserContext, then this call to ResumeParse would
          // do the wrong thing and try to continue parsing using
          // mParserContext. We need to wait to actually resume parsing on pc.
          ResumeParse(false, false, false);
        }
      }
    }
  }

  return result;
}

NS_IMETHODIMP
nsParser::ParseFragment(const nsAString& aSourceBuffer,
                        nsTArray<nsString>& aTagStack)
{
  nsresult result = NS_OK;
  nsAutoString  theContext;
  uint32_t theCount = aTagStack.Length();
  uint32_t theIndex = 0;

  // Disable observers for fragments
  mFlags &= ~NS_PARSER_FLAG_OBSERVERS_ENABLED;

  for (theIndex = 0; theIndex < theCount; theIndex++) {
    theContext.Append('<');
    theContext.Append(aTagStack[theCount - theIndex - 1]);
    theContext.Append('>');
  }

  if (theCount == 0) {
    // Ensure that the buffer is not empty. Because none of the DTDs care
    // about leading whitespace, this doesn't change the result.
    theContext.Assign(' ');
  }

  // First, parse the context to build up the DTD's tag stack. Note that we
  // pass false for the aLastCall parameter.
  result = Parse(theContext,
                 (void*)&theContext,
                 false);
  if (NS_FAILED(result)) {
    mFlags |= NS_PARSER_FLAG_OBSERVERS_ENABLED;
    return result;
  }

  if (!mSink) {
    // Parse must have failed in the XML case and so the sink was killed.
    return NS_ERROR_HTMLPARSER_STOPPARSING;
  }

  nsCOMPtr<nsIFragmentContentSink> fragSink = do_QueryInterface(mSink);
  NS_ASSERTION(fragSink, "ParseFragment requires a fragment content sink");

  fragSink->WillBuildContent();
  // Now, parse the actual content. Note that this is the last call
  // for HTML content, but for XML, we will want to build and parse
  // the end tags.  However, if tagStack is empty, it's the last call
  // for XML as well.
  if (theCount == 0) {
    result = Parse(aSourceBuffer,
                   &theContext,
                   true);
    fragSink->DidBuildContent();
  } else {
    // Add an end tag chunk, so expat will read the whole source buffer,
    // and not worry about ']]' etc.
    result = Parse(aSourceBuffer + NS_LITERAL_STRING("</"),
                   &theContext,
                   false);
    fragSink->DidBuildContent();

    if (NS_SUCCEEDED(result)) {
      nsAutoString endContext;
      for (theIndex = 0; theIndex < theCount; theIndex++) {
         // we already added an end tag chunk above
        if (theIndex > 0) {
          endContext.AppendLiteral("</");
        }

        nsString& thisTag = aTagStack[theIndex];
        // was there an xmlns=?
        int32_t endOfTag = thisTag.FindChar(char16_t(' '));
        if (endOfTag == -1) {
          endContext.Append(thisTag);
        } else {
          endContext.Append(Substring(thisTag,0,endOfTag));
        }

        endContext.Append('>');
      }

      result = Parse(endContext,
                     &theContext,
                     true);
    }
  }

  mFlags |= NS_PARSER_FLAG_OBSERVERS_ENABLED;

  return result;
}

/**
 *  This routine is called to cause the parser to continue parsing its
 *  underlying stream.  This call allows the parse process to happen in
 *  chunks, such as when the content is push based, and we need to parse in
 *  pieces.
 *
 *  An interesting change in how the parser gets used has led us to add extra
 *  processing to this method.  The case occurs when the parser is blocked in
 *  one context, and gets a parse(string) call in another context.  In this
 *  case, the parserContexts are linked. No problem.
 *
 *  The problem is that Parse(string) assumes that it can proceed unabated,
 *  but if the parser is already blocked that assumption is false. So we
 *  needed to add a mechanism here to allow the parser to continue to process
 *  (the pop and free) contexts until 1) it get's blocked again; 2) it runs
 *  out of contexts.
 *
 *
 *  @param   allowItertion : set to true if non-script resumption is requested
 *  @param   aIsFinalChunk : tells us when the last chunk of data is provided.
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult
nsParser::ResumeParse(bool allowIteration, bool aIsFinalChunk,
                      bool aCanInterrupt)
{
  nsresult result = NS_OK;

  if ((mFlags & NS_PARSER_FLAG_PARSER_ENABLED) &&
      mInternalState != NS_ERROR_HTMLPARSER_STOPPARSING) {

    result = WillBuildModel(mParserContext->mScanner->GetFilename());
    if (NS_FAILED(result)) {
      mFlags &= ~NS_PARSER_FLAG_CAN_TOKENIZE;
      return result;
    }

    if (mDTD) {
      mSink->WillResume();
      bool theIterationIsOk = true;

      while (result == NS_OK && theIterationIsOk) {
        if (!mUnusedInput.IsEmpty() && mParserContext->mScanner) {
          // -- Ref: Bug# 22485 --
          // Insert the unused input into the source buffer
          // as if it was read from the input stream.
          // Adding UngetReadable() per vidur!!
          mParserContext->mScanner->UngetReadable(mUnusedInput);
          mUnusedInput.Truncate(0);
        }

        // Only allow parsing to be interrupted in the subsequent call to
        // build model.
        nsresult theTokenizerResult = (mFlags & NS_PARSER_FLAG_CAN_TOKENIZE)
                                      ? Tokenize(aIsFinalChunk)
                                      : NS_OK;
        result = BuildModel();

        if (result == NS_ERROR_HTMLPARSER_INTERRUPTED && aIsFinalChunk) {
          PostContinueEvent();
        }

        theIterationIsOk = theTokenizerResult != kEOF &&
                           result != NS_ERROR_HTMLPARSER_INTERRUPTED;

        // Make sure not to stop parsing too early. Therefore, before shutting
        // down the parser, it's important to check whether the input buffer
        // has been scanned to completion (theTokenizerResult should be kEOF).
        // kEOF -> End of buffer.

        // If we're told to block the parser, we disable all further parsing
        // (and cache any data coming in) until the parser is re-enabled.
        if (NS_ERROR_HTMLPARSER_BLOCK == result) {
          mSink->WillInterrupt();
          if (mFlags & NS_PARSER_FLAG_PARSER_ENABLED) {
            // If we were blocked by a recursive invocation, don't re-block.
            BlockParser();
          }
          return NS_OK;
        }
        if (NS_ERROR_HTMLPARSER_STOPPARSING == result) {
          // Note: Parser Terminate() calls DidBuildModel.
          if (mInternalState != NS_ERROR_HTMLPARSER_STOPPARSING) {
            DidBuildModel(mStreamStatus);
            mInternalState = result;
          }

          return NS_OK;
        }
        if ((NS_OK == result && theTokenizerResult == kEOF) ||
             result == NS_ERROR_HTMLPARSER_INTERRUPTED) {
          bool theContextIsStringBased =
            CParserContext::eCTString == mParserContext->mContextType;

          if (mParserContext->mStreamListenerState == eOnStop ||
              !mParserContext->mMultipart || theContextIsStringBased) {
            if (!mParserContext->mPrevContext) {
              if (mParserContext->mStreamListenerState == eOnStop) {
                DidBuildModel(mStreamStatus);
                return NS_OK;
              }
            } else {
              CParserContext* theContext = PopContext();
              if (theContext) {
                theIterationIsOk = allowIteration && theContextIsStringBased;
                if (theContext->mCopyUnused) {
                  theContext->mScanner->CopyUnusedData(mUnusedInput);
                }

                delete theContext;
              }

              result = mInternalState;
              aIsFinalChunk = mParserContext &&
                              mParserContext->mStreamListenerState == eOnStop;
              // ...then intentionally fall through to mSink->WillInterrupt()...
            }
          }
        }

        if (theTokenizerResult == kEOF ||
            result == NS_ERROR_HTMLPARSER_INTERRUPTED) {
          result = (result == NS_ERROR_HTMLPARSER_INTERRUPTED) ? NS_OK : result;
          mSink->WillInterrupt();
        }
      }
    } else {
      mInternalState = result = NS_ERROR_HTMLPARSER_UNRESOLVEDDTD;
    }
  }

  return (result == NS_ERROR_HTMLPARSER_INTERRUPTED) ? NS_OK : result;
}

/**
 *  This is where we loop over the tokens created in the
 *  tokenization phase, and try to make sense out of them.
 */
nsresult
nsParser::BuildModel()
{
  nsITokenizer* theTokenizer = nullptr;

  nsresult result = NS_OK;
  if (mParserContext) {
    result = mParserContext->GetTokenizer(mDTD, mSink, theTokenizer);
  }

  if (NS_SUCCEEDED(result)) {
    if (mDTD) {
      result = mDTD->BuildModel(theTokenizer, mSink);
    }
  } else {
    mInternalState = result = NS_ERROR_HTMLPARSER_BADTOKENIZER;
  }
  return result;
}

/*******************************************************************
  These methods are used to talk to the netlib system...
 *******************************************************************/

nsresult
nsParser::OnStartRequest(nsIRequest *request, nsISupports* aContext)
{
  NS_PRECONDITION(eNone == mParserContext->mStreamListenerState,
                  "Parser's nsIStreamListener API was not setup "
                  "correctly in constructor.");
  if (mObserver) {
    mObserver->OnStartRequest(request, aContext);
  }
  mParserContext->mStreamListenerState = eOnStart;
  mParserContext->mAutoDetectStatus = eUnknownDetect;
  mParserContext->mRequest = request;

  NS_ASSERTION(!mParserContext->mPrevContext,
               "Clobbering DTD for non-root parser context!");
  mDTD = nullptr;

  nsresult rv;
  nsAutoCString contentType;
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (channel) {
    rv = channel->GetContentType(contentType);
    if (NS_SUCCEEDED(rv)) {
      mParserContext->SetMimeType(contentType);
    }
  }

  rv = NS_OK;

  return rv;
}

static bool
ExtractCharsetFromXmlDeclaration(const unsigned char* aBytes, int32_t aLen,
                                 nsCString& oCharset)
{
  // This code is rather pointless to have. Might as well reuse expat as
  // seen in nsHtml5StreamParser. -- hsivonen
  oCharset.Truncate();
  if ((aLen >= 5) &&
      ('<' == aBytes[0]) &&
      ('?' == aBytes[1]) &&
      ('x' == aBytes[2]) &&
      ('m' == aBytes[3]) &&
      ('l' == aBytes[4])) {
    int32_t i;
    bool versionFound = false, encodingFound = false;
    for (i = 6; i < aLen && !encodingFound; ++i) {
      // end of XML declaration?
      if ((((char*) aBytes)[i] == '?') &&
          ((i + 1) < aLen) &&
          (((char*) aBytes)[i + 1] == '>')) {
        break;
      }
      // Version is required.
      if (!versionFound) {
        // Want to avoid string comparisons, hence looking for 'n'
        // and only if found check the string leading to it. Not
        // foolproof, but fast.
        // The shortest string allowed before this is  (strlen==13):
        // <?xml version
        if ((((char*) aBytes)[i] == 'n') &&
            (i >= 12) &&
            (0 == PL_strncmp("versio", (char*) (aBytes + i - 6), 6))) {
          // Fast forward through version
          char q = 0;
          for (++i; i < aLen; ++i) {
            char qi = ((char*) aBytes)[i];
            if (qi == '\'' || qi == '"') {
              if (q && q == qi) {
                //  ending quote
                versionFound = true;
                break;
              } else {
                // Starting quote
                q = qi;
              }
            }
          }
        }
      } else {
        // encoding must follow version
        // Want to avoid string comparisons, hence looking for 'g'
        // and only if found check the string leading to it. Not
        // foolproof, but fast.
        // The shortest allowed string before this (strlen==26):
        // <?xml version="1" encoding
        if ((((char*) aBytes)[i] == 'g') && (i >= 25) && (0 == PL_strncmp(
            "encodin", (char*) (aBytes + i - 7), 7))) {
          int32_t encStart = 0;
          char q = 0;
          for (++i; i < aLen; ++i) {
            char qi = ((char*) aBytes)[i];
            if (qi == '\'' || qi == '"') {
              if (q && q == qi) {
                int32_t count = i - encStart;
                // encoding value is invalid if it is UTF-16
                if (count > 0 && PL_strncasecmp("UTF-16",
                    (char*) (aBytes + encStart), count)) {
                  oCharset.Assign((char*) (aBytes + encStart), count);
                }
                encodingFound = true;
                break;
              } else {
                encStart = i + 1;
                q = qi;
              }
            }
          }
        }
      } // if (!versionFound)
    } // for
  }
  return !oCharset.IsEmpty();
}

inline const char
GetNextChar(nsACString::const_iterator& aStart,
            nsACString::const_iterator& aEnd)
{
  NS_ASSERTION(aStart != aEnd, "end of buffer");
  return (++aStart != aEnd) ? *aStart : '\0';
}

static NS_METHOD
NoOpParserWriteFunc(nsIInputStream* in,
                void* closure,
                const char* fromRawSegment,
                uint32_t toOffset,
                uint32_t count,
                uint32_t *writeCount)
{
  *writeCount = count;
  return NS_OK;
}

typedef struct {
  bool mNeedCharsetCheck;
  nsParser* mParser;
  nsScanner* mScanner;
  nsIRequest* mRequest;
} ParserWriteStruct;

/*
 * This function is invoked as a result of a call to a stream's
 * ReadSegments() method. It is called for each contiguous buffer
 * of data in the underlying stream or pipe. Using ReadSegments
 * allows us to avoid copying data to read out of the stream.
 */
static NS_METHOD
ParserWriteFunc(nsIInputStream* in,
                void* closure,
                const char* fromRawSegment,
                uint32_t toOffset,
                uint32_t count,
                uint32_t *writeCount)
{
  nsresult result;
  ParserWriteStruct* pws = static_cast<ParserWriteStruct*>(closure);
  const unsigned char* buf =
    reinterpret_cast<const unsigned char*> (fromRawSegment);
  uint32_t theNumRead = count;

  if (!pws) {
    return NS_ERROR_FAILURE;
  }

  if (pws->mNeedCharsetCheck) {
    pws->mNeedCharsetCheck = false;
    int32_t source;
    nsAutoCString preferred;
    nsAutoCString maybePrefer;
    pws->mParser->GetDocumentCharset(preferred, source);

    // This code was bogus when I found it. It expects the BOM or the XML
    // declaration to be entirely in the first network buffer. -- hsivonen
    if (nsContentUtils::CheckForBOM(buf, count, maybePrefer)) {
      // The decoder will swallow the BOM. The UTF-16 will re-sniff for
      // endianness. The value of preferred is now either "UTF-8" or "UTF-16".
      preferred.Assign(maybePrefer);
      source = kCharsetFromByteOrderMark;
    } else if (source < kCharsetFromChannel) {
      nsAutoCString declCharset;

      if (ExtractCharsetFromXmlDeclaration(buf, count, declCharset)) {
        if (EncodingUtils::FindEncodingForLabel(declCharset, maybePrefer)) {
          preferred.Assign(maybePrefer);
          source = kCharsetFromMetaTag;
        }
      }
    }

    pws->mParser->SetDocumentCharset(preferred, source);
    pws->mParser->SetSinkCharset(preferred);

  }

  result = pws->mScanner->Append(fromRawSegment, theNumRead, pws->mRequest);
  if (NS_SUCCEEDED(result)) {
    *writeCount = count;
  }

  return result;
}

nsresult
nsParser::OnDataAvailable(nsIRequest *request, nsISupports* aContext,
                          nsIInputStream *pIStream, uint64_t sourceOffset,
                          uint32_t aLength)
{
  NS_PRECONDITION((eOnStart == mParserContext->mStreamListenerState ||
                   eOnDataAvail == mParserContext->mStreamListenerState),
            "Error: OnStartRequest() must be called before OnDataAvailable()");
  NS_PRECONDITION(NS_InputStreamIsBuffered(pIStream),
                  "Must have a buffered input stream");

  nsresult rv = NS_OK;

  if (mIsAboutBlank) {
    MOZ_ASSERT(false, "Must not get OnDataAvailable for about:blank");
    // ... but if an extension tries to feed us data for about:blank in a
    // release build, silently ignore the data.
    uint32_t totalRead;
    rv = pIStream->ReadSegments(NoOpParserWriteFunc,
                                nullptr,
                                aLength,
                                &totalRead);
    return rv;
  }

  CParserContext *theContext = mParserContext;

  while (theContext && theContext->mRequest != request) {
    theContext = theContext->mPrevContext;
  }

  if (theContext) {
    theContext->mStreamListenerState = eOnDataAvail;

    if (eInvalidDetect == theContext->mAutoDetectStatus) {
      if (theContext->mScanner) {
        nsScannerIterator iter;
        theContext->mScanner->EndReading(iter);
        theContext->mScanner->SetPosition(iter, true);
      }
    }

    uint32_t totalRead;
    ParserWriteStruct pws;
    pws.mNeedCharsetCheck = true;
    pws.mParser = this;
    pws.mScanner = theContext->mScanner;
    pws.mRequest = request;

    rv = pIStream->ReadSegments(ParserWriteFunc, &pws, aLength, &totalRead);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Don't bother to start parsing until we've seen some
    // non-whitespace data
    if (IsOkToProcessNetworkData() &&
        theContext->mScanner->FirstNonWhitespacePosition() >= 0) {
      nsCOMPtr<nsIParser> kungFuDeathGrip(this);
      nsCOMPtr<nsIContentSink> sinkDeathGrip(mSink);
      mProcessingNetworkData = true;
      if (mSink) {
        mSink->WillParse();
      }
      rv = ResumeParse();
      mProcessingNetworkData = false;
    }
  } else {
    rv = NS_ERROR_UNEXPECTED;
  }

  return rv;
}

/**
 *  This is called by the networking library once the last block of data
 *  has been collected from the net.
 */
nsresult
nsParser::OnStopRequest(nsIRequest *request, nsISupports* aContext,
                        nsresult status)
{
  nsresult rv = NS_OK;

  CParserContext *pc = mParserContext;
  while (pc) {
    if (pc->mRequest == request) {
      pc->mStreamListenerState = eOnStop;
      pc->mScanner->SetIncremental(false);
      break;
    }

    pc = pc->mPrevContext;
  }

  mStreamStatus = status;

  if (IsOkToProcessNetworkData() && NS_SUCCEEDED(rv)) {
    mProcessingNetworkData = true;
    if (mSink) {
      mSink->WillParse();
    }
    rv = ResumeParse(true, true);
    mProcessingNetworkData = false;
  }

  // If the parser isn't enabled, we don't finish parsing till
  // it is reenabled.


  // XXX Should we wait to notify our observers as well if the
  // parser isn't yet enabled?
  if (mObserver) {
    mObserver->OnStopRequest(request, aContext, status);
  }

  return rv;
}


/*******************************************************************
  Here come the tokenization methods...
 *******************************************************************/


/**
 *  Part of the code sandwich, this gets called right before
 *  the tokenization process begins. The main reason for
 *  this call is to allow the delegate to do initialization.
 */
bool
nsParser::WillTokenize(bool aIsFinalChunk)
{
  if (!mParserContext) {
    return true;
  }

  nsITokenizer* theTokenizer;
  nsresult result = mParserContext->GetTokenizer(mDTD, mSink, theTokenizer);
  NS_ENSURE_SUCCESS(result, false);
  return NS_SUCCEEDED(theTokenizer->WillTokenize(aIsFinalChunk));
}


/**
 * This is the primary control routine to consume tokens.
 * It iteratively consumes tokens until an error occurs or
 * you run out of data.
 */
nsresult nsParser::Tokenize(bool aIsFinalChunk)
{
  nsITokenizer* theTokenizer;

  nsresult result = NS_ERROR_NOT_AVAILABLE;
  if (mParserContext) {
    result = mParserContext->GetTokenizer(mDTD, mSink, theTokenizer);
  }

  if (NS_SUCCEEDED(result)) {
    bool flushTokens = false;

    bool killSink = false;

    WillTokenize(aIsFinalChunk);
    while (NS_SUCCEEDED(result)) {
      mParserContext->mScanner->Mark();
      result = theTokenizer->ConsumeToken(*mParserContext->mScanner,
                                          flushTokens);
      if (NS_FAILED(result)) {
        mParserContext->mScanner->RewindToMark();
        if (kEOF == result){
          break;
        }
        if (NS_ERROR_HTMLPARSER_STOPPARSING == result) {
          killSink = true;
          result = Terminate();
          break;
        }
      } else if (flushTokens && (mFlags & NS_PARSER_FLAG_OBSERVERS_ENABLED)) {
        // I added the extra test of NS_PARSER_FLAG_OBSERVERS_ENABLED to fix Bug# 23931.
        // Flush tokens on seeing </SCRIPT> -- Ref: Bug# 22485 --
        // Also remember to update the marked position.
        mFlags |= NS_PARSER_FLAG_FLUSH_TOKENS;
        mParserContext->mScanner->Mark();
        break;
      }
    }

    if (killSink) {
      mSink = nullptr;
    }
  } else {
    result = mInternalState = NS_ERROR_HTMLPARSER_BADTOKENIZER;
  }

  return result;
}

/**
 * Get the channel associated with this parser
 *
 * @param aChannel out param that will contain the result
 * @return NS_OK if successful
 */
NS_IMETHODIMP
nsParser::GetChannel(nsIChannel** aChannel)
{
  nsresult result = NS_ERROR_NOT_AVAILABLE;
  if (mParserContext && mParserContext->mRequest) {
    result = CallQueryInterface(mParserContext->mRequest, aChannel);
  }
  return result;
}

/**
 * Get the DTD associated with this parser
 */
NS_IMETHODIMP
nsParser::GetDTD(nsIDTD** aDTD)
{
  if (mParserContext) {
    NS_IF_ADDREF(*aDTD = mDTD);
  }

  return NS_OK;
}

/**
 * Get this as nsIStreamListener
 */
nsIStreamListener*
nsParser::GetStreamListener()
{
  return this;
}
