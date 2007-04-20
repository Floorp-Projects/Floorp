/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
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
 *   jce2@po.cwru.edu <Jason Eager>: Added pref to turn on/off
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *   rbs@maths.uq.edu.au
 *   Andreas M. Schneider <clarence@clarence.de>
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

/*
 * Set NS_VIEWSOURCE_TOKENS_PER_BLOCK to 0 to disable multi-block
 * output.  Multi-block output helps reduce the amount of bidi
 * processing we have to do on the resulting content model.
 */
#define NS_VIEWSOURCE_TOKENS_PER_BLOCK 16

#ifdef RAPTOR_PERF_METRICS
#  define START_TIMER()                    \
    if(mParser) mParser->mParseTime.Start(PR_FALSE); \
    if(mParser) mParser->mDTDTime.Start(PR_FALSE); 

#  define STOP_TIMER()                     \
    if(mParser) mParser->mParseTime.Stop(); \
    if(mParser) mParser->mDTDTime.Stop(); 

#else
#  define STOP_TIMER() 
#  define START_TIMER()
#endif

#include "nsIAtom.h"
#include "nsViewSourceHTML.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsIParser.h"
#include "nsDTDUtils.h"
#include "nsIContentSink.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLTokenizer.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"

#include "nsIServiceManager.h"

#include "nsElementTable.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"
#include "prmem.h"

#ifdef RAPTOR_PERF_METRICS
#include "stopwatch.h"
Stopwatch vsTimer;
#endif



// Define this to dump the viewsource stuff to a file
//#define DUMP_TO_FILE
#ifdef DUMP_TO_FILE
#include <stdio.h>
  FILE* gDumpFile=0;
  static const char* gDumpFileName = "/tmp/viewsource.html";
//  static const char* gDumpFileName = "\\temp\\viewsource.html";
#endif // DUMP_TO_FILE

// bug 22022 - these are used to toggle 'Wrap Long Lines' on the viewsource
// window by selectively setting/unsetting the following class defined in
// viewsource.css; the setting is remembered between invocations using a pref.
static const char kBodyId[] = "viewsource";
static const char kBodyClassWrap[] = "wrap";

NS_IMPL_ISUPPORTS1(CViewSourceHTML, nsIDTD)

/********************************************
 ********************************************/

class CIndirectTextToken : public CTextToken {
public:
  CIndirectTextToken() : CTextToken() {
    mIndirectString=0;
  }
  
  void SetIndirectString(const nsSubstring& aString) {
    mIndirectString=&aString;
  }

  virtual const nsSubstring& GetStringValue(void){
    return (const nsSubstring&)*mIndirectString;
  }

  const nsSubstring* mIndirectString;
};


/*******************************************************************
  Now define the CSharedVSCOntext class...
 *******************************************************************/

class CSharedVSContext {
public:

  CSharedVSContext() {
  }
  
  ~CSharedVSContext() {
  }

  static CSharedVSContext& GetSharedContext() {
    static CSharedVSContext gSharedVSContext;
    return gSharedVSContext;
  }

  nsCParserNode       mEndNode;
  nsCParserStartNode  mStartNode;
  nsCParserStartNode  mTokenNode;
  CIndirectTextToken  mITextToken;
  nsCParserStartNode  mErrorNode;
  nsCParserNode       mEndErrorNode;
};

enum {
  kStartTag = 0,
  kEndTag,
  kComment,
  kCData,
  kDoctype,
  kPI,
  kEntity,
  kText,
  kAttributeName,
  kAttributeValue,
  kMarkupDecl
};

static const char* const kElementClasses[] = {
  "start-tag",
  "end-tag",
  "comment",
  "cdata",
  "doctype",
  "pi",
  "entity",
  "text",
  "attribute-name",
  "attribute-value",
  "markupdeclaration"  
};

static const char* const kBeforeText[] = {
  "<",
  "</",
  "",
  "",
  "",
  "",
  "&",
  "",
  "",
  "=",
  ""
};

static const char* const kAfterText[] = {
  ">",
  ">",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  ""
};

#ifdef DUMP_TO_FILE
static const char* const kDumpFileBeforeText[] = {
  "&lt;",
  "&lt;/",
  "",
  "",
  "",
  "",
  "&amp;",
  "",
  "",
  "=",
  ""
};

