/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
#ifndef NS_IPARSER___
#define NS_IPARSER___


/**
 * MODULE NOTES:
 *  
 *  This class defines the iparser interface. This XPCOM
 *  inteface is all that parser clients ever need to see.
 *
 **/

#include "nsISupports.h"
#include "nsIStreamListener.h"
#include "nsIDTD.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "nsIAtom.h"

#define NS_IPARSER_IID \
{ 0xcbc0cbd8, 0xbbb7, 0x46d6, \
  { 0xa5, 0x51, 0x37, 0x8a, 0x69, 0x53, 0xa7, 0x14 } }

// {41421C60-310A-11d4-816F-000064657374}
#define NS_IDEBUG_DUMP_CONTENT_IID \
{ 0x41421c60, 0x310a, 0x11d4, \
{ 0x81, 0x6f, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

class nsIContentSink;
class nsIRequestObserver;
class nsIParserFilter;
class nsString;
class nsIURI;
class nsIChannel;
class nsIContent;

enum eParserCommands {
  eViewNormal,
  eViewSource,
  eViewFragment,
  eViewErrors
};

enum eParserDocType {
  ePlainText = 0,
  eXML,
  eHTML_Quirks,
  eHTML_Strict
};


// define Charset source constants
// note: the value order defines the priority; higher numbers take priority
#define kCharsetUninitialized           0
#define kCharsetFromWeakDocTypeDefault  1
#define kCharsetFromUserDefault         2
#define kCharsetFromDocTypeDefault      3
#define kCharsetFromCache               4
#define kCharsetFromParentFrame         5
#define kCharsetFromAutoDetection       6
#define kCharsetFromHintPrevDoc         7
#define kCharsetFromMetaPrescan         8 // this one and smaller: HTML5 Tentative
#define kCharsetFromMetaTag             9 // this one and greater: HTML5 Confident
#define kCharsetFromByteOrderMark      10
#define kCharsetFromChannel            11
#define kCharsetFromOtherComponent     12
// Levels below here will be forced onto childframes too
#define kCharsetFromParentForced       13
#define kCharsetFromUserForced         14
#define kCharsetFromPreviousLoading    15

enum eStreamState {eNone,eOnStart,eOnDataAvail,eOnStop};

/** 
 *  FOR DEBUG PURPOSE ONLY
 *
 *  Use this interface to query objects that contain content information.
 *  Ex. Parser can trigger dump content by querying the sink that has
 *      access to the content.
 *  
 *  @update  harishd 05/25/00
 */
class nsIDebugDumpContent : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDEBUG_DUMP_CONTENT_IID)
  NS_IMETHOD DumpContentModel()=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDebugDumpContent, NS_IDEBUG_DUMP_CONTENT_IID)

/**
 *  This class defines the iparser interface. This XPCOM
 *  inteface is all that parser clients ever need to see.
 */
