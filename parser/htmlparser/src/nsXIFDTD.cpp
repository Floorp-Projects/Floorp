/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/**
 * MODULE NOTES:
 * @update  gpk 06/18/98
 * 
 *         
 */

#include "nsXIFDTD.h" 
#include "nsHTMLTokens.h"
#include "nsCRT.h"
#include "nsIParser.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsTokenHandler.h"
#include "nsIDTDDebug.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLContentSinkStream.h"
#include "prmem.h"
#include "nsXMLTokenizer.h"
#include "nsDTDUtils.h"

static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_XIF_DTD_CID); 

static const char* kNullToken = "Error: Null token given";
static const char* kXIFDocHeader= "<!DOCTYPE xif>";
static const char* kXIFDocInfo= "document_info";
static const char* kXIFCharset= "charset";
  

struct nsXIFTagEntry {
  char      mName[32];
  eXIFTags  fTagID;
};

  // KEEP THIS LIST SORTED!
  // NOTE: This table is sorted in ascii collating order. If you
  // add a new entry, make sure you put it in the right spot otherwise
  // the binary search code above will break! 
nsXIFTagEntry gXIFTagTable[] =
{
  {"!!UNKNOWN",             eXIFTag_unknown},
  {"!DOCTYPE",              eXIFTag_doctype},
  {"?XML",                  eXIFTag_xml},

  {"attr",                  eXIFTag_attr},
  {"comment",               eXIFTag_comment},
  {"container",             eXIFTag_container},
  {"content",               eXIFTag_content},

  {"css_declaration",       eXIFTag_css_declaration},
  {"css_declaration_list",  eXIFTag_css_declaration_list},
  {"css_rule",              eXIFTag_css_rule},
  {"css_selector",          eXIFTag_css_selector},
  {"css_stylelist",         eXIFTag_css_stylelist},
  {"css_stylerule",         eXIFTag_css_stylerule},
  {"css_stylesheet",        eXIFTag_css_stylesheet},

  {"document_info",         eXIFTag_document_info},

  {"encode",                eXIFTag_encode},
  {"entity",                eXIFTag_entity},

  {"import",                eXIFTag_import},

  {"leaf",                  eXIFTag_leaf},
  {"link",                  eXIFTag_link},

  {"markup_declaration",    eXIFTag_markupDecl},

  {"section",               eXIFTag_section},
  {"section_body",          eXIFTag_section_body}, 
  {"section_head",          eXIFTag_section_head}, 
  {"stylelist",             eXIFTag_stylelist},

  {"url",                   eXIFTag_url},


};

struct nsXIFAttrEntry
{
  char            mName[11];
  eXIFAttributes  fAttrID;
};

nsXIFAttrEntry gXIFAttributeTable[] =
{
  {"key",     eXIFAttr_key},
  {"tag",     eXIFAttr_tag},
  {"value",   eXIFAttr_value},  
};



/*
 *  This method iterates the tagtable to ensure that is 
 *  is proper sort order. This method only needs to be
 *  called once.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
class nsXIFTagTableVerifier {
public:
  nsXIFTagTableVerifier()
  {
    PRInt32  count=sizeof(gXIFTagTable)/sizeof(nsXIFTagEntry);
    PRInt32 i,j;
    for(i=1;i<count-1;i++)
    {
      j=strcmp(gXIFTagTable[i-1].mName,gXIFTagTable[i].mName);
      if(j>0) {
        cout << "XIF Tag Element Table is out of order at " << i << "!" << endl;
        return;
      }
    }

    count = sizeof(gXIFAttributeTable)/sizeof(nsXIFAttrEntry);
    for(i=1;i<count-1;i++)
    {
      j=strcmp(gXIFAttributeTable[i-1].mName,gXIFAttributeTable[i].mName);
      if(j>0) {
        cout << "XIF Tag Attribute Table is out of order at " << i << "!" << endl;
        return;
      }
    }
    return;
  }
};


/*
 *  This method accepts a string (and optionally, its length)
 *  and determines the eXIFTag (id) value.
 *  
 *  @update  gess 3/25/98
 *  @param   aString -- string to be convered to id
 *  @return  valid id, or user_defined.
 */
static
eXIFTags DetermineXIFTagType(const nsString& aString)
{
  PRInt32  result=-1;
  PRInt32  cnt=sizeof(gXIFTagTable)/sizeof(nsXIFTagEntry);
  PRInt32  low=0; 
  PRInt32  high=cnt-1;
  PRInt32  middle=kNotFound;
  
  while(low<=high){
    middle=(PRInt32)(low+high)/2;
    result=aString.CompareWithConversion(gXIFTagTable[middle].mName, PR_TRUE); 
    if (result==0)
      return gXIFTagTable[middle].fTagID; 
    if (result<0)
      high=middle-1; 
    else low=middle+1; 
  }
  return eXIFTag_userdefined;
}