static const char* const kDumpFileAfterText[] = {
  "&gt;",
  "&gt;",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  ""
};
#endif // DUMP_TO_FILE

/**
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CViewSourceHTML::CViewSourceHTML()
{
  mSyntaxHighlight = PR_FALSE;
  mWrapLongLines = PR_FALSE;
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch) {
    PRBool temp;
    nsresult rv;
    rv = prefBranch->GetBoolPref("view_source.syntax_highlight", &temp);
    mSyntaxHighlight = NS_SUCCEEDED(rv) ? temp : PR_TRUE;

    rv = prefBranch->GetBoolPref("view_source.wrap_long_lines", &temp);
    mWrapLongLines = NS_SUCCEEDED(rv) ? temp : PR_FALSE;
  }

  mParser = 0;
  mSink = 0;
  mLineNumber = 1;
  mTokenizer = 0;
  mDocType=eHTML3_Quirks; // why?
  mHasOpenRoot=PR_FALSE;
  mHasOpenBody=PR_FALSE;

  mTokenCount=0;

#ifdef DUMP_TO_FILE
  gDumpFile = fopen(gDumpFileName,"w");
#endif // DUMP_TO_FILE

}



/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CViewSourceHTML::~CViewSourceHTML(){
  mParser=0; //just to prove we destructed...
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	rickg 03.20.2000
  * @param	aParserContext
  * @param	aSink
  * @return	error code (almost always 0)
  */