class nsIParser : public nsISupports {
  public:

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPARSER_IID)

    /**
     * Select given content sink into parser for parser output
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  
     */
    NS_IMETHOD_(void) SetContentSink(nsIContentSink* aSink)=0;


    /**
     * retrieve the sink set into the parser 
     * @update	gess5/11/98
     * @return  current sink
     */
    NS_IMETHOD_(nsIContentSink*) GetContentSink(void)=0;

    /**
     *  Call this method once you've created a parser, and want to instruct it
	   *  about the command which caused the parser to be constructed. For example,
     *  this allows us to select a DTD which can do, say, view-source.
     *  
     *  @update  gess 3/25/98
     *  @param   aCommand -- ptrs to string that contains command
     *  @return	 nada
     */
    NS_IMETHOD_(void) GetCommand(nsCString& aCommand)=0;
    NS_IMETHOD_(void) SetCommand(const char* aCommand)=0;
    NS_IMETHOD_(void) SetCommand(eParserCommands aParserCommand)=0;

    /**
     *  Call this method once you've created a parser, and want to instruct it
     *  about what charset to load
     *  
     *  @update  ftang 4/23/99
     *  @param   aCharset- the charest of a document
     *  @param   aCharsetSource- the soure of the chares
     *  @return	 nada
     */
    NS_IMETHOD_(void) SetDocumentCharset(const nsACString& aCharset, PRInt32 aSource)=0;
    NS_IMETHOD_(void) GetDocumentCharset(nsACString& oCharset, PRInt32& oSource)=0;

    NS_IMETHOD_(void) SetParserFilter(nsIParserFilter* aFilter) = 0;

    /** 
     * Get the channel associated with this parser
     * @update harishd,gagan 07/17/01
     * @param aChannel out param that will contain the result
     * @return NS_OK if successful
     */
    NS_IMETHOD GetChannel(nsIChannel** aChannel) = 0;

    /** 
     * Get the DTD associated with this parser
     * @update vidur 9/29/99
     * @param aDTD out param that will contain the result
     * @return NS_OK if successful, NS_ERROR_FAILURE for runtime error
     */
    NS_IMETHOD GetDTD(nsIDTD** aDTD) = 0;
    
    /**
     * Get the nsIStreamListener for this parser
     * @param aDTD out param that will contain the result
     * @return NS_OK if successful
     */
    NS_IMETHOD GetStreamListener(nsIStreamListener** aListener) = 0;

    /**************************************************************************
     *  Parse methods always begin with an input source, and perform
     *  conversions until you wind up being emitted to the given contentsink
     *  (which may or may not be a proxy for the NGLayout content model).
     ************************************************************************/
    
    // Call this method to resume the parser from an unblocked state.
    // This can happen, for example, if parsing was interrupted and then the
    // consumer needed to restart the parser without waiting for more data.
    // This also happens after loading scripts, which unblock the parser in
    // order to process the output of document.write() and then need to
    // continue on with the page load on an enabled parser.
    NS_IMETHOD ContinueInterruptedParsing() = 0;
    
    // Stops parsing temporarily.
    NS_IMETHOD_(void) BlockParser() = 0;
    
    // Open up the parser for tokenization, building up content 
    // model..etc. However, this method does not resume parsing 
    // automatically. It's the callers' responsibility to restart
    // the parsing engine.
    NS_IMETHOD_(void) UnblockParser() = 0;

    NS_IMETHOD_(PRBool) IsParserEnabled() = 0;
    NS_IMETHOD_(PRBool) IsComplete() = 0;
    
    NS_IMETHOD Parse(nsIURI* aURL,
                     nsIRequestObserver* aListener = nsnull,
                     void* aKey = 0,
                     nsDTDMode aMode = eDTDMode_autodetect) = 0;
    NS_IMETHOD Parse(const nsAString& aSourceBuffer,
                     void* aKey,
                     const nsACString& aMimeType,
                     PRBool aLastCall,
                     nsDTDMode aMode = eDTDMode_autodetect) = 0;

    // Return a key, suitable for passing into one of the Parse methods above,
    // that will cause this parser to use the root context.
    NS_IMETHOD_(void *) GetRootContextKey() = 0;
    
    NS_IMETHOD Terminate(void) = 0;

    /**
     * This method gets called when you want to parse a fragment of HTML or XML
     * surrounded by the context |aTagStack|. It requires that the parser have
     * been given a fragment content sink.
     *
     * @param aSourceBuffer The XML or HTML that hasn't been parsed yet.
     * @param aKey The key used by the parser.
     * @param aTagStack The context of the source buffer.
     * @param aXMLMode Whether this is XML or HTML
     * @param aContentType The content-type of this document.
     * @param aMode The DTDMode that the parser should parse this fragment in.
     * @return Success or failure.
     */
    NS_IMETHOD ParseFragment(const nsAString& aSourceBuffer,
                             void* aKey,
                             nsTArray<nsString>& aTagStack,
                             PRBool aXMLMode,
                             const nsACString& aContentType,
                             nsDTDMode aMode = eDTDMode_autodetect) = 0;

    NS_IMETHOD ParseFragment(const nsAString& aSourceBuffer,
                             nsIContent* aTargetNode,
                             nsIAtom* aContextLocalName,
                             PRInt32 aContextNamespace,
                             PRBool aQuirks) = 0;

    /**
     * This method gets called when the tokens have been consumed, and it's time
     * to build the model via the content sink.
     * @update	gess5/11/98
     * @return  error code -- 0 if model building went well .
     */
    NS_IMETHOD BuildModel(void) = 0;

    /**
     *  Call this method to cancel any pending parsing events.
     *  Parsing events may be pending if all of the document's content
     *  has been passed to the parser but the parser has been interrupted
     *  because processing the tokens took too long.
     *  
     *  @update  kmcclusk 05/18/01
     *  @return  NS_OK if succeeded else ERROR.
     */

    NS_IMETHOD CancelParsingEvents() = 0;

    virtual void Reset() = 0;

    /**
     * True if the parser can currently be interrupted. Returns false when
     * parsing for example document.write or innerHTML.
     */
    virtual PRBool CanInterrupt() = 0;

    /**
     * True if the insertion point (per HTML5) is defined.
     */
    virtual PRBool IsInsertionPointDefined() = 0;

    /**
     * Call immediately before starting to evaluate a parser-inserted script.
     */
    virtual void BeginEvaluatingParserInsertedScript() = 0;

    /**
     * Call immediately after having evaluated a parser-inserted script.
     */
    virtual void EndEvaluatingParserInsertedScript() = 0;

    /**
     * Marks the HTML5 parser as not a script-created parser.
     */
    virtual void MarkAsNotScriptCreated() = 0;

    /**
     * True if this is a script-created HTML5 parser.
     */
    virtual PRBool IsScriptCreated() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIParser, NS_IPARSER_IID)