/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gpk 06/18/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult nsXIFDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kIDTDIID)) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsXIFDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}


NS_IMPL_ADDREF(nsXIFDTD)
NS_IMPL_RELEASE(nsXIFDTD)



/**
 *  Default constructor
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
nsXIFDTD::nsXIFDTD() : nsIDTD(){
  NS_INIT_REFCNT();
  mParser=0;
  mTokenizer=0;
  mDTDDebug=nsnull;
  mInContent=PR_FALSE;
  mLowerCaseAttributes=PR_TRUE;
  mCharset.SetLength(0);
  mSink=0;
  mDTDState=NS_OK;

  mXIFContext=new nsDTDContext();
  mNodeRecycler=0;
  
  mContainerKey.AssignWithConversion("isa");
  mEncodeKey.AssignWithConversion("selection");
  mCSSStyleSheetKey.AssignWithConversion("max_css_selector_width");
  mCSSSelectorKey.AssignWithConversion("selectors");
  mCSSDeclarationKey.AssignWithConversion("property");
  mGenericKey.AssignWithConversion("value");

  mLineNumber=1;

}

/**
 *  Default destructor
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
nsXIFDTD::~nsXIFDTD(){
  NS_IF_RELEASE(mSink);
  NS_IF_RELEASE(mTokenizer);

  delete mXIFContext;
}


/**
 * 
 * @update	gess1/8/99
 * @param 
 * @return
 */
const nsIID& nsXIFDTD::GetMostDerivedIID(void) const{
  return kClassIID;
}


/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update	gess7/23/98
 * @param 
 * @return
 */
nsresult nsXIFDTD::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_NewXIFDTD(aInstancePtrResult);
}


/**
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type. 
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @update	gess 02/16/99
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
eAutoDetectResult nsXIFDTD::CanParse(CParserContext& aParserContext,nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;

  if(aParserContext.mMimeType.EqualsWithConversion(kXIFTextContentType)){
    result=ePrimaryDetect;
  }
  else 
  {
    if(kNotFound!=aBuffer.Find(kXIFDocHeader)) {
      aParserContext.SetMimeType( NS_ConvertToString(kXIFTextContentType) );
      result=ePrimaryDetect;
    }
  }
  
  nsString charset; charset.AssignWithConversion("ISO-8859-1");
  PRInt32 offset;
  offset = aBuffer.Find(kXIFDocInfo);

  if(kNotFound!=offset) {

    offset = aBuffer.Find(kXIFCharset);
    if (kNotFound!=offset) {
    
        //begin by finding the start and end quotes in the string...
      PRInt32 start = aBuffer.FindChar('"',PR_FALSE,offset);
      PRInt32 end = aBuffer.FindChar('"',PR_FALSE,start+1);

      if ((start != kNotFound) && (end != kNotFound)) {
#if 0
          //This is faster than using character iteration (used below)...
          //(Why call 1 function when 30 single-character appends will do? :)
        aBuffer.Mid(charset,start+1,(end-start)-1);

#else
          //I removed this old (SLOW) version in favor of calling the Mid() function 
        charset.Truncate();
        for (PRInt32 i = start+1; i < end; i++) {
          PRUnichar ch = aBuffer[i];
          charset.Append(ch);
        }
#endif
      }
    }
  }
  mCharset = charset;

  return result;
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
nsresult nsXIFDTD::WillBuildModel(  const CParserContext& aParserContext,nsIContentSink* aSink){

  nsresult result=NS_OK;

  if(aSink) {

    if(aSink && (!mSink)) {
      result=aSink->QueryInterface(kIHTMLContentSinkIID, (void **)&mSink);
    }
    result = aSink->WillBuildModel();
  }
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
nsresult nsXIFDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  NS_PRECONDITION(mXIFContext!=nsnull,"Create a context before calling build model");

  nsresult result=NS_OK;

  if(aTokenizer) {
    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    mParser=(nsParser*)aParser;

    if(mTokenizer) {

      mTokenAllocator=mTokenizer->GetTokenAllocator();
      result=mXIFContext->GetNodeRecycler(mNodeRecycler);

      if(NS_FAILED(result)) return result;

      if(mSink) {
        while(NS_SUCCEEDED(result)){
          if(mDTDState!=NS_ERROR_HTMLPARSER_STOPPARSING) {
            CToken* theToken=mTokenizer->PopToken();
            if(theToken) { 
              result=HandleToken(theToken,aParser);
            }
            else break;
          }
          else {
            result=mDTDState;
            break;
          }
        }//while
        mTokenizer=oldTokenizer;
      }
    }
  }
  else result=NS_ERROR_HTMLPARSER_BADTOKENIZER;
  return result;
}

/**
 * 
 * @update	gess 7/24/98
 * @param 
 * @return
 */
