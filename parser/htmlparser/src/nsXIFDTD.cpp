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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_XIF_DTD_IID); 

static const char* kNullToken = "Error: Null token given";
static const char* kInvalidTagStackPos = "Error: invalid tag stack position";
static const char* kXIFDocHeader= "<!DOCTYPE xif>";
  

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

  {"encode",                eXIFTag_encode},

  {"import",                eXIFTag_import},

  {"leaf",                  eXIFTag_leaf},
  {"link",                  eXIFTag_link},

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
    result=aString.Compare(gXIFTagTable[middle].mName, PR_TRUE); 
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

/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsHTMLParser.
 *  
 *  @update  gpk 06/18/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_NewXIFDTD(nsIDTD** aInstancePtrResult)
{
  nsXIFDTD* it = new nsXIFDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(nsXIFDTD)
NS_IMPL_RELEASE(nsXIFDTD)



/**
 *  
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
static
PRInt32 XIFDispatchTokenHandler(CToken* aToken,nsIDTD* aDTD) {
  
  PRInt32         result=0;

  eHTMLTokenTypes theType= (eHTMLTokenTypes)aToken->GetTokenType();  
  nsXIFDTD*       theDTD=(nsXIFDTD*)aDTD;

  nsString& name = aToken->GetStringValueXXX();
  eXIFTags type = DetermineXIFTagType(name);
  
  if (type != eXIFTag_userdefined)
    aToken->SetTypeID(type);
  
  if(aDTD) {
    switch(theType) {
      case eToken_start:
        result=theDTD->HandleStartToken(aToken); break;
      case eToken_end:
        result=theDTD->HandleEndToken(aToken); break;
      case eToken_comment:
        result=theDTD->HandleCommentToken(aToken); break;
      case eToken_entity:
        result=theDTD->HandleEntityToken(aToken); break;
      case eToken_whitespace:
        result=theDTD->HandleWhiteSpaceToken(aToken); break;
      case eToken_newline:
        result=theDTD->HandleWhiteSpaceToken(aToken); break;
      case eToken_text:
        result=theDTD->HandleTextToken(aToken); break;
      case eToken_attribute:
        result=theDTD->HandleAttributeToken(aToken); break;
      default:
        result=0;
    }//switch
  }//if
  return result;
}

/**
 *  init the set of default token handlers...
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
void nsXIFDTD::InitializeDefaultTokenHandlers() {
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_start));

  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_end));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_comment));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_entity));

  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_whitespace));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_newline));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_text));
  
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_attribute));
//  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_script));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_style));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_skippedcontent));
}

class nsXIfTokenDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    CToken* aToken = (CToken*)anObject;
    delete aToken;
    return 0;
  }
};



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
  nsCRT::zero(mContextStack,sizeof(mContextStack));
  nsCRT::zero(mTokenHandlers,sizeof(mTokenHandlers));
  
  mHTMLStackPos = 0;
  memset(mHTMLTagStack,0,sizeof(mHTMLTagStack));
  memset(mHTMLNameStack,0,sizeof(mHTMLNameStack));

  
  mContextStackPos=0;
  mHasOpenForm=PR_FALSE;
  mHasOpenMap=PR_FALSE;
  InitializeDefaultTokenHandlers();
  mContextStackPos=0;
  mContextStack[mContextStackPos++]=eXIFTag_unknown;
  mDTDDebug=nsnull;
  mInContent=PR_FALSE;
  mLowerCaseAttributes=PR_TRUE;
  mLowerCaseTags=PR_TRUE;
}

/**
 *  Default destructor
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
nsXIFDTD::~nsXIFDTD(){
  DeleteTokenHandlers();
//  NS_RELEASE(mSink);
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
eAutoDetectResult nsXIFDTD::CanParse(nsString& aContentType, nsString& aCommand, nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;
  if(aContentType.Equals(kXIFTextContentType)){
    result=ePrimaryDetect;
  }
  else {
    if(kNotFound!=aBuffer.Find(kXIFDocHeader)) {
      PRInt32 offset = aBuffer.Find("<section>");
      if (offset != -1)
        aBuffer.Cut(0,offset);
      aContentType= kXIFTextContentType;
      result=ePrimaryDetect;
    }
  }
  return result;
}


/**
 * 
 * @update	gess 7/24/98
 * @param 
 * @return
 */
nsresult nsXIFDTD::WillBuildModel(nsString& aFileName,PRBool aNotifySink,nsString& aSourceType,nsIContentSink* aSink){
  nsresult result=NS_OK;

  mSink=(nsIHTMLContentSink*)aSink;
  if(mSink) {
    mSink->WillBuildModel();
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
NS_IMETHODIMP nsXIFDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  if(aTokenizer) {
    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    nsITokenRecycler* theRecycler=aTokenizer->GetTokenRecycler();

    while(NS_OK==result){
      CToken* theToken=mTokenizer->PopToken();
      if(theToken) {
        result=HandleToken(theToken,aParser);
        if(NS_SUCCEEDED(result)) {
          theRecycler->RecycleToken(theToken);
        }
        else if(NS_ERROR_HTMLPARSER_BLOCK!=result){
          mTokenizer->PushTokenFront(theToken);
        }
        // theRootDTD->Verify(kEmptyString,aParser);
      }
      else break;
    }//while
    mTokenizer=oldTokenizer;
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
 *  This big dispatch method is used to route token handler calls to the right place.
 *  What's wrong with it? This table, and the dispatch methods themselves need to be 
 *  moved over to the delegate. Ah, so much to do...
 *  
 *  @update  gpk 06/18/98
 *  @param   aType
 *  @param   aToken
 *  @param   aParser
 *  @return  
 */
nsresult nsXIFDTD::HandleToken(CToken* aToken,nsIParser* aParser) {
  nsresult result=NS_OK;

  mParser=(nsParser*)aParser;
  mSink=(nsIHTMLContentSink*)aParser->GetContentSink();  //this can change in the parser between calls.

  if(aToken) {
    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    CTokenHandler*  aHandler=GetTokenHandler(theType);

    if(aHandler) {
      result=(*aHandler)(theToken,this);
    }

  }//if
  return result;
}

/**
 *  This method causes all tokens to be dispatched to the given tag handler.
 *
 *  @update  gess 3/25/98
 *  @param   aHandler -- object to receive subsequent tokens...
 *  @return	 error code (usually 0)
 */
nsresult nsXIFDTD::CaptureTokenPump(nsITagHandler* aHandler) {
  nsresult result=NS_OK;
  return result;
}

/**
 *  This method releases the token-pump capture obtained in CaptureTokenPump()
 *
 *  @update  gess 3/25/98
 *  @param   aHandler -- object that received tokens...
 *  @return	 error code (usually 0)
 */
nsresult nsXIFDTD::ReleaseTokenPump(nsITagHandler* aHandler){
  nsresult result=NS_OK;
  return result;
}


/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult nsXIFDTD::HandleWhiteSpaceToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStartToken*  st    = (CStartToken*)(aToken);
  eXIFTags      type  =(eXIFTags)st->GetTypeID();

  //Begin by gathering up attributes...
  nsCParserNode node((CHTMLToken*)aToken);
  PRInt16       attrCount=aToken->GetAttributeCount();
  nsresult      result=(0==attrCount) ? NS_OK : CollectAttributes(node,attrCount);

  if(NS_OK==result)
  {
    if (mInContent == PR_TRUE)
      mSink->AddLeaf(node);
  } 
  return result;
}


/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult nsXIFDTD::HandleTextToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStartToken*    st    = (CStartToken*)(aToken);
  eXIFTags        type  =(eXIFTags)st->GetTypeID();
  nsCParserNode   node((CHTMLToken*)aToken);

  nsresult result = NS_OK;

  if (type == eXIFTag_text)
  {
    nsString& temp = aToken->GetStringValueXXX();

    if (temp != "<xml version=\"1.0\"?>")
    {
      result= AddLeaf(node);
    }
  }

  return result;
}


void nsXIFDTD::AddAttribute(nsIParserNode& aNode) {
  nsString key;
  nsString value;
  PRBool   hasValue;

  hasValue = GetAttributePair(aNode,key,value);
  CAttributeToken* attribute = new CAttributeToken(key,value);
  nsIParserNode* top = PeekNode();
  if (top != nsnull)
    ((nsCParserNode*)top)->AddAttribute(attribute);
}


/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 12/28/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult nsXIFDTD::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStartToken*  st    = (CStartToken*)(aToken);
  eXIFTags      type  =(eXIFTags)st->GetTypeID();

  //Begin by gathering up attributes...
  nsCParserNode   node = ((CHTMLToken*)aToken);
  PRInt16         attrCount=aToken->GetAttributeCount();
  nsresult        result=(0==attrCount) ? NS_OK : CollectAttributes(node,attrCount);

  if(NS_OK==result)   {
    switch (type)
    {
      case eXIFTag_container:
      case eXIFTag_leaf: 
        StartTopOfStack();
        result = OpenContainer(node);        
      break;

      case eXIFTag_content:
        StartTopOfStack();
        mInContent = PR_TRUE;
      break;

      case eXIFTag_encode:
        ProcessEncodeTag(node);
      break;


      case eXIFTag_attr:
        AddAttribute(node);
      break;

      case eXIFTag_css_stylesheet:
        StartTopOfStack();
        BeginCSSStyleSheet(node);
      break;

      case eXIFTag_css_rule:
        BeginCSSStyleRule(node);
      break;

      case eXIFTag_css_selector:
        AddCSSSelector(node);
      break;

      case eXIFTag_css_declaration_list:
        BeginCSSDeclarationList(node);
      break;

      case eXIFTag_css_declaration:
        AddCSSDeclaration(node);
      break;

    }
  } 
  return result;
}


/**
 *  This method gets called when an end token has been 
 *  encountered in the parse process. If the end tag matches
 *  the start tag on the stack, then simply close it. Otherwise,
 *  we have a erroneous state condition. This can be because we
 *  have a close tag with no prior open tag (user error) or because
 *  we screwed something up in the parse process. I'm not sure
 *  yet how to tell the difference.
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult nsXIFDTD::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult        result        = NS_OK;
  CEndToken*      et            = (CEndToken*)(aToken);
  eXIFTags        tokenTagType  = (eXIFTags)et->GetTypeID();
  nsCParserNode   node          = ((CHTMLToken*)aToken);

  switch (tokenTagType)
  {
    case eXIFTag_container:
    case eXIFTag_leaf:
      StartTopOfStack();
      result=CloseContainer(node); 
    break;

    case eXIFTag_content:
      mInContent = PR_FALSE;
    break;
    
    case eXIFTag_css_stylesheet:
      mInContent = PR_FALSE;
      EndCSSStyleSheet(node);
    break;

    case  eXIFTag_css_rule:
      mInContent = PR_FALSE;
      EndCSSStyleRule(node);
    break;


    case eXIFTag_css_declaration_list:
      mInContent = PR_FALSE;
      EndCSSDeclarationList(node);
    break;
  }
  return result;
}

/**
 *  This method gets called when an entity token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult nsXIFDTD::HandleEntityToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CEntityToken* et = (CEntityToken*)(aToken);
  nsresult      result=NS_OK;
  eXIFTags      tokenTagType=(eXIFTags)et->GetTypeID();

  if(PR_FALSE==CanOmit(GetTopNode(),tokenTagType)) {
    nsCParserNode aNode((CHTMLToken*)aToken);
    result=AddLeaf(aNode);
  }
  return result;
}

/**
 *  This method gets called when a comment token has been 
 *  encountered in the parse process. After making sure
 *  we're somewhere in the body, we handle the comment
 *  in the same code that we use for text.
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult nsXIFDTD::HandleCommentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  return NS_OK;
}

/**
 *  This method gets called when an attribute token has been 
 *  encountered in the parse process. This is an error, since
 *  all attributes should have been accounted for in the prior
 *  start or end tokens
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult nsXIFDTD::HandleAttributeToken(CToken* aToken) {
  CAttributeToken*  at = (CAttributeToken*)(aToken);
  NS_PRECONDITION(0!=aToken,kNullToken);
  NS_ERROR("attribute encountered -- this shouldn't happen!");

  nsresult result=NS_OK;
  return result;
}



/**
 *  Finds a tag handler for the given tag type, given in string.
 *  
 *  @update  gpk 06/18/98
 *  @param   aString contains name of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
void nsXIFDTD::DeleteTokenHandlers(void) {
  int i=0;
  for(i=eToken_unknown;i<eToken_last;i++){
    delete mTokenHandlers[i];
    mTokenHandlers[i]=0;
  }
  return;
}


/**
 *  Finds a tag handler for the given tag type.
 *  
 *  @update  gpk 06/18/98
 *  @param   aTagType type of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
CTokenHandler* nsXIFDTD::GetTokenHandler(eHTMLTokenTypes aType) const {
  CTokenHandler* result=0;
  if((aType>0) && (aType<eToken_last)) {
    result=mTokenHandlers[aType];
  } 
  else {
  }
  return result;
}


/**
 *  Register a handler.
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
CTokenHandler* nsXIFDTD::AddTokenHandler(CTokenHandler* aHandler) {
  NS_ASSERTION(0!=aHandler,"Error: Null handler");
  
  if(aHandler)  {
    eHTMLTokenTypes type=(eHTMLTokenTypes)aHandler->GetTokenType();
    if(type<eToken_last) {
      CTokenHandler* old=mTokenHandlers[type];
      mTokenHandlers[type]=aHandler;
    }
    else {
      //add code here to handle dynamic tokens...
    }
  }
  return 0;
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
  PRBool result=PR_FALSE;
  return result;
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
 *  This method gets called to determine whether a given 
 *  tag can contain newlines. Most do not.
 *  
 *  @update  gpk 06/18/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool nsXIFDTD::CanOmit(eXIFTags aParent,eXIFTags aChild) const {
  PRBool result=PR_FALSE;
  return result;
}


/**
 *  This method gets called to determine whether a given
 *  ENDtag can be omitted. Admittedly,this is a gross simplification.
 *  
 *  @update  gpk 06/18/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool nsXIFDTD::CanOmitEndTag(eXIFTags aParent,eXIFTags aChild) const {
  PRBool result=PR_FALSE;
      
  switch(aChild) 
  {
    case eXIFTag_attr:
      result=PR_TRUE; 

    default:
      result=PR_FALSE;
  } 
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gpk 06/18/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool nsXIFDTD::IsXIFContainer(eXIFTags aTag) const {
  PRBool result=PR_FALSE;

  switch(aTag){
    case eXIFTag_attr:
    case eXIFTag_text:
    case eXIFTag_whitespace:
    case eXIFTag_newline:
      result=PR_FALSE;
    break;

    default:
      result=PR_TRUE;
  }
  return result;
}


/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gpk 06/18/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool nsXIFDTD::IsHTMLContainer(eHTMLTags aTag) const {
  PRBool result=PR_TRUE; // by default everything is a 

  switch(aTag) {
    case eHTMLTag_meta:
    case eHTMLTag_br:
    case eHTMLTag_hr:
      result=PR_FALSE;
      break;
    default:
      break;

  }
  return result;
}



/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gpk 06/18/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
eXIFTags nsXIFDTD::GetDefaultParentTagFor(eXIFTags aTag) const{
  
  eXIFTags result=eXIFTag_unknown;
  
  switch(aTag) 
  {
    case eXIFTag_section:
      result=eXIFTag_unknown; break;

    case eXIFTag_section_body:
    case eXIFTag_section_head:
      result=eXIFTag_section; break;

    default:
      break;

   }
  return result;
}



/**
 * 
 * @update	gpk 06/18/98
 * @param   aTag is the id of the html container being opened
 * @return  0 if all is well.
 */
nsresult nsXIFDTD::DidOpenContainer(eXIFTags aTag,PRBool /*anExplicitOpen*/){
  nsresult   result=NS_OK;
  return result;
}

/**
 * 
 * @update	gpk 06/18/98
 * @param 
 * @return
 */
nsresult nsXIFDTD::DidCloseContainer(eXIFTags aTag,PRBool/*anExplicitClosure*/){
  nsresult result=NS_OK;
  return result;
}


/**
 *  This method allows the caller to determine if a form
 *  element is currently open.
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
PRBool nsXIFDTD::HasOpenContainer(eXIFTags aContainer) const {
  PRBool result=PR_FALSE;

  result=(kNotFound!=GetTopmostIndexOf(aContainer));

  return result;
}

/**
 *  This method retrieves the HTMLTag type of the topmost
 *  container on the stack.
 *  
 *  @update  gpk 06/18/98
 *  @return  tag id of topmost node in contextstack
 */
eXIFTags nsXIFDTD::GetTopNode() const {
  if(mContextStackPos) 
    return mContextStack[mContextStackPos-1];
  return eXIFTag_unknown;
}


/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gpk 06/18/98
 *  @param   eXIFTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRInt32 nsXIFDTD::GetTopmostIndexOf(eXIFTags aTag) const {
  int i=0;
  for(i=mContextStackPos-1;i>=0;i--){
    if(mContextStack[i]==aTag)
      return i;
  }
  return kNotFound;
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

    nsAutoString key(k);
    nsAutoString value(v);

    char*  quote = "\"";
 
    key.StripChars(quote);
    value.StripChars(quote);

    if (key.Equals("name"))
      aKey = value;
    if (key.Equals("value"))
    {
      aValue = value;
      hasValue = PR_TRUE;
    }
  }
  return hasValue;
}



