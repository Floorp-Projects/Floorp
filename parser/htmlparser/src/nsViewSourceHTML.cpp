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

#include "nsIAtom.h"
#include "nsViewSourceHTML.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsDTDUtils.h"
#include "nsIContentSink.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLTokenizer.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"
#include "nsNetUtil.h"
#include "nsHTMLEntities.h"
#include "nsParserConstants.h"

#include "nsIServiceManager.h"

#include "nsElementTable.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"
#include "prmem.h"

#include "mozilla/Preferences.h"

#ifdef RAPTOR_PERF_METRICS
#include "stopwatch.h"
Stopwatch vsTimer;
#endif

using namespace mozilla;

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
static const char kBodyTabSize[] = "-moz-tab-size: ";

NS_IMPL_ISUPPORTS1(CViewSourceHTML, nsIDTD)

/********************************************
 ********************************************/

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
  mSyntaxHighlight =
    Preferences::GetBool("view_source.syntax_highlight", true);

  mWrapLongLines = Preferences::GetBool("view_source.wrap_long_lines", false);

  mTabSize = Preferences::GetInt("view_source.tab_size", -1);

  mSink = 0;
  mLineNumber = 1;
  mTokenizer = 0;
  mDocType=eHTML_Quirks;
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
  mSink=0; //just to prove we destructed...
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel().
  * @update rickg 03.20.2000
  * @param  aParserContext
  * @param  aSink
  * @return error code (almost always 0)
  */