nsresult nsXIFDTD::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result=NS_OK;

  if(aParser){
    mSink=(nsIHTMLContentSink*)aParser->GetContentSink();
    if(mSink) {
      mSink->DidBuildModel(anErrorCode);
    }
  }

  return result;
}
/**
 *  Do preprocessing on the token
 *  
 *  @update  harishd 04/06/00
 *  @param   aType  - Set the appropriate type.
 *  @param   aToken - The type of the token..
 *  @return  
 */

nsresult nsXIFDTD::WillHandleToken(CToken* aToken,PRInt32& aType) {
  NS_ASSERTION(aToken!=nsnull,"invalid token");

  if(aToken) {
    aType=eHTMLTokenTypes(aToken->GetTokenType());
    eXIFTags theNewType = eXIFTag_userdefined;

    if((eToken_start==aType) || (eToken_end==aType)) {
      nsString& name = aToken->GetStringValueXXX();
      theNewType=DetermineXIFTagType(name);
      if (theNewType != eXIFTag_userdefined) {
        aToken->SetTypeID(theNewType);
      }
    }
  }
  return NS_OK;
}

/**
 *  This big dispatch method is used to route token handler calls to the right place.
 *  What's wrong with it? This table, and the dispatch methods themselves need to be 
 *  moved over to the delegate. Ah, so much to do...
 *  
 *  @update  harishd 04/06/00
 *  @param   aType
 *  @param   aToken
 *  @param   aParser
 *  @return  
 */
nsresult nsXIFDTD::HandleToken(CToken* aToken,nsIParser* aParser){
  nsresult  result=NS_OK;

  if(aToken) {

    PRInt32 theType=eToken_unknown;
    result=WillHandleToken(aToken,theType);
    
    if(result==NS_OK) {

      switch(theType) {
        case eToken_text:
        case eToken_start:
        case eToken_newline:
        case eToken_whitespace:
          result=HandleStartToken(aToken); break;
        case eToken_end:
          result=HandleEndToken(aToken); break;
        default:
          result=NS_OK;
      }//switch
      result=DidHandleToken(aToken);
    }
  }//if
  return result;
}

/**
 *  Do postprocessing on the token. 
 *  
 *  @update  harishd 04/06/00
 *  @param   aToken  - 
 *  @param   aResult -
 *  @return  
 */

nsresult nsXIFDTD::DidHandleToken(CToken* aToken, nsresult aResult) {
  NS_ASSERTION(mTokenAllocator!=nsnull,"We need a allocator");
  nsresult result=aResult;
  if(NS_SUCCEEDED(result) || (NS_ERROR_HTMLPARSER_BLOCK==result)) {
     IF_FREE(aToken);
  }
  else if(result==NS_ERROR_HTMLPARSER_STOPPARSING)
    mDTDState=result;
  else result=NS_OK;

  return result;
}


/**
 *  Call this method to open the top most node in the stack
 *  It also prevents the node from opening up twice
 *
 *  harishd 04/10/00
 *  @param - nil
 */
nsresult nsXIFDTD::PreprocessStack(void) {
  nsresult        result=NS_OK;
  nsCParserNode*  theNode=(nsCParserNode*)mXIFContext->PeekNode();
  
  if(theNode){
    // mUseCount = 0 ---> Node is not on the internal stack
    // mUseCount = 1 ---> Node is on the internal stack and can be passed to the sink.
    // mUseCount = 2 ---> Node passed to the sink already so don't open once again.
    if(theNode->mUseCount==1) {
      eHTMLTags theTag=(theNode->mToken)? (eHTMLTags)theNode->mToken->GetTypeID():eHTMLTag_unknown;
      if(theTag!=eHTMLTag_unknown)
      {
        if(IsHTMLContainer(theTag)) {
          result=mSink->OpenContainer(*theNode);
        }
        else {
          result=mSink->AddLeaf(*theNode);
        }
      }
      theNode->mUseCount++; // This will make sure that the node is not opened twice.
    }
  }


  return result;
}

