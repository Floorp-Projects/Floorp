/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
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


#include "nshtmlpars.h"
#include "nsISupports.h"
#include "nsIStreamListener.h"
#include "nsIDTD.h"
#include "nsIInputStream.h"


#define NS_IPARSER_IID      \
  {0x355cbba0, 0xbf7d,  0x11d1,  \
  {0xaa, 0xd9, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}



class nsIContentSink;
class nsIStreamObserver;
class nsIParserFilter;
class nsString;
class nsIURI;


enum  eParseMode {
  
  eParseMode_unknown=0,
  eParseMode_raptor,    //5.0 version of nav. and greater
  eParseMode_navigator, //pre 5.0 versions
  eParseMode_noquirks,  //pre 5.0 without quirks (as best as we can...)
  eParseMode_other,
  eParseMode_autodetect
};

enum eCRCQuality {
  eCRCGood = 0,
  eCRCFair,
  eCRCPoor
};


typedef enum {
   kCharsetUninitialized = 0,
   kCharsetFromWeakDocTypeDefault,
   kCharsetFromUserDefault ,
   kCharsetFromDocTypeDefault,
   kCharsetFromParentFrame,
   kCharsetFromAutoDetection,
   kCharsetFromMetaTag,
   kCharsetFromHTTPHeader,
   kCharsetFromUserForced,
   kCharsetFromOtherComponent,
   kCharsetFromPreviousLoading
} nsCharsetSource;

enum eStreamState {eNone,eOnStart,eOnDataAvail,eOnStop};


class nsITagStack {
public:
  virtual void        Push(PRUnichar* aTag)=0;
  virtual PRUnichar*  Pop(void)=0;
  virtual PRUnichar*  TagAt(PRUint32 anIndex)=0;
  virtual PRUint32    GetSize(void)=0;
};


/**
 *  This class defines the iparser interface. This XPCOM
 *  inteface is all that parser clients ever need to see.
 *  
 *  @update  gess 3/25/98
 */
CLASS_EXPORT_HTMLPARS nsIParser : public nsISupports {
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
    virtual void SetCommand(const char* aCommand)=0;

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

    virtual nsIParserFilter* SetParserFilter(nsIParserFilter* aFilter) = 0;

    /**
     * Call this to get a newly constructed tagstack
     * @update	gess 5/05/99
     * @param   aTagStack is an out parm that will contain your result
     * @return  NS_OK if successful, or NS_HTMLPARSER_MEMORY_ERROR on error
     */
    virtual nsresult  CreateTagStack(nsITagStack** aTagStack)=0;


    /******************************************************************************************
     *  Parse methods always begin with an input source, and perform conversions 
     *  until you wind up being emitted to the given contentsink (which may or may not
	   *  be a proxy for the NGLayout content model).
     ******************************************************************************************/
    virtual nsresult  EnableParser(PRBool aState) = 0;
    virtual PRBool    IsParserEnabled() = 0;
    virtual nsresult  Parse(nsIURI* aURL,nsIStreamObserver* aListener = nsnull,PRBool aEnableVerify=PR_FALSE, void* aKey=0,eParseMode aMode=eParseMode_autodetect) = 0;
    virtual nsresult	Parse(nsIInputStream& aStream, PRBool aEnableVerify=PR_FALSE, void* aKey=0,eParseMode aMode=eParseMode_autodetect) = 0;
    virtual nsresult  Parse(const nsString& aSourceBuffer,void* aKey,const nsString& aContentType,PRBool aEnableVerify,PRBool aLastCall,eParseMode aMode=eParseMode_autodetect) = 0;
    virtual nsresult  Terminate(void) = 0;

    virtual PRBool    IsValidFragment(const nsString& aSourceBuffer,nsITagStack& aStack,PRUint32 anInsertPos,const nsString& aContentType,eParseMode aMode=eParseMode_autodetect)=0;
    virtual nsresult  ParseFragment(const nsString& aSourceBuffer,void* aKey,nsITagStack& aStack,PRUint32 anInsertPos,const nsString& aContentType,eParseMode aMode=eParseMode_autodetect)=0;

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
    virtual eParseMode GetParseMode(void)=0;

};

/* ===========================================================*
  Some useful constants...
 * ===========================================================*/

#include "prtypes.h"
#include "nsError.h"

#define NS_ERROR_HTMLPARSER_EOF                   NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1000)
#define NS_ERROR_HTMLPARSER_UNKNOWN               NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1001)
#define NS_ERROR_HTMLPARSER_CANTPROPAGATE         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1002)
#define NS_ERROR_HTMLPARSER_CONTEXTMISMATCH       NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1003)
#define NS_ERROR_HTMLPARSER_BADFILENAME           NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1004)
#define NS_ERROR_HTMLPARSER_BADURL                NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1005)
#define NS_ERROR_HTMLPARSER_INVALIDPARSERCONTEXT  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1006)
#define NS_ERROR_HTMLPARSER_INTERRUPTED           NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1007)
#define NS_ERROR_HTMLPARSER_BLOCK                 NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1008)
#define NS_ERROR_HTMLPARSER_BADTOKENIZER          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1009)
#define NS_ERROR_HTMLPARSER_BADATTRIBUTE          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1010)
#define NS_ERROR_HTMLPARSER_UNRESOLVEDDTD         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1011)
#define NS_ERROR_HTMLPARSER_MISPLACED             NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1012)
#define NS_ERROR_HTMLPARSER_BADDTD                NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1013)
#define NS_ERROR_HTMLPARSER_BADCONTEXT            NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1014)
#define NS_ERROR_HTMLPARSER_STOPPARSING           NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_HTMLPARSER,1015)

#define NS_ERROR_HTMLPARSER_CONTINUE              NS_OK


const PRUint32  kEOF              = NS_ERROR_HTMLPARSER_EOF;
const PRUint32  kUnknownError     = NS_ERROR_HTMLPARSER_UNKNOWN;
const PRUint32  kCantPropagate    = NS_ERROR_HTMLPARSER_CANTPROPAGATE;
const PRUint32  kContextMismatch  = NS_ERROR_HTMLPARSER_CONTEXTMISMATCH;
const PRUint32  kBadFilename      = NS_ERROR_HTMLPARSER_BADFILENAME;
const PRUint32  kBadURL           = NS_ERROR_HTMLPARSER_BADURL;
const PRUint32  kInvalidParserContext = NS_ERROR_HTMLPARSER_INVALIDPARSERCONTEXT;
const PRUint32  kBlocked          = NS_ERROR_HTMLPARSER_BLOCK;

const PRUint32  kNewLine          = '\n';
const PRUint32  kCR               = '\r';
const PRUint32  kLF               = '\n';
const PRUint32  kTab              = '\t';
const PRUint32  kSpace            = ' ';
const PRUint32  kQuote            = '"';
const PRUint32  kApostrophe       = '\'';
const PRUint32  kLessThan         = '<';
const PRUint32  kGreaterThan      = '>';
const PRUint32  kAmpersand        = '&';
const PRUint32  kForwardSlash     = '/';
const PRUint32  kBackSlash        = '\\';
const PRUint32  kEqual            = '=';
const PRUint32  kMinus            = '-';
const PRUint32  kPlus             = '+';
const PRUint32  kExclamation      = '!';
const PRUint32  kSemicolon        = ';';
const PRUint32  kHashsign         = '#';
const PRUint32  kAsterisk         = '*';
const PRUint32  kUnderbar         = '_';
const PRUint32  kComma            = ',';
const PRUint32  kLeftParen        = '(';
const PRUint32  kRightParen       = ')';
const PRUint32  kLeftBrace        = '{';
const PRUint32  kRightBrace       = '}';
const PRUint32  kQuestionMark     = '?';
const PRUint32  kLeftSquareBracket  = '[';
const PRUint32  kRightSquareBracket = ']';
const PRUnichar kNullCh           = '\0';

#define kHTMLTextContentType  "text/html"
#define kXMLTextContentType   "text/xml"
#define kXULTextContentType   "text/xul"
#define kRDFTextContentType   "text/rdf"
#define kXIFTextContentType   "text/xif"
#define kPlainTextContentType "text/plain"
#define kViewSourceCommand    "view-source"

#endif 