NS_IMETHODIMP
CViewSourceHTML::WillBuildModel(const CParserContext& aParserContext,
                                nsITokenizer* aTokenizer,
                                nsIContentSink* aSink)
{
  nsresult result=NS_OK;

#ifdef RAPTOR_PERF_METRICS
  vsTimer.Reset();
  NS_START_STOPWATCH(vsTimer);
#endif

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
      fprintf(gDumpFile, "<link rel=\"stylesheet\" type=\"text/css\" href=\"resource://gre-resources/viewsource.css\">\n");
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

  return result;
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel().
  * @update gess5/18/98
  * @param  aFilename is the name of the file being parsed.
  * @return error code (almost always 0)
  */
NS_IMETHODIMP CViewSourceHTML::BuildModel(nsITokenizer* aTokenizer,
                                          bool aCanInterrupt,
                                          bool aCountLines,
                                          const nsCString* aCharsetPtr)
{
  nsresult result=NS_OK;

  if(aTokenizer) {

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
          static_cast<CStartToken*>
                     (theAllocator->CreateTokenOfType(eToken_start,
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
                        NS_LITERAL_STRING("resource://gre-resources/viewsource.css"));

          mSink->AddLeaf(theNode);
        }
        IF_FREE(theToken, theAllocator);
      }

      result = mSink->CloseContainer(eHTMLTag_head);
      if(NS_SUCCEEDED(result)) {
        mHasOpenRoot = PR_TRUE;
      }
    }
    if (NS_SUCCEEDED(result) && !mHasOpenBody) {
      if (theAllocator) {
        CStartToken* bodyToken=
          static_cast<CStartToken*>
                     (theAllocator->CreateTokenOfType(eToken_start,
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
          if (mTabSize >= 0) {
            nsAutoString styleValue = NS_ConvertASCIItoUTF16(kBodyTabSize);
            styleValue.AppendInt(mTabSize);
            AddAttrToNode(bodyNode, theAllocator,
                          NS_LITERAL_STRING("style"),
                          styleValue);
          }
          result = mSink->OpenContainer(bodyNode);
          if(NS_SUCCEEDED(result)) mHasOpenBody=PR_TRUE;
        }
        IF_FREE(bodyToken, theAllocator);

        if (NS_SUCCEEDED(result)) {
          CStartToken* preToken =
            static_cast<CStartToken*>
                       (theAllocator->CreateTokenOfType(eToken_start,
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
          IF_FREE(preToken, theAllocator);
        }
      }
    }

    NS_ASSERTION(aCharsetPtr, "CViewSourceHTML::BuildModel expects a charset!");
    mCharset = *aCharsetPtr;

    NS_ASSERTION(aCanInterrupt, "CViewSourceHTML can't run scripts, so "
                 "document.write should not forbid interruptions. Why is "
                 "the parser telling us not to interrupt?");

    while(NS_SUCCEEDED(result)){
      CToken* theToken=mTokenizer->PopToken();
      if(theToken) {
        result=HandleToken(theToken);
        if(NS_SUCCEEDED(result)) {
          IF_FREE(theToken, mTokenizer->GetTokenAllocator());
          if (mSink->DidProcessAToken() == NS_ERROR_HTMLPARSER_INTERRUPTED) {
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
    static_cast<CStartToken*>
               (theAllocator->CreateTokenOfType(eToken_start,
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
  IF_FREE(theToken, theAllocator);

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

  // Parser nodes assume that they are being handed a ref when AddAttribute is
  // called, unlike Init() and construction, when they actually addref the
  // incoming token.  Do NOT release here unless this setup changes.
}

/**
 *
 * @update  gess5/18/98
 * @param
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::DidBuildModel(nsresult anErrorCode)
{
  nsresult result= NS_OK;

  //ADD CODE HERE TO CLOSE OPEN CONTAINERS...

  if (mSink) {
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
 * @update  harishd 07/22/99
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

NS_IMETHODIMP_(nsDTDMode)
CViewSourceHTML::GetMode() const
{
  // Quirks mode needn't affect how the source is viewed, so parse the source
  // view in full standards mode no matter what:
  return eDTDMode_full_standards;
}

/**
 * Called by the parser to enable/disable dtd verification of the
 * internal context stack.
 * @update  gess 7/23/98
 * @param
 * @return
 */
void CViewSourceHTML::SetVerification(bool aEnabled)
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
NS_IMETHODIMP_(bool)
CViewSourceHTML::CanContain(PRInt32 aParent, PRInt32 aChild) const
{
  bool result=true;
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
NS_IMETHODIMP_(bool)
CViewSourceHTML::IsContainer(PRInt32 aTag) const
{
  bool result=true;
  return result;
}

/**
 *  This method gets called when a tag needs to write it's attributes
 *
 *  @update  gess 3/25/98
 *  @param
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteAttributes(const nsAString& tagName, 
                                          nsTokenAllocator* allocator, 
                                          PRInt32 attrCount, bool aOwnerInError) {
  nsresult result=NS_OK;

  if(attrCount){ //go collect the attributes...
    int attr = 0;
    for(attr = 0; attr < attrCount; ++attr){
      CToken* theToken = mTokenizer->PeekToken();
      if(theToken)  {
        eHTMLTokenTypes theType = eHTMLTokenTypes(theToken->GetTokenType());
        if(eToken_attribute == theType){
          mTokenizer->PopToken(); //pop it for real...
          mTokenNode.AddAttribute(theToken);  //and add it to the node.

          CAttributeToken* theAttrToken = (CAttributeToken*)theToken;
          const nsSubstring& theKey = theAttrToken->GetKey();

          // The attribute is only in error if its owner is NOT in error.
          const bool attributeInError =
            !aOwnerInError && theAttrToken->IsInError();

          result = WriteTag(kAttributeName,theKey,0,attributeInError);
          const nsSubstring& theValue = theAttrToken->GetValue();

          if(!theValue.IsEmpty() || theAttrToken->mHasEqualWithoutValue){
            if (IsUrlAttribute(tagName, theKey, theValue)) {
              WriteHrefAttribute(allocator, theValue);
            } else {
              WriteTag(kAttributeValue,theValue,0,attributeInError);
            }
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
nsresult CViewSourceHTML::WriteTag(PRInt32 aTagType,const nsSubstring & aText,PRInt32 attrCount,bool aTagInError) {
  nsresult result=NS_OK;

  // adjust line number to what it will be after we finish writing this tag
  // XXXbz life here sucks.  We can't use the GetNewlineCount on the token,
  // because Text tokens in <style>, <script>, etc lie through their teeth.
  // On the other hand, the parser messes up newline counting in some token
  // types (bug 137315).  So our line numbers will disagree with the parser's
  // in some cases...
  // XXXbenjamn Shouldn't we be paying attention to the aCountLines BuildModel
  // parameter here?
  mLineNumber += aText.CountChar(PRUnichar('\n'));

  nsTokenAllocator* theAllocator=mTokenizer->GetTokenAllocator();
  NS_ASSERTION(0!=theAllocator,"Error: no allocator");
  if(0==theAllocator)
    return NS_ERROR_FAILURE;

  // Highlight all parts of all erroneous tags.
  if (mSyntaxHighlight && aTagInError) {
    CStartToken* theTagToken=
      static_cast<CStartToken*>
                 (theAllocator->CreateTokenOfType(eToken_start,
                                                     eHTMLTag_span,
                                                     NS_LITERAL_STRING("SPAN")));
    NS_ENSURE_TRUE(theTagToken, NS_ERROR_OUT_OF_MEMORY);
    mErrorNode.Init(theTagToken, theAllocator);
    AddAttrToNode(mErrorNode, theAllocator,
                  NS_LITERAL_STRING("class"),
                  NS_LITERAL_STRING("error"));
    mSink->OpenContainer(mErrorNode);
    IF_FREE(theTagToken, theAllocator);
#ifdef DUMP_TO_FILE
    if (gDumpFile) {
      fprintf(gDumpFile, "<span class=\"error\">");
    }
#endif
  }

  if (kBeforeText[aTagType][0] != 0) {
    NS_ConvertASCIItoUTF16 beforeText(kBeforeText[aTagType]);
    mITextToken.SetIndirectString(beforeText);
    nsCParserNode theNode(&mITextToken, 0/*stack token*/);
    mSink->AddLeaf(theNode);
  }
#ifdef DUMP_TO_FILE
  if (gDumpFile && kDumpFileBeforeText[aTagType][0])
    fprintf(gDumpFile, kDumpFileBeforeText[aTagType]);
#endif // DUMP_TO_FILE

  if (mSyntaxHighlight && aTagType != kText) {
    CStartToken* theTagToken=
      static_cast<CStartToken*>
                 (theAllocator->CreateTokenOfType(eToken_start,
                                                     eHTMLTag_span,
                                                     NS_LITERAL_STRING("SPAN")));
    NS_ENSURE_TRUE(theTagToken, NS_ERROR_OUT_OF_MEMORY);
    mStartNode.Init(theTagToken, theAllocator);
    AddAttrToNode(mStartNode, theAllocator,
                  NS_LITERAL_STRING("class"),
                  NS_ConvertASCIItoUTF16(kElementClasses[aTagType]));
    mSink->OpenContainer(mStartNode);  //emit <starttag>...
    IF_FREE(theTagToken, theAllocator);
#ifdef DUMP_TO_FILE
    if (gDumpFile) {
      fprintf(gDumpFile, "<span class=\"");
      fprintf(gDumpFile, kElementClasses[aTagType]);
      fprintf(gDumpFile, "\">");
    }
#endif // DUMP_TO_FILE
  }

  mITextToken.SetIndirectString(aText);  //now emit the tag name...

  nsCParserNode theNode(&mITextToken, 0/*stack token*/);
  mSink->AddLeaf(theNode);
#ifdef DUMP_TO_FILE
  if (gDumpFile) {
    fputs(NS_ConvertUTF16toUTF8(aText).get(), gDumpFile);
  }
#endif // DUMP_TO_FILE

  if (mSyntaxHighlight && aTagType != kText) {
    mStartNode.ReleaseAll();
    mSink->CloseContainer(eHTMLTag_span);  //emit </endtag>...
#ifdef DUMP_TO_FILE
    if (gDumpFile)
      fprintf(gDumpFile, "</span>");
#endif //DUMP_TO_FILE
  }

  if(attrCount){
    result=WriteAttributes(aText, theAllocator, attrCount, aTagInError);
  }

  // Tokens are set in error if their ending > is not there, so don't output
  // the after-text
  if (!aTagInError && kAfterText[aTagType][0] != 0) {
    NS_ConvertASCIItoUTF16 afterText(kAfterText[aTagType]);
    mITextToken.SetIndirectString(afterText);
    nsCParserNode theNode(&mITextToken, 0/*stack token*/);
    mSink->AddLeaf(theNode);
  }
#ifdef DUMP_TO_FILE
  if (!aTagInError && gDumpFile && kDumpFileAfterText[aTagType][0])
    fprintf(gDumpFile, kDumpFileAfterText[aTagType]);
#endif // DUMP_TO_FILE

  if (mSyntaxHighlight && aTagInError) {
    mErrorNode.ReleaseAll();
    mSink->CloseContainer(eHTMLTag_span);  //emit </endtag>...
#ifdef DUMP_TO_FILE
    if (gDumpFile)
      fprintf(gDumpFile, "</span>");
#endif //DUMP_TO_FILE
  }

  return result;
}

/**
 *
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
nsresult
CViewSourceHTML::HandleToken(CToken* aToken)
{
  nsresult        result=NS_OK;
  CHTMLToken*     theToken= (CHTMLToken*)(aToken);
  eHTMLTokenTypes theType= (eHTMLTokenTypes)theToken->GetTokenType();

  NS_ASSERTION(mSink, "No sink in CViewSourceHTML::HandleToken? Was WillBuildModel called?");

  mTokenNode.Init(theToken, mTokenizer->GetTokenAllocator());

  switch(theType) {

    case eToken_start:
      {
        const nsSubstring& startValue = aToken->GetStringValue();
        result = WriteTag(kStartTag,startValue,aToken->GetAttributeCount(),aToken->IsInError());

        if((ePlainText!=mDocType) && (NS_OK==result)) {
          result = mSink->NotifyTagObservers(&mTokenNode);
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

  mTokenNode.ReleaseAll();

  return result;
}

bool CViewSourceHTML::IsUrlAttribute(const nsAString& tagName,
                                       const nsAString& attrName, 
                                       const nsAString& attrValue) {
  const nsSubstring &trimmedAttrName = TrimTokenValue(attrName);

  bool isHref = trimmedAttrName.LowerCaseEqualsLiteral("href");
  bool isSrc = !isHref && trimmedAttrName.LowerCaseEqualsLiteral("src");
  bool isXLink = !isHref && !isSrc &&
    mDocType == eXML && trimmedAttrName.EqualsLiteral("xlink:href");

  // If this is the HREF attribute of a BASE element, then update the base URI.
  // This doesn't feel like the ideal place for this, but the alternatives don't
  // seem all that nice either.
  if (isHref && tagName.LowerCaseEqualsLiteral("base")) {
    const nsAString& baseSpec = TrimTokenValue(attrValue);
    nsAutoString expandedBaseSpec;
    ExpandEntities(baseSpec, expandedBaseSpec);
    SetBaseURI(expandedBaseSpec);
  }

  return isHref || isSrc || isXLink;
}

void CViewSourceHTML::WriteHrefAttribute(nsTokenAllocator* allocator,
                                         const nsAString& href) {
  // The "href" will typically contain not only the href proper, but the single
  // or double quotes and often some surrounding whitespace as well.  Find the
  // location of the href proper inside the string.
  nsAString::const_iterator startProper, endProper;
  href.BeginReading(startProper);
  href.EndReading(endProper);
  TrimTokenValue(startProper, endProper);

  // Break the href into three parts, the preceding text, the href proper, and
  // the succeeding text.
  nsAString::const_iterator start, end;
  href.BeginReading(start);
  href.EndReading(end);  
  const nsAString &precedingText = Substring(start, startProper);
  const nsAString &hrefProper = Substring(startProper, endProper);
  const nsAString &succeedingText = Substring(endProper, end);

  nsAutoString fullPrecedingText;
  fullPrecedingText.Assign(kEqual);
  fullPrecedingText.Append(precedingText);

  // Regular URLs and view-source URLs work the same way for .js and .css files.
  // However, if the user follows a link in the view source window to a .html
  // file (i.e. the HREF in an A tag), then presumably they will expect to see
  // the *source* of that new page, not the rendered version.  So for now we
  // just slap a "view-source:" at the beginning of each URL.  There are two
  // big downsides to doing it this way -- we must make relative URLs into
  // absolute URLs before we can turn them into view-source URLs, and links
  // to images don't work right -- nobody wants to see the bytes constituting a
  // PNG rendered as text.  A smarter view-source handler might be able to deal
  // with the latter problem.

  // Construct a "view-source" URL for the HREF.
  nsAutoString viewSourceUrl;
  CreateViewSourceURL(hrefProper, viewSourceUrl);

  // Construct the HTML that will represent the HREF.
  if (viewSourceUrl.IsEmpty()) {
    nsAutoString equalsHref(kEqual);
    equalsHref.Append(href);
    WriteTextInSpan(equalsHref, allocator, EmptyString(), EmptyString());
  } else {
    NS_NAMED_LITERAL_STRING(HREF, "href");
    if (fullPrecedingText.Length() > 0) {
      WriteTextInSpan(fullPrecedingText, allocator, EmptyString(), EmptyString());
    }
    WriteTextInAnchor(hrefProper, allocator, HREF, viewSourceUrl);
    if (succeedingText.Length() > 0) {
      WriteTextInSpan(succeedingText, allocator, EmptyString(), EmptyString());
    }
  }
}

nsresult CViewSourceHTML::CreateViewSourceURL(const nsAString& linkUrl,
                                              nsString& viewSourceUrl) {
  nsCOMPtr<nsIURI> baseURI;
  nsCOMPtr<nsIURI> hrefURI;
  nsresult rv;

  // Default the view source URL to the empty string in case we fail.
  viewSourceUrl.Truncate();

  // Get the BaseURI.
  rv = GetBaseURI(getter_AddRefs(baseURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // Use the link URL and the base URI to build a URI for the link.  Note that
  // the link URL may have untranslated entities in it.
  nsAutoString expandedLinkUrl;
  ExpandEntities(linkUrl, expandedLinkUrl);
  rv = NS_NewURI(getter_AddRefs(hrefURI), expandedLinkUrl, mCharset.get(), baseURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the absolute URL from the link URI.
  nsCString absoluteLinkUrl;
  hrefURI->GetSpec(absoluteLinkUrl);

  // URLs that execute script (e.g. "javascript:" URLs) should just be
  // ignored.  There's nothing reasonable we can do with them, and allowing
  // them to execute in the context of the view-source window presents a
  // security risk.  Just return the empty string in this case.
  bool openingExecutesScript = false;
  rv = NS_URIChainHasFlags(hrefURI, nsIProtocolHandler::URI_OPENING_EXECUTES_SCRIPT,
                           &openingExecutesScript);
  NS_ENSURE_SUCCESS(rv, NS_OK); // if there's an error, return the empty string
  if (openingExecutesScript) {
    return NS_OK;
  }

  // URLs that return data (e.g. "http:" URLs) should be prefixed with
  // "view-source:".  URLs that don't return data should just be returned
  // undecorated.
  bool doesNotReturnData = false;
  rv = NS_URIChainHasFlags(hrefURI, nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA,
                           &doesNotReturnData);
  NS_ENSURE_SUCCESS(rv, NS_OK);  // if there's an error, return the empty string
  if (!doesNotReturnData) {
    viewSourceUrl.AssignLiteral("view-source:");    
  }

  AppendUTF8toUTF16(absoluteLinkUrl, viewSourceUrl);

  return NS_OK;
}

void CViewSourceHTML::WriteTextInSpan(const nsAString& text, 
                                      nsTokenAllocator* allocator, 
                                      const nsAString& attrName, 
                                      const nsAString& attrValue) {
  NS_NAMED_LITERAL_STRING(SPAN, "SPAN");
  WriteTextInElement(SPAN, eHTMLTag_span, text, allocator, attrName, attrValue);
}

void CViewSourceHTML::WriteTextInAnchor(const nsAString& text, 
                                        nsTokenAllocator* allocator, 
                                        const nsAString& attrName, 
                                        const nsAString& attrValue) {
  NS_NAMED_LITERAL_STRING(ANCHOR, "A");
  WriteTextInElement(ANCHOR, eHTMLTag_a, text, allocator, attrName, attrValue);
}

void CViewSourceHTML::WriteTextInElement(const nsAString& tagName, 
                                         eHTMLTags tagType, const nsAString& text,
                                         nsTokenAllocator* allocator,
                                         const nsAString& attrName, 
                                         const nsAString& attrValue) {
  // Open the element, supplying the attribute, if any.
  nsTokenAllocator* theAllocator = mTokenizer->GetTokenAllocator();
  if (!theAllocator) {
    return;
  }

  CStartToken* startToken =
    static_cast<CStartToken*>
      (theAllocator->CreateTokenOfType(eToken_start, tagType, tagName));
  if (!startToken) {
    return;
  }

  nsCParserStartNode startNode(startToken, theAllocator);
  if (!attrName.IsEmpty()) {
    AddAttrToNode(startNode, allocator, attrName, attrValue);
  }
  mSink->OpenContainer(startNode);
  IF_FREE(startToken, theAllocator);

  // Add the text node.
  CTextToken textToken(text);
  nsCParserNode textNode(&textToken, 0/*stack token*/);
  mSink->AddLeaf(textNode);

  // Close the element.
  mSink->CloseContainer(tagType);
}

const nsDependentSubstring CViewSourceHTML::TrimTokenValue(const nsAString& tokenValue) {
  nsAString::const_iterator start, end;
  tokenValue.BeginReading(start);
  tokenValue.EndReading(end);
  TrimTokenValue(start, end);
  return Substring(start, end);
}

void CViewSourceHTML::TrimTokenValue(nsAString::const_iterator& start,
                                     nsAString::const_iterator& end) {
  // Token values -- tag names, attribute names, and attribute values --
  // generally contain adjacent whitespace and, in the case of attribute values,
  // the surrounding double or single quotes.  Return a new string with this
  // adjacent text stripped off, so only the value proper remains.
        
  // Skip past any whitespace or quotes on the left.
  while (start != end) {
    if (!IsTokenValueTrimmableCharacter(*start)) break;
    ++start;
  }

  // Skip past any whitespace or quotes on the right.  Note that the interval 
  // start..end is half-open.  That means the last character of the interval is
  // at *(end - 1).
  while (end != start) {      
    --end;
    if (!IsTokenValueTrimmableCharacter(*end)) {
      ++end; // we've actually gone one too far at this point, so adjust.
      break;
    }
  }
}

bool CViewSourceHTML::IsTokenValueTrimmableCharacter(PRUnichar ch) {
  if (ch == ' ') return PR_TRUE;
  if (ch == '\t') return PR_TRUE;
  if (ch == '\r') return PR_TRUE;
  if (ch == '\n') return PR_TRUE;
  if (ch == '\'') return PR_TRUE;
  if (ch == '"') return PR_TRUE;
  return PR_FALSE;
}

nsresult CViewSourceHTML::GetBaseURI(nsIURI **result) {
  nsresult rv = NS_OK;
  if (!mBaseURI) {
    rv = SetBaseURI(mFilename);
  }
  NS_IF_ADDREF(*result = mBaseURI);
  return rv;
}

nsresult CViewSourceHTML::SetBaseURI(const nsAString& baseSpec) {
  // Create a new base URI and store it in mBaseURI.
  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI), baseSpec, mCharset.get());
  NS_ENSURE_SUCCESS(rv, rv);
  mBaseURI = baseURI;
  return NS_OK;
}

void CViewSourceHTML::ExpandEntities(const nsAString& textIn, nsString& textOut)
{  
  nsAString::const_iterator iter, end;
  textIn.BeginReading(iter);
  textIn.EndReading(end);

  // The outer loop treats the input as a sequence of pairs of runs.  The first
  // run of each pair is just a run of regular characters.  The second run is
  // something that looks like it might be an entity reference, e.g. "&amp;".
  // Any regular run may be empty, and the entity run can be skipped at the end
  // of the text.  Apparent entities that can't be translated are copied
  // verbatim.  In particular this allows for raw ampersands in the input.
  // Special care is taken to handle the end of the text at any point inside
  // the loop.
  while (iter != end) {
    // Copy characters to textOut until but not including the first ampersand.
    for (; iter != end; ++iter) {
      PRUnichar ch = *iter;
      if (ch == kAmpersand) {
        break;
      }
      textOut.Append(ch);
    }

    // We have a possible entity.  If the entity is well-formed (or well-enough
    // formed) copy the entity value to "textOut".  Otherwise, copy the "entity"
    // source characters to "textOut" verbatim.  Either way, advance "iter" to
    // the first position after the entity/entity-like-thing.
    CopyPossibleEntity(iter, end, textOut);
  }
}

static bool InRange(PRUnichar ch, unsigned char chLow, unsigned char chHigh)
{
  return (chLow <= ch) && (ch <= chHigh);
}

static bool IsDigit(PRUnichar ch)
{ 
  return InRange(ch, '0', '9');
}

static bool IsHexDigit(PRUnichar ch)
{
  return IsDigit(ch) || InRange(ch, 'A', 'F') || InRange(ch, 'a', 'f');
}

static bool IsAlphaNum(PRUnichar ch)
{
  return InRange(ch, 'A', 'Z') || InRange(ch, 'a', 'z') || IsDigit(ch);
}

static bool IsAmpersand(PRUnichar ch)
{
  return ch == kAmpersand;
}

static bool IsHashsign(PRUnichar ch)
{
  return ch == kHashsign;
}

static bool IsXx(PRUnichar ch)
{
  return (ch == 'X') || (ch == 'x');
}

static bool IsSemicolon(PRUnichar ch)
{
  return ch == kSemicolon;
}

static bool ConsumeChar(nsAString::const_iterator& start,
                          const nsAString::const_iterator &end,
                          bool (*testFun)(PRUnichar ch))
{
  if (start == end) {
    return PR_FALSE;
  }
  if (!testFun(*start)) {
    return PR_FALSE;
  }
  ++start;
  return PR_TRUE;
}

void CViewSourceHTML::CopyPossibleEntity(nsAString::const_iterator& iter,
                                         const nsAString::const_iterator& end,
                                         nsAString& textBuffer)
{
  // Note that "iter" is passed by reference, and we need to make sure that
  // we update its position as we parse characters, so the caller will know
  // how much text we processed.

  // Remember where we started.
  const nsAString::const_iterator start(iter);
  
  // Our possible entity must at least start with an '&' -- bail if it doesn't.
  if (!ConsumeChar(iter, end, IsAmpersand)) {
    return;
  }

  // Identify the entity "body" and classify it.
  nsAString::const_iterator startBody, endBody;
  enum {TYPE_ID, TYPE_DECIMAL, TYPE_HEXADECIMAL} entityType;
  if (ConsumeChar(iter, end, IsHashsign)) {
    if (ConsumeChar(iter, end, IsXx)) {
      startBody = iter;
      entityType = TYPE_HEXADECIMAL;
      while (ConsumeChar(iter, end, IsHexDigit)) {
        // empty
      }
    } else {
      startBody = iter;
      entityType = TYPE_DECIMAL;
      while (ConsumeChar(iter, end, IsDigit)) {
        // empty
      }
    }
  } else {
    startBody = iter;
    entityType = TYPE_ID;
    // The parser seems to allow some other characters, such as ":" and "_".
    // However, all of the entities that we know about (see nsHTMLEntityList.h)
    // are strictly alphanumeric.
    while (ConsumeChar(iter, end, IsAlphaNum)) {
      // empty
    }
  }

  // Record the end of the entity body.
  endBody = iter;
  
  // If the entity body is terminated with a semicolon, consume that too.
  bool properlyTerminated = ConsumeChar(iter, end, IsSemicolon);

  // If the entity body is empty, then it's not really an entity.  Copy what
  // we've parsed verbatim, and return immediately.
  if (startBody == endBody) {
    textBuffer.Append(Substring(start, iter));
    return;
  }

  // Construct a string from the body range.  Note that we need a regular
  // string since substrings don't provide ToInteger().
  nsAutoString entityBody(Substring(startBody, endBody));

  // Decode the entity to a Unicode character.
  PRInt32 entityCode = -1;
  switch (entityType) {
  case TYPE_ID:
    entityCode = nsHTMLEntities::EntityToUnicode(entityBody);
    break;
  case TYPE_DECIMAL:
    entityCode = ToUnicode(entityBody, 10, -1);
    break;
  case TYPE_HEXADECIMAL:
    entityCode = ToUnicode(entityBody, 16, -1);
    break;
  default:
    NS_NOTREACHED("Unknown entity type!");
    break;
  }

  // Note that the parser does not require terminating semicolons for entities
  // with eight bit values.  We want to allow the same here.
  if (properlyTerminated || ((0 <= entityCode) && (entityCode < 256))) {
    textBuffer.Append((PRUnichar) entityCode);
  } else {
    // If the entity is malformed in any way, just copy the source text verbatim.
    textBuffer.Append(Substring(start, iter));
  }
}

PRInt32 CViewSourceHTML::ToUnicode(const nsString &strNum, PRInt32 radix, PRInt32 fallback)
{
  PRInt32 result;
  PRInt32 code = strNum.ToInteger(&result, radix);
  if (result == NS_OK) {
    return code;
  }
  return fallback;
}