/**
 *  Closes the top most contaier node on the internal stack..
 *
 *  @update  harishd 04/12/00
 *  @param   aToken - 
 *  @param   aTag   - The tag that might require preprocessing.
 *  @param   aNode  - The node to be handled
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult nsXIFDTD::WillHandleStartToken(CToken* aToken, eXIFTags aTag, nsIParserNode& aNode) {
  nsresult result=NS_OK;

  switch(aTag) {
    case eXIFTag_leaf:
    case eXIFTag_comment:
    case eXIFTag_entity:
    case eXIFTag_container:
      result=PreprocessStack(); // Pass the topmost node to the sink ( if it hasn't been yet ).
      break;
    default:
      break; 
  }

  return result;
}

/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. 
 *  
 *  @update  harishd 04/12/00
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult nsXIFDTD::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStartToken*  st    = (CStartToken*)(aToken);
  eXIFTags      type  =(eXIFTags)st->GetTypeID();

  //Begin by gathering up attributes...
  nsCParserNode* node=mNodeRecycler->CreateNode();
  node->Init(aToken,mLineNumber,mTokenAllocator);
  
  PRInt16         attrCount=aToken->GetAttributeCount();
  nsresult        result=(0==attrCount) ? NS_OK : CollectAttributes(*node,attrCount);

  if (NS_SUCCEEDED(result))
  {
    result=WillHandleStartToken(aToken,type,*node);
    if(NS_SUCCEEDED(result)) {
      switch (type)
      {
        case eXIFTag_attr:
          result=HandleAttributeToken(aToken,*node);
          break;
        case eXIFTag_comment:
          result=HandleCommentToken(aToken,*node);
          break;
        case eXIFTag_content:
          mInContent=PR_TRUE;
          break;
        case eXIFTag_encode:
          ProcessEncodeTag(*node);
          break;
        case eXIFTag_entity:
          ProcessEntityTag(*node);
          break;
        case eXIFTag_markupDecl:
          mXIFContext->Push(node);
          break;
        case eXIFTag_document_info:
          // This is XIF only tag. For HTML it's userdefined.
          node->mToken->SetTypeID(eHTMLTag_userdefined);
          result=mSink->OpenContainer(*node);
          break;
        default:
          result=HandleDefaultToken(aToken,*node);
          break;
      }
    }
  }

  mNodeRecycler->RecycleNode(node);
  return result;
}

/**
 *  This method determines if the token should be processed as
 *  a contianer or as a leaf.  Also determines if the token can
 *  can be handled or not.
 *
 *  @update  harishd 04/12/00
 *  @param   aToken - The start token.
 *  @param   aNode  - The container node
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult nsXIFDTD::HandleDefaultToken(CToken* aToken,nsIParserNode& aNode) {
  nsresult result=NS_OK;

  PRBool isConainer;
  eXIFTags theTag=(eXIFTags)aToken->GetTypeID();
      
  // First find out if the tag can be handled or not....
  // because there are a few tags that do not belong to HTML
  if(CanHandleDefaultTag(theTag,isConainer)) {
    if(isConainer) {
      result=HandleContainer(aNode);
    }
    else {
      result=AddLeaf(aNode);
    }
  }
  return result;
}

/**
 *  This method gets called when container needs to be processed.
 *  It collects the actual name ( html equivalent ), which resides as an
 *  attribute on the node, of the container and reinitializes the node 
 *  with the new name
 *  
 *  @update  harishd 04/12/00
 *  @param   aNode -- The container node
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult nsXIFDTD::HandleContainer(nsIParserNode& aNode) {

  nsCParserNode* theNode=(nsCParserNode*)&aNode;
  
  NS_ASSERTION(theNode!=nsnull,"need a node to process");

  nsresult result=NS_OK;

  if(theNode) {
    nsAutoString   theTagName;
    eHTMLTags      theTagID;

    if(theNode->mToken) {
      // The attribute key is "isa" ( mContainerKey )
      // The attribute value is the TAG name.
      GetAttribute(aNode,mContainerKey,theTagName);

      theTagID=nsHTMLTags::LookupTag(theTagName);

      theNode->mToken->Reinitialize(theTagID,theTagName);
      theNode->Init(theNode->mToken,0,mTokenAllocator);
    }
    mXIFContext->Push(&aNode);
  }
  return result;
}

/**
 *  This method does a couple of jobs. 
 *   a) Determines if the tag could be handled or not
 *   b) Determins  if the tag is a container
 *  
 *  @update  harishd 04/12/00
 *  @param   aTag        - The tag in question
 *  @param   aIsConainer - 0 if non-container, 1 if conainer, -1 if unknown
 *  @return  - TRUE if the tag can be handled
 */

PRBool nsXIFDTD::CanHandleDefaultTag(eXIFTags aTag,PRInt32& aIsContainer) {
  PRBool result=PR_TRUE;

  switch(aTag) {
    case eXIFTag_newline:
    case eXIFTag_whitespace:
    case eXIFTag_text:
      aIsContainer=0; // it's a leaf
      break;
    case eXIFTag_leaf:
    case eXIFTag_container:
      aIsContainer=1; // it's a conainer
      break;
    default:
      aIsContainer=-1; // unknown state
      result=PR_FALSE; // don't know how to handle this tag...
      break;
  }
  return result;
}

