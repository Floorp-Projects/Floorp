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

#include "nsxpfcCIID.h"
#include "nsXPFCXMLDTD.h"
#include "nsParser.h"
#include "nsParserNode.h"
#include "nsXPFCXMLContentSink.h"
#include "nsHTMLTokens.h"
#include "nsParserCIID.h"
#include "nsCRT.h"
#include "nsxpfcstrings.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_IXPFCXML_DTD_IID); 
static NS_DEFINE_IID(kBaseClassIID, NS_INAVHTML_DTD_IID); 

static NS_DEFINE_IID(kCParserNodeCID, NS_PARSER_NODE_IID); 
static NS_DEFINE_IID(kCParserNodeIID, NS_IPARSER_NODE_IID); 

static const char* kMenuXMLTextContentType = "ui/menubar";
static const char* kMenuXMLDocHeader= "<!DOCTYPE menubar";
static const char* kToolbarXMLTextContentType = "ui/toolbar";
static const char* kToolbarXMLDocHeader= "<!DOCTYPE toolbar";
static const char* kDialogXMLTextContentType = "ui/dialog";
static const char* kDialogXMLDocHeader= "<!DOCTYPE dialog";
static const char* kApplicationXMLTextContentType = "ui/application";
static const char* kApplicationXMLDocHeader= "<!DOCTYPE application";

struct nsXPFCXMLTagEntry {
  char      mName[32];
  eXPFCXMLTags  fTagID;
};

nsXPFCXMLTagEntry gXPFCXMLTagTable[] =
{
  {"!!UNKNOWN",             eXPFCXMLTag_unknown},
  {"!DOCTYPE",              eXPFCXMLTag_doctype},
  {"?XML",                  eXPFCXMLTag_xml},
  {"application",           eXPFCXMLTag_application},
  {"button",                eXPFCXMLTag_button},
  {"canvas",                eXPFCXMLTag_canvas},
  {"dialog",                eXPFCXMLTag_dialog},
  {"editfield",             eXPFCXMLTag_editfield},
  {"menubar",               eXPFCXMLTag_menubar},
  {"menucontainer",         eXPFCXMLTag_menucontainer},
  {"menuitem",              eXPFCXMLTag_menuitem},
  {"object",                eXPFCXMLTag_object},
  {"separator",             eXPFCXMLTag_separator},
  {"set",                   eXPFCXMLTag_set},
  {"tabwidget",             eXPFCXMLTag_tabwidget},
  {"toolbar",               eXPFCXMLTag_toolbar},
  {"ui",                    eXPFCXMLTag_ui},
  {"userdefined",           eXPFCXMLTag_userdefined},
  {"xpbutton",              eXPFCXMLTag_xpbutton},

};

eXPFCXMLTags DetermineXPFCXMLTagType(const nsString& aString)
{
  PRInt32  result=-1;
  PRInt32  cnt=sizeof(gXPFCXMLTagTable)/sizeof(nsXPFCXMLTagEntry);
  PRInt32  low=0; 
  PRInt32  high=cnt-1;
  PRInt32  middle=0;

  while(low<=high){
    middle=(PRInt32)(low+high)/2;
    //result=aString.Compare(gXPFCXMLTagTable[middle].mName, nsCRT::strlen(gXPFCXMLTagTable[middle].mName), PR_TRUE);
    result=aString.Compare(gXPFCXMLTagTable[middle].mName, PR_TRUE);
    if (result==0)
      return gXPFCXMLTagTable[middle].fTagID; 
    if (result<0)
      high=middle-1; 
    else low=middle+1; 
  }
  return eXPFCXMLTag_userdefined;
}


PRInt32 XPFCXMLDispatchTokenHandler(CToken* aToken,nsIDTD* aDTD) {
  
  PRInt32         result=0;

  eHTMLTokenTypes theType= (eHTMLTokenTypes)aToken->GetTokenType();  
  nsXPFCXMLDTD*    theDTD=(nsXPFCXMLDTD*)aDTD;

  nsString& name = aToken->GetStringValueXXX();
  eXPFCXMLTags type = DetermineXPFCXMLTagType(name);
  
  if (type != eXPFCXMLTag_userdefined)
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
      case eToken_attribute:
        result=theDTD->HandleAttributeToken(aToken); break;
      default:
        result=0;
    }//switch
  }//if
  return result;
}