nsresult CViewSourceHTML::WillBuildModel(const CParserContext& aParserContext,
                                         nsITokenizer* aTokenizer,
                                         nsIContentSink* aSink){

  nsresult result=NS_OK;

#ifdef RAPTOR_PERF_METRICS
  vsTimer.Reset();
  NS_START_STOPWATCH(vsTimer);
#endif 

  STOP_TIMER();
  mSink=(nsIHTMLContentSink*)aSink;

  if((!aParserContext.mPrevContext) && (mSink)) {

    nsAString & contextFilename = aParserContext.mScanner->GetFilename();
    mFilename = Substring(contextFilename,
                          12, // The length of "view-source:"
                          contextFilename.Length() - 12);
    
    mDocType=aParserContext.mDocType;
    mMimeType=aParserContext.mMimeType;
    mDTDMode=aParserContext.mDTDMode;
    mParserCommand=aParserContext.mParserCommand;
    mTokenizer = aTokenizer;

#ifdef DUMP_TO_FILE
    if (gDumpFile) {

      fprintf(gDumpFile, "<html>\n");
      fprintf(gDumpFile, "<head>\n");
      fprintf(gDumpFile, "<title>");
      fprintf(gDumpFile, "Source of: ");
      fputs(NS_ConvertUTF16toUTF8(mFilename).get(), gDumpFile);
      fprintf(gDumpFile, "</title>\n");
      fprintf(gDumpFile, "<link rel=\"stylesheet\" type=\"text/css\" href=\"resource://gre/res/viewsource.css\">\n");
      fprintf(gDumpFile, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n");
      fprintf(gDumpFile, "</head>\n");
      fprintf(gDumpFile, "<body id=\"viewsource\">\n");
      fprintf(gDumpFile, "<pre id=\"line1\">\n");
    }
#endif //DUMP_TO_FILE
  }


  if(eViewSource!=aParserContext.mParserCommand)
    mDocType=ePlainText;
  else mDocType=aParserContext.mDocType;

  mLineNumber = 1;
  // Munge the DTD mode so that the document will be in standards mode even if
  // the original source was quirks.  The CONST_CAST is evil, but the other
  // options seem to be:
  // 1) Change the WillBuildModel signature to take an nsIParser so that we can
  //    push a new parser context right here.
  // 2) Make some assumptions about the exact class of mSink and get at the
  //    document that way.
  // #1 doesn't seem worth it, and #2 is even more evil, since we plan to reset
  // the DTD mode right back to what it was before, let's risk this.
  CParserContext& parserContext = NS_CONST_CAST(CParserContext&, aParserContext);
  parserContext.mDTDMode = eDTDMode_full_standards;
  result = mSink->WillBuildModel();
  // And reset the DTD mode back to the right one
  parserContext.mDTDMode = mDTDMode;
  START_TIMER();
  return result;
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	gess5/18/98
  * @param	aFilename is the name of the file being parsed.
  * @return	error code (almost always 0)
  */
NS_IMETHODIMP CViewSourceHTML::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  if(aTokenizer && aParser) {

    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    nsTokenAllocator* theAllocator=mTokenizer->GetTokenAllocator();

    if(!mHasOpenRoot) {
      // For the stack-allocated tokens below, it's safe to pass a null
      // token allocator, because there are no attributes on the tokens.
      CStartToken htmlToken(NS_LITERAL_STRING("HTML"), eHTMLTag_html);
      nsCParserNode htmlNode(&htmlToken, 0/*stack token*/);
      mSink->OpenContainer(htmlNode);

      CStartToken headToken(NS_LITERAL_STRING("HEAD"), eHTMLTag_head);
      nsCParserNode headNode(&headToken, 0/*stack token*/);
      mSink->OpenContainer(headNode);

      CStartToken titleToken(NS_LITERAL_STRING("TITLE"), eHTMLTag_title);
      nsCParserNode titleNode(&titleToken, 0/*stack token*/);
      mSink->OpenContainer(titleNode);

      // Note that XUL will automatically add the prefix "Source of: "
      if (StringBeginsWith(mFilename, NS_LITERAL_STRING("data:")) &&
          mFilename.Length() > 50) {
        nsAutoString dataFilename(Substring(mFilename, 0, 50));
        dataFilename.AppendLiteral("...");
        CTextToken titleText(dataFilename);
        nsCParserNode titleTextNode(&titleText, 0/*stack token*/);
        mSink->AddLeaf(titleTextNode);
      } else {
        CTextToken titleText(mFilename);
        nsCParserNode titleTextNode(&titleText, 0/*stack token*/);
        mSink->AddLeaf(titleTextNode);
      }

      mSink->CloseContainer(eHTMLTag_title);

      if (theAllocator) {
        CStartToken* theToken=
          NS_STATIC_CAST(CStartToken*,
                         theAllocator->CreateTokenOfType(eToken_start,
                                                         eHTMLTag_link,
                                                         NS_LITERAL_STRING("LINK")));
        if (theToken) {
          nsCParserStartNode theNode(theToken, theAllocator);

          AddAttrToNode(theNode, theAllocator,
                        NS_LITERAL_STRING("rel"),
                        NS_LITERAL_STRING("stylesheet"));

          AddAttrToNode(theNode, theAllocator,
                        NS_LITERAL_STRING("type"),
                        NS_LITERAL_STRING("text/css"));

          AddAttrToNode(theNode, theAllocator,
                        NS_LITERAL_STRING("href"),
                        NS_LITERAL_STRING("resource://gre/res/viewsource.css"));
          
          mSink->AddLeaf(theNode);
        }
      }

      result = mSink->CloseContainer(eHTMLTag_head);
      if(NS_SUCCEEDED(result)) {
        mHasOpenRoot = PR_TRUE;
      }
    }
    if (NS_SUCCEEDED(result) && !mHasOpenBody) {
      if (theAllocator) {
        CStartToken* bodyToken=
          NS_STATIC_CAST(CStartToken*,
                         theAllocator->CreateTokenOfType(eToken_start,
                                                         eHTMLTag_body,
                                                         NS_LITERAL_STRING("BODY")));
        if (bodyToken) {
          nsCParserStartNode bodyNode(bodyToken, theAllocator);

          AddAttrToNode(bodyNode, theAllocator,
                        NS_LITERAL_STRING("id"),
                        NS_ConvertASCIItoUTF16(kBodyId));
          
          if (mWrapLongLines) {
            AddAttrToNode(bodyNode, theAllocator,
                          NS_LITERAL_STRING("class"),
                          NS_ConvertASCIItoUTF16(kBodyClassWrap));
          }
          result = mSink->OpenContainer(bodyNode);
          if(NS_SUCCEEDED(result)) mHasOpenBody=PR_TRUE;
        }
        
        if (NS_SUCCEEDED(result)) {
          CStartToken* preToken =
            NS_STATIC_CAST(CStartToken*,
                           theAllocator->CreateTokenOfType(eToken_start,
                                                           eHTMLTag_pre,
                                                           NS_LITERAL_STRING("PRE")));
          if (preToken) {
            nsCParserStartNode preNode(preToken, theAllocator);
            AddAttrToNode(preNode, theAllocator,
                          NS_LITERAL_STRING("id"),
                          NS_LITERAL_STRING("line1"));
            result = mSink->OpenContainer(preNode);
          } else {
            result = NS_ERROR_OUT_OF_MEMORY;
          }
        }
      }
    }

    mSink->WillProcessTokens();

    while(NS_SUCCEEDED(result)){
      CToken* theToken=mTokenizer->PopToken();
      if(theToken) {
        result=HandleToken(theToken,aParser);
        if(NS_SUCCEEDED(result)) {
          IF_FREE(theToken, mTokenizer->GetTokenAllocator());
          if (mParser->CanInterrupt() &&
              mSink->DidProcessAToken() == NS_ERROR_HTMLPARSER_INTERRUPTED) {
            result = NS_ERROR_HTMLPARSER_INTERRUPTED;
            break;
          }
        } else {
          mTokenizer->PushTokenFront(theToken);
        }
      }
      else break;
    }//while
   
    mTokenizer=oldTokenizer;
  }
  else result=NS_ERROR_HTMLPARSER_BADTOKENIZER;
  return result;
}

/**
 * Call this to start a new PRE block.  See bug 86355 for why this
 * makes some pages much faster.
 */
void CViewSourceHTML::StartNewPreBlock(void){
  CEndToken endToken(eHTMLTag_pre);
  nsCParserNode endNode(&endToken, 0/*stack token*/);
  mSink->CloseContainer(eHTMLTag_pre);

  nsTokenAllocator* theAllocator = mTokenizer->GetTokenAllocator();
  if (!theAllocator) {
    return;
  }
  
  CStartToken* theToken =
    NS_STATIC_CAST(CStartToken*,
                   theAllocator->CreateTokenOfType(eToken_start,
                                                   eHTMLTag_pre,
                                                   NS_LITERAL_STRING("PRE")));
  if (!theToken) {
    return;
  }

  nsCParserStartNode startNode(theToken, theAllocator);
  AddAttrToNode(startNode, theAllocator,
                NS_LITERAL_STRING("id"),
                NS_ConvertASCIItoUTF16(nsPrintfCString("line%d", mLineNumber)));
  mSink->OpenContainer(startNode);
  
#ifdef DUMP_TO_FILE
  if (gDumpFile) {
    fprintf(gDumpFile, "</pre>\n");
    fprintf(gDumpFile, "<pre id=\"line%d\">\n", mLineNumber);
  }
#endif // DUMP_TO_FILE

  mTokenCount = 0;
}

void CViewSourceHTML::AddAttrToNode(nsCParserStartNode& aNode,
                                    nsTokenAllocator* aAllocator,
                                    const nsAString& aAttrName,
                                    const nsAString& aAttrValue)
{
  NS_PRECONDITION(aAllocator, "Must have a token allocator!");
  
  CAttributeToken* theAttr =
    (CAttributeToken*) aAllocator->CreateTokenOfType(eToken_attribute,
                                                     eHTMLTag_unknown,
                                                     aAttrValue);
  if (!theAttr) {
    NS_ERROR("Failed to allocate attribute token");
    return;
  }

  theAttr->SetKey(aAttrName);
  aNode.AddAttribute(theAttr);
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result= NS_OK;

  //ADD CODE HERE TO CLOSE OPEN CONTAINERS...

  if(aParser){

    mParser=(nsParser*)aParser;  //debug XXX
    STOP_TIMER();

    mSink=(nsIHTMLContentSink*)aParser->GetContentSink();
    if((aNotifySink) && (mSink)) {
        //now let's close automatically auto-opened containers...

#ifdef DUMP_TO_FILE
      if(gDumpFile) {
        fprintf(gDumpFile, "</pre>\n");
        fprintf(gDumpFile, "</body>\n");
        fprintf(gDumpFile, "</html>\n");
        fclose(gDumpFile);
      }
#endif // DUMP_TO_FILE

      if(ePlainText!=mDocType) {
        mSink->CloseContainer(eHTMLTag_pre);
        mSink->CloseContainer(eHTMLTag_body);
        mSink->CloseContainer(eHTMLTag_html);
      }
      result = mSink->DidBuildModel();
    }

    START_TIMER();

  }

#ifdef RAPTOR_PERF_METRICS
  NS_STOP_STOPWATCH(vsTimer);
  printf("viewsource timer: ");
  vsTimer.Print();
  printf("\n");
#endif 

  return result;
}

/**
 * Use this id you want to stop the building content model
 * --------------[ Sets DTD to STOP mode ]----------------
 * It's recommended to use this method in accordance with
 * the parser's terminate() method.
 *
 * @update	harishd 07/22/99
 * @param 
 * @return
 */
NS_IMETHODIMP_(void)  
CViewSourceHTML::Terminate() {
}

NS_IMETHODIMP_(PRInt32)  
CViewSourceHTML::GetType() {
  return NS_IPARSER_FLAG_HTML;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::WillResumeParse(nsIContentSink* aSink){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillResume();
  }
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::WillInterruptParse(nsIContentSink* aSink){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillInterrupt();
  }
  return result;
}

/**
 * Called by the parser to enable/disable dtd verification of the
 * internal context stack.
 * @update	gess 7/23/98
 * @param 
 * @return
 */
void CViewSourceHTML::SetVerification(PRBool aEnabled)
{
}

/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 3/25/98
 *  @param   aParent -- int tag of parent container
 *  @param   aChild -- int tag of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CViewSourceHTML::CanContain(PRInt32 aParent,PRInt32 aChild) const{
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CViewSourceHTML::IsContainer(PRInt32 aTag) const{
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  This method gets called when a tag needs to write it's attributes
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteAttributes(PRInt32 attrCount, PRBool aOwnerInError) {
  nsresult result=NS_OK;
  
  if(attrCount){ //go collect the attributes...

    CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();

    int attr = 0;
    for(attr = 0; attr < attrCount; ++attr){
      CToken* theToken = mTokenizer->PeekToken();
      if(theToken)  {
        eHTMLTokenTypes theType = eHTMLTokenTypes(theToken->GetTokenType());
        if(eToken_attribute == theType){
          mTokenizer->PopToken(); //pop it for real...
          theContext.mTokenNode.AddAttribute(theToken);  //and add it to the node.

          CAttributeToken* theAttrToken = (CAttributeToken*)theToken;
          const nsSubstring& theKey = theAttrToken->GetKey();
          
          // The attribute is only in error if its owner is NOT in error.
          const PRBool attributeInError =
            !aOwnerInError && theAttrToken->IsInError();

          result = WriteTag(kAttributeName,theKey,0,attributeInError);
          const nsSubstring& theValue = theAttrToken->GetValue();

          if(!theValue.IsEmpty() || theAttrToken->mHasEqualWithoutValue){
            result = WriteTag(kAttributeValue,theValue,0,attributeInError);
          }
        } 
      }
      else return kEOF;
    }
  }

  return result;
}

/**
 *  This method gets called when a tag needs to be sent out
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteTag(PRInt32 aTagType,const nsSubstring & aText,PRInt32 attrCount,PRBool aTagInError) {
  nsresult result=NS_OK;

  // adjust line number to what it will be after we finish writing this tag
  // XXXbz life here sucks.  We can't use the GetNewlineCount on the token,
  // because Text tokens in <style>, <script>, etc lie through their teeth.
  // On the other hand, the parser messes up newline counting in some token
  // types (bug 137315).  So our line numbers will disagree with the parser's
  // in some cases...
  mLineNumber += aText.CountChar(PRUnichar('\n'));
  
  CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();

  nsTokenAllocator* theAllocator=mTokenizer->GetTokenAllocator();
  NS_ASSERTION(0!=theAllocator,"Error: no allocator");
  if(0==theAllocator)
    return NS_ERROR_FAILURE;

  // Highlight all parts of all erroneous tags.
  if (mSyntaxHighlight && aTagInError) {
    CStartToken* theTagToken=
      NS_STATIC_CAST(CStartToken*,
                     theAllocator->CreateTokenOfType(eToken_start,
                                                     eHTMLTag_span,
                                                     NS_LITERAL_STRING("SPAN")));
    NS_ENSURE_TRUE(theTagToken, NS_ERROR_OUT_OF_MEMORY);
    theContext.mErrorNode.Init(theTagToken, theAllocator);
    AddAttrToNode(theContext.mErrorNode, theAllocator,
                  NS_LITERAL_STRING("class"),
                  NS_LITERAL_STRING("error"));
    mSink->OpenContainer(theContext.mErrorNode);
#ifdef DUMP_TO_FILE
    if (gDumpFile) {
      fprintf(gDumpFile, "<span class=\"error\">");
    }
#endif
  }

  if (kBeforeText[aTagType][0] != 0) {
    NS_ConvertASCIItoUTF16 beforeText(kBeforeText[aTagType]);
    theContext.mITextToken.SetIndirectString(beforeText);
    nsCParserNode theNode(&theContext.mITextToken, 0/*stack token*/);
    mSink->AddLeaf(theNode);
  }
#ifdef DUMP_TO_FILE
  if (gDumpFile && kDumpFileBeforeText[aTagType][0])
    fprintf(gDumpFile, kDumpFileBeforeText[aTagType]);
#endif // DUMP_TO_FILE
  
  if (mSyntaxHighlight && aTagType != kText) {
    CStartToken* theTagToken=
      NS_STATIC_CAST(CStartToken*,
                     theAllocator->CreateTokenOfType(eToken_start,
                                                     eHTMLTag_span,
                                                     NS_LITERAL_STRING("SPAN")));
    NS_ENSURE_TRUE(theTagToken, NS_ERROR_OUT_OF_MEMORY);
    theContext.mStartNode.Init(theTagToken, theAllocator);
    AddAttrToNode(theContext.mStartNode, theAllocator,
                  NS_LITERAL_STRING("class"),
                  NS_ConvertASCIItoUTF16(kElementClasses[aTagType]));
    mSink->OpenContainer(theContext.mStartNode);  //emit <starttag>...
#ifdef DUMP_TO_FILE
    if (gDumpFile) {
      fprintf(gDumpFile, "<span class=\"");
      fprintf(gDumpFile, kElementClasses[aTagType]);
      fprintf(gDumpFile, "\">");
    }
#endif // DUMP_TO_FILE
  }

  STOP_TIMER();

  theContext.mITextToken.SetIndirectString(aText);  //now emit the tag name...

  nsCParserNode theNode(&theContext.mITextToken, 0/*stack token*/);
  mSink->AddLeaf(theNode);
#ifdef DUMP_TO_FILE
  if (gDumpFile) {
    fputs(NS_ConvertUTF16toUTF8(aText).get(), gDumpFile);
  }
#endif // DUMP_TO_FILE

  if (mSyntaxHighlight && aTagType != kText) {
    theContext.mStartNode.ReleaseAll(); 
    CEndToken theEndToken(eHTMLTag_span);
    theContext.mEndNode.Init(&theEndToken, 0/*stack token*/);
    mSink->CloseContainer(eHTMLTag_span);  //emit </endtag>...
#ifdef DUMP_TO_FILE
    if (gDumpFile)
      fprintf(gDumpFile, "</span>");
#endif //DUMP_TO_FILE
  }

  if(attrCount){
    result=WriteAttributes(attrCount, aTagInError);
  }

  // Tokens are set in error if their ending > is not there, so don't output 
  // the after-text
  if (!aTagInError && kAfterText[aTagType][0] != 0) {
    NS_ConvertASCIItoUTF16 afterText(kAfterText[aTagType]);
    theContext.mITextToken.SetIndirectString(afterText);
    nsCParserNode theNode(&theContext.mITextToken, 0/*stack token*/);
    mSink->AddLeaf(theNode);
  }
#ifdef DUMP_TO_FILE
  if (!aTagInError && gDumpFile && kDumpFileAfterText[aTagType][0])
    fprintf(gDumpFile, kDumpFileAfterText[aTagType]);
#endif // DUMP_TO_FILE

  if (mSyntaxHighlight && aTagInError) {
    theContext.mErrorNode.ReleaseAll(); 
    CEndToken theEndToken(eHTMLTag_span);
    theContext.mEndErrorNode.Init(&theEndToken, 0/*stack token*/);
    mSink->CloseContainer(eHTMLTag_span);  //emit </endtag>...
#ifdef DUMP_TO_FILE
    if (gDumpFile)
      fprintf(gDumpFile, "</span>");
#endif //DUMP_TO_FILE
  }

  START_TIMER();

  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
NS_IMETHODIMP CViewSourceHTML::HandleToken(CToken* aToken,nsIParser* aParser)
{
  nsresult        result=NS_OK;
  CHTMLToken*     theToken= (CHTMLToken*)(aToken);
  eHTMLTokenTypes theType= (eHTMLTokenTypes)theToken->GetTokenType();
 
  mParser=(nsParser*)aParser;
  mSink=(nsIHTMLContentSink*)aParser->GetContentSink();
 
  CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();
  theContext.mTokenNode.Init(theToken, mTokenizer->GetTokenAllocator());

  switch(theType) {
    
    case eToken_start:
      {
        const nsSubstring& startValue = aToken->GetStringValue();
        result = WriteTag(kStartTag,startValue,aToken->GetAttributeCount(),aToken->IsInError());

        if((ePlainText!=mDocType) && mParser && (NS_OK==result)) {
          result = mSink->NotifyTagObservers(&theContext.mTokenNode);
        }
      }
      break;

    case eToken_end:
      {
        const nsSubstring& endValue = aToken->GetStringValue();
        result = WriteTag(kEndTag,endValue,aToken->GetAttributeCount(),aToken->IsInError());
      }
      break;

    case eToken_cdatasection:
      {
        nsAutoString theStr;
        theStr.AssignLiteral("<!");
        theStr.Append(aToken->GetStringValue());
        if (!aToken->IsInError()) {
          theStr.AppendLiteral(">");
        }
        result=WriteTag(kCData,theStr,0,aToken->IsInError());
      }
      break;

    case eToken_markupDecl:
      {
        nsAutoString theStr;
        theStr.AssignLiteral("<!");
        theStr.Append(aToken->GetStringValue());
        if (!aToken->IsInError()) {
          theStr.AppendLiteral(">");
        }
        result=WriteTag(kMarkupDecl,theStr,0,aToken->IsInError());
      }
      break;

    case eToken_comment: 
      {
        nsAutoString theStr;
        aToken->AppendSourceTo(theStr);
        result=WriteTag(kComment,theStr,0,aToken->IsInError());
      }
      break;

    case eToken_doctypeDecl:
      {
        const nsSubstring& doctypeValue = aToken->GetStringValue();
        result=WriteTag(kDoctype,doctypeValue,0,aToken->IsInError());
      }
      break;

    case eToken_newline:
      {
        const nsSubstring& newlineValue = aToken->GetStringValue();
        result=WriteTag(kText,newlineValue,0,PR_FALSE);
        ++mTokenCount;
        if (NS_VIEWSOURCE_TOKENS_PER_BLOCK > 0 &&
            mTokenCount > NS_VIEWSOURCE_TOKENS_PER_BLOCK) {
          StartNewPreBlock();
        }
      }
      break;

    case eToken_whitespace:
      {
        const nsSubstring& wsValue = aToken->GetStringValue();
        result=WriteTag(kText,wsValue,0,PR_FALSE);
        ++mTokenCount;
        if (NS_VIEWSOURCE_TOKENS_PER_BLOCK > 0 &&
            mTokenCount > NS_VIEWSOURCE_TOKENS_PER_BLOCK &&
            !wsValue.IsEmpty()) {
          PRUnichar ch = wsValue.Last();
          if (ch == kLF || ch == kCR)
            StartNewPreBlock();
        }
      }
      break;

    case eToken_text:
      {
        const nsSubstring& str = aToken->GetStringValue();         
        result=WriteTag(kText,str,aToken->GetAttributeCount(),aToken->IsInError());
        ++mTokenCount;
        if (NS_VIEWSOURCE_TOKENS_PER_BLOCK > 0 &&
            mTokenCount > NS_VIEWSOURCE_TOKENS_PER_BLOCK && !str.IsEmpty()) {
          PRUnichar ch = str.Last();
          if (ch == kLF || ch == kCR)
            StartNewPreBlock();
        }
      }

      break;

    case eToken_entity:
      result=WriteTag(kEntity,aToken->GetStringValue(),0,aToken->IsInError());
      break;

    case eToken_instruction:
      result=WriteTag(kPI,aToken->GetStringValue(),0,aToken->IsInError());
      break;

    default:
      result=NS_OK;
  }//switch

  theContext.mTokenNode.ReleaseAll(); 

  return result;
}