/**
 *  This method gets called when an end token has been 
 *  encountered in the parse process. If the end tag matches
 *  the start tag on the stack, then simply close it.
 *  
 *  @update  harishd 04/12/00
 *  @param   aToken -- next (end) token to be handled
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */
nsresult nsXIFDTD::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult       result   =NS_OK;
  eXIFTags       theTag   =(eXIFTags)aToken->GetTypeID();
  nsCParserNode* theNode  =(nsCParserNode*)mXIFContext->PeekNode();
  
  if(NS_SUCCEEDED(result)) {
    switch (theTag)
    {
      case eXIFTag_leaf:
      case eXIFTag_container:
      case eXIFTag_markupDecl:
        result=CloseContainer(*theNode);
        break;
      case eXIFTag_content:
        mInContent=PR_FALSE;
        break;
      default:
        break;
    }
  }
  return result;
}

/**
 * Consumes contents of a comment in one gulp. 
 *
 * @update	harishd 10/05/99
 * @param   aNode  - node to consume comment
 * @param   aToken - a comment token
 * @return  Error condition.
 */

nsresult nsXIFDTD::HandleCommentToken(CToken* aToken, nsIParserNode& aNode) {
  NS_PRECONDITION(aToken!=nsnull,"empty token");

  nsresult          result=NS_OK;
  CToken*           token=nsnull;
  eHTMLTokenTypes   type=(eHTMLTokenTypes)aToken->GetTokenType();

  if(type==eToken_start) {
    nsTokenAllocator* allocator=(mTokenizer)? mTokenizer->GetTokenAllocator():nsnull;
    if(allocator) {
      nsAutoString fragment;
      PRBool       done=PR_FALSE;
      PRBool       inContent=PR_FALSE;
      nsString&    comment=aToken->GetStringValueXXX(); 
      comment.AssignWithConversion("<!--"); // overwrite comment with "<!--"
      while (!done && NS_SUCCEEDED(result))
      {
        token=mTokenizer->PopToken();
      
        if(!token) return result;

        type=(eHTMLTokenTypes)token->GetTokenType();
        fragment.Assign(token->GetStringValueXXX());
        if(fragment.EqualsWithConversion("content")) {
          if(type==eToken_start) 
            inContent=PR_TRUE;
          else inContent=PR_FALSE;
        }
        else if(fragment.EqualsWithConversion("comment")) {
          comment.AppendWithConversion("-->");
          result=(mSink)? mSink->AddComment(aNode):NS_OK;
          done=PR_TRUE;
        }
        else {
          if(inContent) comment.Append(fragment);
        }
        IF_FREE(token);
      }
    }
  }
  return result;
}

/**
 *  This method gets called when an attribute token has been 
 *  encountered in the parse process. 
 *  
 *  @update  harishd 04/10/00
 *  @param   aToken -- the attribute token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult nsXIFDTD::HandleAttributeToken(CToken* aToken,nsIParserNode& aNode) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult result=NS_OK;
  nsCParserNode*  thePeekNode=(nsCParserNode*)mXIFContext->PeekNode();

  if(thePeekNode) {
    nsAutoString theKey;
    nsAutoString theValue;

    PRBool hasValue=GetAttributePair(aNode,theKey,theValue);

    if(hasValue) {
      CToken* attribute = mTokenAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown,theValue);
      nsString& key=((CAttributeToken*)attribute)->GetKey();
      key=theKey; // set the new key on the attribute..
      thePeekNode->AddAttribute(attribute);
    }
  }
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
PRBool nsXIFDTD::IsContainer(PRInt32 aTag) const{
  return PR_TRUE;
}
 

/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gpk 06/18/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool nsXIFDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const {

  PRBool result=PR_FALSE;
  
  // Revisit -- for now, anybody can contain anything
  result = PR_TRUE;  
  
  return result;
}

/**
 * Give rest of world access to our tag enums, so that CanContain(), etc,
 * become useful.
 */
NS_IMETHODIMP nsXIFDTD::StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXIFDTD::IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXIFDTD::ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


PRBool nsXIFDTD::IsInlineElement(PRInt32 aTagID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;
  return result;
}

PRBool nsXIFDTD::IsBlockElement(PRInt32 aTagID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;
  return result;
}


/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  rickg 06June2000
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */

PRBool nsXIFDTD::IsHTMLContainer(eHTMLTags aTag) const {
  PRBool result=PR_TRUE; // by default everything is a container

  switch(aTag) {
    case eHTMLTag_area:
    case eHTMLTag_base:
    case eHTMLTag_basefont:
    case eHTMLTag_br:
    case eHTMLTag_col:
    case eHTMLTag_frame:
    case eHTMLTag_hr:
    case eHTMLTag_img:
    case eHTMLTag_image:
    case eHTMLTag_input:
    case eHTMLTag_isindex:
    case eHTMLTag_link:
    case eHTMLTag_meta:
    case eHTMLTag_param:
    case eHTMLTag_sound:
      result=PR_FALSE;
      break;
    default:
      break;
  }
  return result;
}


