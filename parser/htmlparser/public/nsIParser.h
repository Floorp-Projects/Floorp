/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef NS_IPARSER___
#define NS_IPARSER___


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 *  
 *  This class defines the iparser interface. This XPCOM
 *  inteface is all that parser clients ever need to see.
 *
 **/

#include "nsISupports.h"
#include "nsIStreamListener.h"
#include "nsIDTD.h"
#include "nsIInputStream.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"

#define NS_IPARSER_IID      \
  {0x355cbba0, 0xbf7d,  0x11d1,  \
  {0xaa, 0xd9, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}

// {41421C60-310A-11d4-816F-000064657374}
#define NS_IDEBUG_DUMP_CONTENT_IID \
{ 0x41421c60, 0x310a, 0x11d4, { 0x81, 0x6f, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } };

class nsIContentSink;
class nsIRequestObserver;
class nsIParserFilter;
class nsString;
class nsIURI;
class nsIChannel;

enum eParserCommands {
  eViewNormal,
  eViewSource,
  eViewErrors
};

enum eCRCQuality {
  eCRCGood = 0,
  eCRCFair,
  eCRCPoor
};


enum eParserDocType {
  ePlainText = 0,
  eXML,
  eHTML_Quirks,
  eHTML3_Quirks, // separate, for editor output, since HTML pre-4.0 lacks tbody
  eHTML_Strict
};


typedef enum {
   kCharsetUninitialized = 0,
   kCharsetFromWeakDocTypeDefault,
   kCharsetFromUserDefault ,
   kCharsetFromDocTypeDefault,
   kCharsetFromCache,
   kCharsetFromParentFrame,
   kCharsetFromBookmarks,
   kCharsetFromAutoDetection,
   kCharsetFromMetaTag,
   kCharsetFromByteOrderMark,
   kCharsetFromHTTPHeader,
   kCharsetFromUserForced,
   kCharsetFromOtherComponent,
   kCharsetFromPreviousLoading
} nsCharsetSource;

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
  static const nsIID& GetIID() { static nsIID iid = NS_IDEBUG_DUMP_CONTENT_IID; return iid; }
  NS_IMETHOD DumpContentModel()=0;
};

/**
 *  This class defines the iparser interface. This XPCOM
 *  inteface is all that parser clients ever need to see.
 *  
 *  @update  gess 3/25/98
 */
class nsIParser : public nsISupports {
  public:

    static const nsIID& GetIID() { static nsIID iid = NS_IPARSER_IID; return iid; }

    /**
     *  Call this method if you have a DTD that you want to share with the parser.
	   *  Registered DTD's get remembered until the system shuts down.
     *  
     *  @update  gess 3/25/98
     *  @param   aDTD -- ptr DTD that you're publishing the services of
     */
    virtual void RegisterDTD(nsIDTD* aDTD)=0;


    /**
     * Select given content sink into parser for parser output
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* SetContentSink(nsIContentSink* aSink)=0;


    /**
     * retrive the sink set into the parser 
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* GetContentSink(void)=0;

    /**
     *  Call this method once you've created a parser, and want to instruct it
	   *  about the command which caused the parser to be constructed. For example,
     *  this allows us to select a DTD which can do, say, view-source.
     *  
     *  @update  gess 3/25/98
     *  @param   aCommand -- ptrs to string that contains command
     *  @return	 nada
     */
    virtual void GetCommand(nsString& aCommand)=0;
    virtual void SetCommand(const char* aCommand)=0;
    virtual void SetCommand(eParserCommands aParserCommand)=0;

    /**
     *  Call this method once you've created a parser, and want to instruct it
     *  about what charset to load
     *  
     *  @update  ftang 4/23/99
     *  @param   aCharset- the charest of a document
     *  @param   aCharsetSource- the soure of the chares
     *  @return	 nada
     */
    virtual void SetDocumentCharset(nsString& aCharset, nsCharsetSource aSource)=0;
    virtual void GetDocumentCharset(nsString& oCharset, nsCharsetSource& oSource)=0;

    virtual nsIParserFilter* SetParserFilter(nsIParserFilter* aFilter) = 0;

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

    /******************************************************************************************
     *  Parse methods always begin with an input source, and perform conversions 
     *  until you wind up being emitted to the given contentsink (which may or may not
	   *  be a proxy for the NGLayout content model).
     ******************************************************************************************/
    