/* ===========================================================*
  Some useful constants...
 * ===========================================================*/

#include "prtypes.h"
#include "nsError.h"

#define NS_ERROR_HTMLPARSER_EOF                            NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1000)
#define NS_ERROR_HTMLPARSER_UNKNOWN                        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1001)
#define NS_ERROR_HTMLPARSER_CANTPROPAGATE                  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1002)
#define NS_ERROR_HTMLPARSER_CONTEXTMISMATCH                NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1003)
#define NS_ERROR_HTMLPARSER_BADFILENAME                    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1004)
#define NS_ERROR_HTMLPARSER_BADURL                         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1005)
#define NS_ERROR_HTMLPARSER_INVALIDPARSERCONTEXT           NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1006)
#define NS_ERROR_HTMLPARSER_INTERRUPTED                    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1007)
#define NS_ERROR_HTMLPARSER_BLOCK                          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1008)
#define NS_ERROR_HTMLPARSER_BADTOKENIZER                   NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1009)
#define NS_ERROR_HTMLPARSER_BADATTRIBUTE                   NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1010)
#define NS_ERROR_HTMLPARSER_UNRESOLVEDDTD                  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1011)
#define NS_ERROR_HTMLPARSER_MISPLACEDTABLECONTENT          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1012)
#define NS_ERROR_HTMLPARSER_BADDTD                         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1013)
#define NS_ERROR_HTMLPARSER_BADCONTEXT                     NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1014)
#define NS_ERROR_HTMLPARSER_STOPPARSING                    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1015)
#define NS_ERROR_HTMLPARSER_UNTERMINATEDSTRINGLITERAL      NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1016)
#define NS_ERROR_HTMLPARSER_HIERARCHYTOODEEP               NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1017)
#define NS_ERROR_HTMLPARSER_FAKE_ENDTAG                    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1018)
#define NS_ERROR_HTMLPARSER_INVALID_COMMENT                NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1019)