/**
  * Begin Support for converting from XIF to HTML
  *
  */

PRBool nsXIFDTD::GetAttribute(const nsIParserNode& aNode, const nsString& aKey, nsString& aValue)
{
  PRInt32   i;
  PRInt32   count = aNode.GetAttributeCount();
  for (i = 0; i < count; i++)
  {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.Equals(aKey))
    {
      const nsString& value = aNode.GetValueAt(i);
      aValue = value;
      aValue.StripChars("\"");
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}



/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
PRBool nsXIFDTD::GetAttributePair(nsIParserNode& aNode, nsString& aKey, nsString& aValue)
{
 
  PRInt32 count = aNode.GetAttributeCount();
  PRInt32 i;
  PRBool  hasValue = PR_FALSE;

  for (i = 0; i < count; i++)
  {
    const nsString& k = aNode.GetKeyAt(i);
    const nsString& v = aNode.GetValueAt(i);

    nsAutoString key; key.Assign(k);
    nsAutoString value; value.Assign(v);

    char*  quote = "\"";
 
    key.StripChars(quote);
    value.StripChars(quote);

    if (key.EqualsWithConversion("name"))
      aKey = value;
    if (key.EqualsWithConversion("value"))
    {
      aValue = value;
      hasValue = PR_TRUE;
    }
  }
  return hasValue;
}

/**
 *  Closes the top most contaier node on the internal stack..
 *
 *  @update  harishd 04/12/00
 *  @param   aNode  - The leaf node
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult nsXIFDTD::CloseContainer(const nsIParserNode& aNode)
{
  nsresult result=NS_OK;

  PreprocessStack();
  
  nsCParserNode* theNode=(nsCParserNode*)mXIFContext->Pop();
  if(theNode) {
    eHTMLTags theTag=(theNode->mToken)? (eHTMLTags)theNode->mToken->GetTypeID():eHTMLTag_unknown;
    
    // theNode->mUseCount is decremented by one when it's popped out of the
    // context, i.e. theNode->mUseCount will be 1 after pop. But inorder to
    // recycle the node, mUseCount should be set to zero...so let's decrement 
    // by one more..
    theNode->mUseCount--; // Do this to recycle the node...
    if(IsHTMLContainer(theTag) && theTag!=eHTMLTag_unknown) {
      result=mSink->CloseContainer(aNode); 
    }
    mNodeRecycler->RecycleNode(theNode);
  }
  return result;
}

/**
 *  process leaf nodes
 *
 *  @update  harishd 04/12/00
 *  @param   aNode  - The leaf node
 *  @return  NS_OK if all went well; NS_ERROR_XXX if error occured
 */

nsresult nsXIFDTD::AddLeaf(const nsIParserNode& aNode)
{
  nsresult result=NS_OK;

  nsCParserNode* theNode=(nsCParserNode*)&aNode;

  if(theNode) {
    CToken*  theToken=((nsCParserNode&)aNode).mToken;
    eXIFTags theTag=(theToken)? (eXIFTags)theToken->GetTypeID():eXIFTag_unknown;
    PRBool   handled=PR_FALSE;
 
    switch(theTag) {
      case eXIFTag_newline:
         if(mInContent) mLineNumber++;
      case eXIFTag_whitespace:
         if(!mInContent) handled=PR_TRUE;
         break;
      case eXIFTag_text:
        if(theToken) {
          nsString& temp =theToken->GetStringValueXXX();
          if (temp.EqualsWithConversion("<xml version=\"1.0\"?>")) handled=PR_TRUE;
        }
        break;
      default:
        break;
    }
    if(!handled) {
       // Handle top node that has not been passed 
       // on to the sink yet.
       PreprocessStack();
       result=mSink->AddLeaf(aNode);
    }
  }
  
  return result;
}


/**
 * Retrieve the preferred tokenizer for use by this DTD.
 * @update  gess12/28/98
 * @param   none
 * @return  ptr to tokenizer
 */
nsresult nsXIFDTD::GetTokenizer(nsITokenizer*& aTokenizer) {
  nsresult result=NS_OK;
  if(!mTokenizer) {
    result=NS_NewXMLTokenizer(&mTokenizer);
  }
  aTokenizer=mTokenizer;
  return result;
}


/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
nsTokenAllocator* nsXIFDTD::GetTokenAllocator(void){
  nsITokenizer* theTokenizer=0;
  
  nsresult result=GetTokenizer(theTokenizer);

  if (NS_SUCCEEDED(result)) {
    return theTokenizer->GetTokenAllocator();
  }
  return 0;
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
nsresult  nsXIFDTD::Terminate(nsIParser* aParser)
{
  return NS_ERROR_HTMLPARSER_STOPPARSING;
}

/**
 * Retrieve the attributes for this node, and add then into
 * the node.
 *
 * @update  gess4/22/98
 * @param   aNode is the node you want to collect attributes for
 * @param   aCount is the # of attributes you're expecting
 * @return error code (should be 0)
 */
nsresult nsXIFDTD::CollectAttributes(nsCParserNode& aNode,PRInt32 aCount){
  int attr=0;
  for(attr=0;attr<aCount;attr++){
    CToken* theToken=mTokenizer->PeekToken();
    if(theToken)  {
      eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
      if(eToken_attribute==theType){
        mTokenizer->PopToken(); //pop it for real...
        aNode.AddAttribute(theToken);
      } 
    }
    else return NS_ERROR_FAILURE;
  }
  return NS_OK;
}


/**
 * Causes the next skipped-content token (if any) to
 * be consumed by this node.
 * @update	gess5/11/98
 * @param   node to consume skipped-content
 * @param   holds the number of skipped content elements encountered
 * @return  Error condition.
 */
nsresult nsXIFDTD::CollectSkippedContent(nsCParserNode& aNode,PRInt32& aCount) {
  nsresult           result=NS_OK;
  return result;
}

/**
 * 
 * @update	gpk 06/18/98
 * @param 
 * @return
 */
nsresult nsXIFDTD::WillResumeParse(void){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillResume();
  }
  return result;
}

/**
 * 
 * @update	gpk 06/18/98
 * @param 
 * @return
 */
nsresult nsXIFDTD::WillInterruptParse(void){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillInterrupt();
  }
  return result;
}