nsresult nsXPFCXMLDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kBaseClassIID)) {  //do nav dtd base class...
    *aInstancePtr = (CNavDTD*)(this);                                        
  }
  else if(aIID.Equals(kIDTDIID)) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsXPFCXMLDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(nsXPFCXMLDTD)
NS_IMPL_RELEASE(nsXPFCXMLDTD)

nsXPFCXMLDTD::nsXPFCXMLDTD() : CNavDTD() 
{
  NS_INIT_REFCNT();
}

nsXPFCXMLDTD::~nsXPFCXMLDTD()
{
}

PRBool nsXPFCXMLDTD::CanParse(nsString& aContentType, PRInt32 aVersion)
{
  if (aContentType == kMenuXMLTextContentType || aContentType == kToolbarXMLTextContentType || aContentType == kDialogXMLTextContentType || aContentType == kApplicationXMLTextContentType)
    return PR_TRUE;

  return PR_FALSE;
}

eAutoDetectResult nsXPFCXMLDTD::AutoDetectContentType(nsString& aBuffer,nsString& aType)
{
  if ((aType == kMenuXMLTextContentType) || (aBuffer.Find(kMenuXMLDocHeader) != -1))
  {
    aType = kMenuXMLTextContentType;
    return eValidDetect;
  }

  if ((aType == kToolbarXMLTextContentType) || (aBuffer.Find(kToolbarXMLDocHeader) != -1))
  {
    aType = kToolbarXMLTextContentType;
    return eValidDetect;
  }

  if ((aType == kDialogXMLTextContentType) || (aBuffer.Find(kDialogXMLDocHeader) != -1))
  {
    aType = kDialogXMLTextContentType;
    return eValidDetect;
  }

  if ((aType == kApplicationXMLTextContentType) || (aBuffer.Find(kApplicationXMLDocHeader) != -1))
  {
    aType = kApplicationXMLTextContentType;
    return eValidDetect;
  }

  return eUnknownDetect;
}


nsresult nsXPFCXMLDTD::HandleToken(CToken* aToken)
{
  nsresult result=NS_OK;

  if(aToken) {

    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());

    result=XPFCXMLDispatchTokenHandler(theToken,this);

  }
  return result;

}



nsresult nsXPFCXMLDTD::CreateNewInstance(nsIDTD** aInstancePtrResult)
{
  static NS_DEFINE_IID(kCCalXPFCXMLDTD, NS_IXPFCXML_DTD_IID);

  nsresult result = nsRepository::CreateInstance(kCCalXPFCXMLDTD, 
                                                 nsnull, 
                                                 kIDTDIID,
                                                 (void**) aInstancePtrResult);

  return (result);
}


nsresult nsXPFCXMLDTD::HandleStartToken(CToken* aToken) 
{
  CStartToken * st          = (CStartToken*)aToken;
  eXPFCXMLTags tokenTagType  = (eXPFCXMLTags) st->GetTypeID();
  nsCParserNode * attrNode = nsnull;

  //Begin by gathering up attributes...
  static NS_DEFINE_IID(kCParserNodeCID, NS_PARSER_NODE_IID); 
  static NS_DEFINE_IID(kCParserNodeIID, NS_IPARSER_NODE_IID); 

  nsresult result = nsRepository::CreateInstance(kCParserNodeCID, nsnull, kCParserNodeIID,(void**) &attrNode);

  if (NS_OK != result)
    return result;

  attrNode->Init((CHTMLToken*)aToken,mLineNumber);

  PRInt16       attrCount=aToken->GetAttributeCount();
  result=(0==attrCount) ? NS_OK : CollectAttributes(*attrNode,attrCount);

  if (tokenTagType == eXPFCXMLTag_object) {
    tokenTagType = TagTypeFromObject(*attrNode);
    st->SetTypeID(tokenTagType);
  }

  if(NS_OK==result) {
  
      switch(tokenTagType) {

        case eXPFCXMLTag_xml:
        case eXPFCXMLTag_doctype:
        case eXPFCXMLTag_ui:
           break;

        /*
         * the Panel Tag represents the core container object for layout
         */

        case eXPFCXMLTag_object:
        case eXPFCXMLTag_menubar:
        case eXPFCXMLTag_menucontainer:
        case eXPFCXMLTag_toolbar:
        case eXPFCXMLTag_canvas:
        case eXPFCXMLTag_application:
        case eXPFCXMLTag_dialog:
        {
          mSink->OpenContainer(*attrNode);
        }
        break;

        case eXPFCXMLTag_menuitem:
        case eXPFCXMLTag_button:
        case eXPFCXMLTag_xpbutton:
        case eXPFCXMLTag_editfield:
        case eXPFCXMLTag_separator:
        case eXPFCXMLTag_tabwidget:
        {
          if (((nsXPFCXMLContentSink *)mSink)->IsContainer(*attrNode) == PR_TRUE)
            st->SetTypeID(eXPFCXMLTag_menucontainer);
          mSink->AddLeaf(*attrNode);
        }
        break;

        case eXPFCXMLTag_set:
           break;

        default:
          break;
      }
  } 

  NS_RELEASE(attrNode);

  if(eHTMLTag_newline==tokenTagType)
    mLineNumber++;

  return result;
}