#define NS_ERROR_HTMLPARSER_CONTINUE              NS_OK


const PRUint32  kEOF              = NS_ERROR_HTMLPARSER_EOF;
const PRUint32  kUnknownError     = NS_ERROR_HTMLPARSER_UNKNOWN;
const PRUint32  kCantPropagate    = NS_ERROR_HTMLPARSER_CANTPROPAGATE;
const PRUint32  kContextMismatch  = NS_ERROR_HTMLPARSER_CONTEXTMISMATCH;
const PRUint32  kBadFilename      = NS_ERROR_HTMLPARSER_BADFILENAME;
const PRUint32  kBadURL           = NS_ERROR_HTMLPARSER_BADURL;
const PRUint32  kInvalidParserContext = NS_ERROR_HTMLPARSER_INVALIDPARSERCONTEXT;
const PRUint32  kBlocked          = NS_ERROR_HTMLPARSER_BLOCK;
const PRUint32  kBadStringLiteral = NS_ERROR_HTMLPARSER_UNTERMINATEDSTRINGLITERAL;
const PRUint32  kHierarchyTooDeep = NS_ERROR_HTMLPARSER_HIERARCHYTOODEEP;
const PRUint32  kFakeEndTag       = NS_ERROR_HTMLPARSER_FAKE_ENDTAG;
const PRUint32  kNotAComment      = NS_ERROR_HTMLPARSER_INVALID_COMMENT;

const PRUnichar  kNewLine          = '\n';
const PRUnichar  kCR               = '\r';
const PRUnichar  kLF               = '\n';
const PRUnichar  kTab              = '\t';
const PRUnichar  kSpace            = ' ';
const PRUnichar  kQuote            = '"';
const PRUnichar  kApostrophe       = '\'';
const PRUnichar  kLessThan         = '<';
const PRUnichar  kGreaterThan      = '>';
const PRUnichar  kAmpersand        = '&';
const PRUnichar  kForwardSlash     = '/';
const PRUnichar  kBackSlash        = '\\';
const PRUnichar  kEqual            = '=';
const PRUnichar  kMinus            = '-';
const PRUnichar  kPlus             = '+';
const PRUnichar  kExclamation      = '!';
const PRUnichar  kSemicolon        = ';';
const PRUnichar  kHashsign         = '#';
const PRUnichar  kAsterisk         = '*';
const PRUnichar  kUnderbar         = '_';
const PRUnichar  kComma            = ',';
const PRUnichar  kLeftParen        = '(';
const PRUnichar  kRightParen       = ')';
const PRUnichar  kLeftBrace        = '{';
const PRUnichar  kRightBrace       = '}';
const PRUnichar  kQuestionMark     = '?';
const PRUnichar  kLeftSquareBracket  = '[';
const PRUnichar  kRightSquareBracket = ']';
const PRUnichar kNullCh           = '\0';

#define NS_IPARSER_FLAG_UNKNOWN_MODE         0x00000000
#define NS_IPARSER_FLAG_QUIRKS_MODE          0x00000002
#define NS_IPARSER_FLAG_STRICT_MODE          0x00000004
#define NS_IPARSER_FLAG_AUTO_DETECT_MODE     0x00000010
#define NS_IPARSER_FLAG_VIEW_NORMAL          0x00000020
#define NS_IPARSER_FLAG_VIEW_SOURCE          0x00000040
#define NS_IPARSER_FLAG_VIEW_ERRORS          0x00000080
#define NS_IPARSER_FLAG_PLAIN_TEXT           0x00000100
#define NS_IPARSER_FLAG_XML                  0x00000200
#define NS_IPARSER_FLAG_HTML                 0x00000400
#define NS_IPARSER_FLAG_SCRIPT_ENABLED       0x00000800
#define NS_IPARSER_FLAG_FRAMES_ENABLED       0x00001000

#endif 