/************************************************************************
  Here's a bunch of stuff JEvering put into the parser to do debugging.
 ************************************************************************/




/**
 * This debug method allows us to determine whether or not 
 * we've seen (and can handle) the given context vector.
 *
 * @update  gpk 06/18/98
 * @param   tags is an array of eXIFTags
 * @param   count represents the number of items in the tags array
 * @param   aDTD is the DTD we plan to ask for verification
 * @return  TRUE if we know how to handle it, else false
 */
PRBool nsXIFDTD::VerifyContextVector(void) const {

  PRBool  result=PR_TRUE;
  return result;
}

/**
 * Called by the parser to enable/disable dtd verification of the
 * internal context stack.
 * @update	gess 7/23/98
 * @param 
 * @return
 */
void nsXIFDTD::SetVerification(PRBool aEnabled){
}


/**
 * Called by the parser to initiate dtd verification of the
 * internal context stack.
 * @update	gess 7/23/98
 * @param 
 * @return
 */
PRBool nsXIFDTD::Verify(nsString& aURLRef,nsIParser* aParser) {
  PRBool result=PR_TRUE;
  mParser=(nsParser*)aParser;
  return result;
}


void nsXIFDTD::SetURLRef(char * aURLRef)
{
}

/**
 * 
 * @update	harishd 04/12/00
 * @param  aNode - 
 * @return NS_OK if success.
 */

nsresult nsXIFDTD::ProcessEncodeTag(const nsIParserNode& aNode)
{
  nsresult result=NS_OK;
  nsString value;
  PRInt32  error;

  if (GetAttribute(aNode,NS_ConvertToString("selection"),value))
  {
    PRInt32 temp = value.ToInteger(&error);
    if (temp == 1)
    {
      result=mSink->DoFragment(PR_TRUE);
    }
  }
  else
    result=mSink->DoFragment(PR_FALSE);

  return result;
}

/**
 * 
 * 
 * @update	harishd 04/12/00
 * @param  aNode - The entity node
 * @return NS_OK if success.
 */

nsresult nsXIFDTD::ProcessEntityTag(const nsIParserNode& aNode)
{
  nsresult result=NS_OK;
  nsAutoString value;

  // Get the entity from the node. This resides as an
  // attribute on the node. 
  //
  // XXX - Entities within STYLE and SCRIPT should probably be translated. 
  // Why were we given entities in the first place. Could be a problem in
  // the XIF converter. 
  // Ex. <SCRIPT> document.write("a<b"); </SCRIPT> is being given to us, by the
  // converter as <SCRIPT> document.write("a &lt; b"); </SCRIPT>.
  // That's totally worng. 
  if (GetAttribute(aNode,NS_ConvertToString("value"),value)) {
    value.AppendWithConversion(';');
    CToken* entity=((nsCParserNode&)aNode).mToken;
    if(entity) {
      entity->Reinitialize(eHTMLTag_entity,value);
      nsCParserNode*  thePeekNode=(nsCParserNode*)mXIFContext->PeekNode();
      if(thePeekNode) {
        eHTMLTags theTag=(eHTMLTags)thePeekNode->mToken->GetTypeID();
        if(theTag==eHTMLTag_script || theTag==eHTMLTag_style) {
          nsAutoString scratch;
          ((CEntityToken*)entity)->TranslateToUnicodeStr(scratch);  // Ex. &gt would become '<'
          entity->Reinitialize(eHTMLTag_text,scratch); // Covert type to text and set the translated value.
        }
      }
      ((nsCParserNode&)aNode).Init(entity,mLineNumber,mTokenAllocator);
    }
    result=mSink->AddLeaf(aNode);
  }
  return result;
}