/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
eHTMLTags nsXIFDTD::GetHTMLTag(const nsString& aName)
{
  eHTMLTags  tag = eHTMLTag_unknown;
  
  if (aName.EqualsIgnoreCase("style"))
    tag = eHTMLTag_style;
  else
  {
    char buffer[256];

    aName.ToCString(buffer,255);
    tag = NS_TagToEnum(buffer);
  }
  return tag;
}




/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
eHTMLTags nsXIFDTD::GetStartTag(const nsIParserNode& aNode, nsString& aName)
{
  eXIFTags  type = (eXIFTags)aNode.GetNodeType();  
  eHTMLTags tag = eHTMLTag_unknown;

  switch (type)
  {
    case eXIFTag_container:
    case eXIFTag_leaf:
      if (GetAttribute(aNode,nsString("isa"),aName))
        tag = GetHTMLTag(aName);
    break;

    case eXIFTag_css_stylesheet:
      aName = "style";
      tag = GetHTMLTag(aName);
    break;  
  }
  return tag;
}



/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
void nsXIFDTD::PushHTMLTag(const eHTMLTags aTag, const nsString& aName)
{

  mHTMLTagStack[mHTMLStackPos]=aTag;
  mHTMLNameStack[mHTMLStackPos]=new nsAutoString(aName);
  mHTMLTagStack[++mHTMLStackPos]=eHTMLTag_unknown;
}