nsresult nsXPFCXMLDTD::HandleEndToken(CToken* aToken) 
{

  nsresult    result=NS_OK;
  CEndToken*  et = (CEndToken*)(aToken);
  eXPFCXMLTags   tokenTagType=(eXPFCXMLTags)et->GetTypeID();
  nsCParserNode * attrNode = nsnull;

  static NS_DEFINE_IID(kCParserNodeCID, NS_PARSER_NODE_IID); 
  static NS_DEFINE_IID(kCParserNodeIID, NS_IPARSER_NODE_IID); 

  result = nsRepository::CreateInstance(kCParserNodeCID, nsnull, kCParserNodeIID, (void**)&attrNode);

  if (NS_OK != result)
    return result;

  attrNode->Init((CHTMLToken*)aToken,mLineNumber);

  if (tokenTagType == eXPFCXMLTag_object) {
    tokenTagType = TagTypeFromObject(*attrNode);
    et->SetTypeID(tokenTagType);
  }

  switch(tokenTagType) {

    case eXPFCXMLTag_xml:
    case eXPFCXMLTag_doctype:
    case eXPFCXMLTag_ui:

    case eXPFCXMLTag_object:
    case eXPFCXMLTag_menubar:
    case eXPFCXMLTag_menucontainer:
    case eXPFCXMLTag_toolbar:
    case eXPFCXMLTag_canvas:
    case eXPFCXMLTag_dialog:
    case eXPFCXMLTag_application:
    {
      mSink->CloseContainer(*attrNode);
    }
    break;

    case eXPFCXMLTag_menuitem:
    case eXPFCXMLTag_editfield:
    case eXPFCXMLTag_separator:
    case eXPFCXMLTag_button:
    case eXPFCXMLTag_xpbutton:
    case eXPFCXMLTag_tabwidget:
       break;

    case eXPFCXMLTag_set:
       break;


    default:
      break;
  }

  NS_RELEASE(attrNode);

  return result;
}

eXPFCXMLTags nsXPFCXMLDTD::TagTypeFromObject(const nsIParserNode& aNode) 
{
  PRInt32 i = 0;
  
  for (i = 0; i < aNode.GetAttributeCount(); i++) {
   
   nsString key = aNode.GetKeyAt(i);

   key.StripChars("\"");

   if (key.EqualsIgnoreCase(XPFC_STRING_CLASS)) {

      nsString value = aNode.GetValueAt(i);
    
      value.StripChars("\"");

      if (value.EqualsIgnoreCase("button"))
        return (eXPFCXMLTag_button);
      else if (value.EqualsIgnoreCase("separator"))
        return (eXPFCXMLTag_separator);
      else if (value.EqualsIgnoreCase("xpbutton"))
        return (eXPFCXMLTag_xpbutton);
      else if (value.EqualsIgnoreCase("editfield"))
        return (eXPFCXMLTag_editfield);
      else if (value.EqualsIgnoreCase("tabwidget"))
        return (eXPFCXMLTag_tabwidget);
   }
   
  }

  return (eXPFCXMLTag_unknown);

}