/*******************************
 * ----- CSS Methods Begin-----*
 *                             *
 * XXX - CURRENTLY NOT IN USE  *
 *                             *
 *******************************/

nsresult nsXIFDTD::BeginCSSStyleSheet(const nsIParserNode& aNode)
{
  nsresult result=NS_OK;

  PRInt32  error;
  nsAutoString value;
  CToken* theToken=((nsCParserNode&)aNode).mToken;
  
  if(theToken) {
    theToken->Reinitialize(eHTMLTag_style,NS_ConvertASCIItoUCS2("style"));
    mXIFContext->Push(&aNode);
  }

  mBuffer.Truncate(0);
  mMaxCSSSelectorWidth = 10;
  if (GetAttribute(aNode,mCSSStyleSheetKey,value))
  {
    PRInt32 temp = value.ToInteger(&error);
    if (error == NS_OK)
      mMaxCSSSelectorWidth = temp;
  }
  return result;
}

nsresult nsXIFDTD::EndCSSStyleSheet(const nsIParserNode& aNode)
{
  nsresult result=NS_OK;
  nsAutoString tagName; tagName.AssignWithConversion(nsHTMLTags::GetStringValue(eHTMLTag_style));

  mBuffer.AppendWithConversion("</");
  mBuffer.Append(tagName);
  mBuffer.AppendWithConversion(">");
  ((nsCParserNode&)aNode).SetSkippedContent(mBuffer);
  
  result=mSink->AddLeaf(aNode);
  mNodeRecycler->RecycleNode((nsCParserNode*)mXIFContext->Pop());
  return result;
}

nsresult nsXIFDTD::BeginCSSStyleRule(const nsIParserNode& aNode)
{
  mCSSDeclarationCount = 0;
  mCSSSelectorCount = 0; 
  return NS_OK;
}


nsresult nsXIFDTD::EndCSSStyleRule(const nsIParserNode& aNode)
{
  return NS_OK;
}


nsresult nsXIFDTD::AddCSSSelector(const nsIParserNode& aNode)
{
  nsAutoString value;

  if (GetAttribute(aNode,mCSSSelectorKey, value))
  {
    if (mLowerCaseAttributes == PR_TRUE)
      value.ToLowerCase();
    else
      value.ToUpperCase();
    value.CompressWhitespace();
    mBuffer.Append(value);
  }
  return NS_OK;
}

nsresult nsXIFDTD::BeginCSSDeclarationList(const nsIParserNode& aNode)
{
  nsresult result=NS_OK;
  PRInt32 indx = mBuffer.RFindChar('\n');
  if (indx == kNotFound)
    indx = 0;
  PRInt32 offset = mBuffer.Length() - indx;
  PRInt32 count = mMaxCSSSelectorWidth - offset;

  if (count < 0)
    count = 0;

  for (PRInt32 i = 0; i < count; i++)
    mBuffer.AppendWithConversion(" ");

  mBuffer.AppendWithConversion("   {");
  mCSSDeclarationCount = 0;
  return result;
}

nsresult nsXIFDTD::EndCSSDeclarationList(const nsIParserNode& aNode)
{
  mBuffer.AppendWithConversion("}\n");
  return NS_OK;
}


nsresult nsXIFDTD::AddCSSDeclaration(const nsIParserNode& aNode)
{
  nsAutoString property;
  nsAutoString value;


  if (PR_TRUE == GetAttribute(aNode, mCSSDeclarationKey, property)) {
    if (PR_TRUE == GetAttribute(aNode, mGenericKey, value))
    {
      if (mCSSDeclarationCount != 0)
        mBuffer.AppendWithConversion(";");
      mBuffer.AppendWithConversion(" ");
      mBuffer.Append(property);
      mBuffer.AppendWithConversion(": ");
      mBuffer.Append(value);
      mCSSDeclarationCount++;
    }
  }
  return NS_OK;
}

/*******************************
 * ----- CSS Methods Ends------*
 *******************************/