/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
void nsXIFDTD::PopHTMLTag(eHTMLTags& aTag, nsString*& aName)
{
  NS_ASSERTION(mHTMLStackPos > 0,"The stack must not be empty");

  aTag = eHTMLTag_unknown;
  if (mHTMLStackPos > 0)
  {
    aTag = mHTMLTagStack[mHTMLStackPos-1];
    aName = mHTMLNameStack[mHTMLStackPos-1];
    mHTMLTagStack[--mHTMLStackPos] = eHTMLTag_unknown;
  }
}



/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
void nsXIFDTD::PushNodeAndToken(nsString& aName)
{
  CToken*         token = new CStartToken(aName);
  nsCParserNode*  node = new nsCParserNode(token);

  mTokenStack.AppendElement((void*)token);
  mNodeStack.AppendElement((void*)node);  
}


/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
nsIParserNode* nsXIFDTD::PeekNode()
{
  PRInt32 count = mNodeStack.Count()-1;
  
  if (count >= 0)
    return (nsIParserNode*)mNodeStack.ElementAt(mNodeStack.Count()-1);
  return nsnull;
}


/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
CToken* nsXIFDTD::PeekToken()
{
  PRInt32 count = mTokenStack.Count()-1; 
  if (count >= 0)
    return (CToken*)mTokenStack.ElementAt(mTokenStack.Count()-1);
  return nsnull;
}