    // Call this method to resume the parser from the blocked state..
    virtual nsresult  ContinueParsing()   =0;
    
    // Stops parsing temporarily.
    virtual void      BlockParser()     =0;
    
    // Open up the parser for tokenization, building up content 
    // model..etc. However, this method does not resume parsing 
    // automatically. It's the callers' responsibility to restart
    // the parsing engine.
    virtual void      UnblockParser()   =0;

    virtual PRBool    IsParserEnabled() =0;
    virtual PRBool    IsComplete() =0;
    
    virtual nsresult  Parse(nsIURI* aURL,nsIRequestObserver* aListener = nsnull,PRBool aEnableVerify=PR_FALSE, void* aKey=0,nsDTDMode aMode=eDTDMode_autodetect) = 0;
    virtual nsresult	Parse(nsIInputStream& aStream, const nsAReadableString& aMimeType,PRBool aEnableVerify=PR_FALSE, void* aKey=0,nsDTDMode aMode=eDTDMode_autodetect) = 0;
    virtual nsresult  Parse(const nsAReadableString& aSourceBuffer,void* aKey,const nsAReadableString& aContentType,PRBool aEnableVerify,PRBool aLastCall,nsDTDMode aMode=eDTDMode_autodetect) = 0;
    
    virtual nsresult  Terminate(void) = 0;

    virtual nsresult  ParseFragment(const nsAReadableString& aSourceBuffer,
                                    void* aKey,
                                    nsVoidArray& aTagStack,
                                    PRUint32 anInsertPos,
                                    const nsAReadableString& aContentType,
                                    nsDTDMode aMode=eDTDMode_autodetect) = 0;

    /**
     * This method gets called when the tokens have been consumed, and it's time
     * to build the model via the content sink.
     * @update	gess5/11/98
     * @return  error code -- 0 if model building went well .
     */
    virtual nsresult  BuildModel(void)=0;


    /**
     *  Retrieve the parse mode from the parser...
     *  
     *  @update  gess 6/9/98
     *  @return  ptr to scanner
     */
    virtual nsDTDMode GetParseMode(void)=0;

    /**
     *  Call this method to determine a DTD for a DOCTYPE
     *  
     *  @update  harishd 05/01/00
     *  @param   aDTD  -- Carries the deduced ( from DOCTYPE ) DTD.
     *  @param   aDocTypeStr -- A doctype for which a DTD is to be selected.
     *  @param   aMimeType   -- A mimetype for which a DTD is to be selected.
                                Note: aParseMode might be required.
     *  @param   aCommand    -- A command for which a DTD is to be selected.
     *  @param   aParseMode  -- Used with aMimeType to choose the correct DTD.
     *  @return  NS_OK if succeeded else ERROR.
     */
    NS_IMETHOD CreateCompatibleDTD(nsIDTD** aDTD, 
                                   nsString* aDocTypeStr,
                                   eParserCommands aCommand,
                                   const nsString* aMimeType=nsnull, 
                                   nsDTDMode aDTDMode=eDTDMode_unknown)=0;

    /**
     *  Call this method to cancel any pending parsing events.
     *  Parsing events may be pending if all of the document's content
     *  has been passed to the parser but the parser has been interrupted
     *  because processing the tokens took too long.
     *  
     *  @update  kmcclusk 05/18/01
     *  @return  NS_OK if succeeded else ERROR.
     */

    NS_IMETHOD CancelParsingEvents()=0;
};

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

#define kHTMLTextContentType  "text/html"
#define kXMLTextContentType   "text/xml"
#define kXMLApplicationContentType "application/xml"
#define kXHTMLApplicationContentType "application/xhtml+xml"
#define kXULTextContentType   "application/vnd.mozilla.xul+xml"
#define kRDFTextContentType   "text/rdf"
#define kXIFTextContentType   "text/xif"
#define kPlainTextContentType "text/plain"
#define kViewSourceCommand    "view-source"
#define kTextCSSContentType   "text/css"
#define kApplicationJSContentType   "application/x-javascript"
#define kTextJSContentType    "text/javascript"
#define kSGMLTextContentType   "text/sgml"


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

#endif 