/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
void nsXIFDTD::PopAndDelete()
{
  nsIParserNode* node = PeekNode();
  CToken* token = PeekToken();

  if (node != nsnull && token != nsnull)
  {
    mNodeStack.RemoveElement(node);
    mTokenStack.RemoveElement(token);
    
    delete node;
    delete token;
  }
}


/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
PRBool nsXIFDTD::StartTopOfStack()
{
  // If something is already on the stack, then
  // emit it and pop it off the stack
  nsIParserNode* top = PeekNode();
  if (top != nsnull)
  {
    eHTMLTags  tag = (eHTMLTags)top->GetNodeType();
    
    if (IsHTMLContainer(tag))
    {
      mInContent = PR_TRUE;
      mSink->OpenContainer(*top);
    }
    else
      mSink->AddLeaf(*top);
    
    PopAndDelete();
    return PR_TRUE;
  }
  return PR_FALSE;
}



/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
void nsXIFDTD::BeginStartTag(const nsIParserNode& aNode)
{
  eXIFTags  type = (eXIFTags)aNode.GetNodeType();
  eHTMLTags tag;
  nsString  tagName;

  switch (type)
  {
    case eXIFTag_container:
    case eXIFTag_leaf:
        tag = GetStartTag(aNode,tagName);
        if (type == eXIFTag_container)
          PushHTMLTag(tag,tagName);  
 
        CToken*         token = new CStartToken(tagName);
        nsCParserNode*  node = new nsCParserNode(token);
        PushNodeAndToken(tagName);
      break;
  }

}

void nsXIFDTD::AddEndTag(const nsIParserNode& aNode)
{
  // Get the top the HTML stack
  eHTMLTags   tag;
  nsString*   name = nsnull;
  PopHTMLTag(tag,name);
   
  // Create a parse node for form this token
  CEndToken     token(*name);
  nsCParserNode node(&token);

  // close the container 
  mSink->CloseContainer(node); 

  // delete the name
  if (name != nsnull)
    delete name;
 
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gpk 06/18/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult nsXIFDTD::OpenContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  
  nsresult  result=NS_OK; 
  eXIFTags  type =(eXIFTags)aNode.GetNodeType();

  switch (type)
  {
    case eXIFTag_container:
    case eXIFTag_leaf:
      BeginStartTag(aNode);
    break;
  }    
  mContextStack[mContextStackPos++]=type;

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gpk 06/18/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult nsXIFDTD::CloseContainer(const nsIParserNode& aNode)
{
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  nsresult   result=NS_OK; //was false
  eXIFTags type=(eXIFTags)aNode.GetNodeType();
  
  if (type == eXIFTag_container)
    AddEndTag(aNode);

  mContextStack[--mContextStackPos]=eXIFTag_unknown;

 
  return result;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gpk 06/18/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult nsXIFDTD::AddLeaf(const nsIParserNode& aNode)
{
  eXIFTags  type = (eXIFTags)aNode.GetNodeType();    
  nsresult result=mSink->AddLeaf(aNode); 
  return result;
}

/**
 *  This method gets called to create a valid context stack
 *  for the given child. We compare the current stack to the
 *  default needs of the child, and push new guys onto the
 *  stack until the child can be properly placed.
 *
 *  @update  gpk 06/18/98
 *  @param   aChildTag is the child for whom we need to 
 *           create a new context vector
 *  @return  true if we succeeded, otherwise false
 */
nsresult nsXIFDTD::CreateContextStackFor(eXIFTags aChildTag)
{
  mContextStack[++mContextStackPos] = aChildTag;
  return NS_OK;
}


/**
 * 
 * @update	gess12/28/98
 * @param 
 * @return
 */
nsITokenizer* nsXIFDTD::GetTokenizer(void){
  if(!mTokenizer)
    mTokenizer=new nsXMLTokenizer();
  return mTokenizer;
}

/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
nsITokenRecycler* nsXIFDTD::GetTokenRecycler(void){
  nsITokenizer* theTokenizer=GetTokenizer();
  return theTokenizer->GetTokenRecycler();
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
    else return kEOF;
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

void nsXIFDTD::ProcessEncodeTag(const nsIParserNode& aNode)
{
  nsString value;
  PRInt32  error;

  if (GetAttribute(aNode,nsString("selection"),value))
  {
    PRInt32 temp = value.ToInteger(&error);
    if (temp == 1)
    {
      mSink->DoFragment(PR_TRUE);
      return;
    }
  }
  mSink->DoFragment(PR_FALSE);
}


/*** CSS Methods ****/

void nsXIFDTD::BeginCSSStyleSheet(const nsIParserNode& aNode)
{
  nsString value;
  PRInt32  error;

  mBuffer.Truncate(0);
  mMaxCSSSelectorWidth = 10;
  if (GetAttribute(aNode,nsString("max_css_selector_width"),value))
  {
    PRInt32 temp = value.ToInteger(&error);
    if (error == NS_OK)
      mMaxCSSSelectorWidth = temp;
  }
  
  const char* name = NS_EnumToTag(eHTMLTag_html);
}

void nsXIFDTD::EndCSSStyleSheet(const nsIParserNode& aNode)
{
  const char* name = NS_EnumToTag(eHTMLTag_style);

  nsString tagName(name);

  if (mLowerCaseTags == PR_TRUE)
    tagName.ToLowerCase();
  else
    tagName.ToUpperCase();

  CStartToken   startToken(tagName);
  nsCParserNode startNode((CToken*)&startToken);

  mBuffer.Append("</");
  mBuffer.Append(tagName);
  mBuffer.Append(">");
  CSkippedContentToken* skipped = new CSkippedContentToken(mBuffer);
  nsString& key = skipped->GetKey();
  key = mBuffer;
  
  startNode.SetSkippedContent(skipped);  
  mSink->AddLeaf(startNode);

}

void nsXIFDTD::BeginCSSStyleRule(const nsIParserNode& aNode)
{
  mCSSDeclarationCount = 0;
  mCSSSelectorCount = 0;  
}


void nsXIFDTD::EndCSSStyleRule(const nsIParserNode& aNode)
{
}


void nsXIFDTD::AddCSSSelector(const nsIParserNode& aNode)
{
  nsString value;

  if (GetAttribute(aNode, nsString("selectors"), value))
  {
    if (mLowerCaseAttributes == PR_TRUE)
      value.ToLowerCase();
    else
      value.ToUpperCase();
    value.CompressWhitespace();
    mBuffer.Append(value);
  }  
}

void nsXIFDTD::BeginCSSDeclarationList(const nsIParserNode& aNode)
{
  PRInt32 index = mBuffer.RFind('\n');
  if (index == kNotFound)
    index = 0;
  PRInt32 offset = mBuffer.Length() - index;
  PRInt32 count = mMaxCSSSelectorWidth - offset;

  if (count < 0)
    count = 0;

  for (PRInt32 i = 0; i < count; i++)
    mBuffer.Append(" ");

  mBuffer.Append("   {");
  mCSSDeclarationCount = 0;

}

void nsXIFDTD::EndCSSDeclarationList(const nsIParserNode& aNode)
{
  mBuffer.Append("}\n");
}


void nsXIFDTD::AddCSSDeclaration(const nsIParserNode& aNode)
{
  nsString property;
  nsString value;


  if (PR_TRUE == GetAttribute(aNode, nsString("property"), property))
    if (PR_TRUE == GetAttribute(aNode, nsString("value"), value))
    {
      if (mCSSDeclarationCount != 0)
        mBuffer.Append(";");
      mBuffer.Append(" ");
      mBuffer.Append(property);
      mBuffer.Append(": ");
      mBuffer.Append(value);
      mCSSDeclarationCount++;
    }
}


 