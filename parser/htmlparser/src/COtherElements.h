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

/************************************************************************
 * MODULE NOTES: 
 * @update  gess 04.08.2000
 *
 *  - CElement::mAutoClose should only be set for tags whose end tag 
 *    is optional.
 *
 *
 ************************************************************************/
#ifndef _COTHERELEMENTS_
#define _COTHERELEMENTS_

#undef  TRANSITIONAL

/************************************************************************
  This union is a bitfield which describes the group membership
 ************************************************************************/


struct CGroupBits {
  PRUint32  mHead: 1;     
  PRUint32  mHeadMisc: 1;     //script, style, meta, link, object 
  PRUint32  mHeadContent: 1;  //title, base
  PRUint32  mFontStyle : 1;
  PRUint32  mPhrase: 1;
  PRUint32  mSpecial: 1;
  PRUint32  mFormControl: 1;
  PRUint32  mHeading: 1;
  PRUint32  mBlock: 1;
  PRUint32  mFrame:1;
  PRUint32  mList: 1;
  PRUint32  mPreformatted: 1;
  PRUint32  mTable: 1;
  PRUint32  mSelf: 1;
  PRUint32  mLeaf: 1;
  PRUint32  mWhiteSpace: 1;
  PRUint32  mComment: 1;
  PRUint32  mTextContainer: 1;
  PRUint32  mTopLevel: 1;
  PRUint32  mDTDInternal: 1;
  PRUint32  mFlowEntity: 1;
  PRUint32  mBlockEntity: 1;
  PRUint32  mInlineEntity: 1;
};

union CGroupMembers {
  PRUint32    mAllBits;
  CGroupBits  mBits;
};


inline PRBool ContainsGroup(CGroupMembers& aGroupSet,CGroupMembers& aGroup) {
  PRBool result=PR_FALSE;
  if(aGroup.mAllBits) {
    result=PRBool(aGroupSet.mAllBits & aGroup.mAllBits);
  }
  return result;
}

inline PRBool ListContainsTag(eHTMLTags* aTagList,eHTMLTags aTag) {
  if(aTagList) {
    eHTMLTags *theNextTag=aTagList;
    while(eHTMLTag_unknown!=*theNextTag) {
      if(aTag==*theNextTag) {
        return PR_TRUE;
      }
      theNextTag++;
    }
  }
  return PR_FALSE;
}


/**********************************************************
  Begin with the baseclass for all elements...
 **********************************************************/
class CElement {
public:

    //break this struct out seperately so that lame compilers don't gack.
  struct CFlags {
    PRUint32  mOmitEndTag:1;
    PRUint32  mIsContainer:1;
    PRUint32  mForDTDUseOnly:1;
    PRUint32  mDiscardTag:1;
    PRUint32  mLegalOpen:1;
    PRUint32  mPropagateDepth:4;
    PRUint32  mBadContentWatch:1;
    PRUint32  mNoStylesLeakOut:1;
    PRUint32  mNoStylesLeakIn:1;
    PRUint32  mMustCloseSelf:1;
    PRUint32  mSaveMisplaced:1;
    PRUint32  mHandleStrayTags:1;
    PRUint32  mRequiresBody:1;
    PRUint32  mOmitWS:1;
  };

  union {
    PRUint32  mAllBits;
    CFlags    mProperties;
  };

  CElement(eHTMLTags aTag=eHTMLTag_unknown) {
    mAllBits=0;
    mTag=aTag;
    mGroup.mAllBits=0;
    mContainsGroups.mAllBits=0;
    mAutoClose=mIncludeKids=mExcludeKids=0;
    mDelegate=eHTMLTag_unknown;
  }

  CElement( eHTMLTags aTag,CGroupMembers&  aGroup)  {
    mAllBits=0;
    mTag=aTag;
    mGroup=aGroup;
    mContainsGroups.mAllBits=0;
    mAutoClose=mIncludeKids=mExcludeKids=0;
    mDelegate=eHTMLTag_unknown;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    anElement.mProperties.mIsContainer=0;
    anElement.mTag=aTag;
    anElement.mGroup.mAllBits=0;;
    anElement.mContainsGroups.mAllBits=0;
  }

  static void InitializeLeaf(CElement& anElement,eHTMLTags aTag,CGroupMembers& aGroup,CGroupMembers& aContainsGroups) {
    anElement.mProperties.mIsContainer=PR_FALSE;
    anElement.mTag=aTag;
    anElement.mGroup.mAllBits=aGroup.mAllBits;
    anElement.mContainsGroups.mAllBits=aContainsGroups.mAllBits;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag,CGroupMembers& aGroup,CGroupMembers& aContainsGroups) {
    anElement.mProperties.mIsContainer=PR_TRUE;
    anElement.mTag=aTag;
    anElement.mGroup.mAllBits=aGroup.mAllBits;
    anElement.mContainsGroups.mAllBits=aContainsGroups.mAllBits;
  }

  static PRBool HasOptionalEndTag(CElement* anElement) {
    static eHTMLTags gContainersWithOptionalEndTag[]={eHTMLTag_body,eHTMLTag_colgroup,eHTMLTag_dd,eHTMLTag_dt,
                                                      eHTMLTag_head,eHTMLTag_html,eHTMLTag_li,eHTMLTag_option,
                                                      eHTMLTag_p,eHTMLTag_tbody,eHTMLTag_td,eHTMLTag_tfoot,
                                                      eHTMLTag_th,eHTMLTag_thead,eHTMLTag_tr,eHTMLTag_unknown};
    if(anElement) {
      return ListContainsTag(gContainersWithOptionalEndTag,anElement->mTag);  
    }
    return PR_FALSE;
  }

  inline CElement*  GetDelegate(void);
  inline CElement*  GetDefaultContainerFor(CElement* anElement);

  virtual PRBool    CanContain(CElement* anElement,nsDTDContext* aContext);
  virtual PRBool    CanBeClosedByStartTag(CElement* anElement,nsDTDContext* aContext);
  virtual PRBool    CanBeClosedByEndTag(CElement* anElement,nsDTDContext* aContext);

  virtual PRBool    IsSectionTag(void)    {return PR_FALSE;}
  virtual PRBool    IsContainer(void)     {return mProperties.mIsContainer;}
  virtual PRBool    IsChildOfHead(void)   {return PR_FALSE;}
  virtual PRBool    IsWhiteSpaceTag(void) {return PR_FALSE;}

  virtual eHTMLTags GetSkipTarget(void)   {return eHTMLTag_unknown;}
  virtual nsresult  HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink);
  virtual nsresult  HandleEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink);

  virtual nsresult  HandleMisplacedStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    return result;
  }

  virtual nsresult  HandleMisplacedEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    return result;
  }

  eHTMLTags       mTag;
  eHTMLTags       mDelegate;
  CGroupMembers   mGroup;
  CGroupMembers   mContainsGroups;
  eHTMLTags       *mIncludeKids;
  eHTMLTags       *mExcludeKids;
  eHTMLTags       *mAutoClose;    //other start tags that close this container


};


/**********************************************************
  This defines the Special element group
 **********************************************************/
class CLeafElement: public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mLeaf=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups={0};
    theGroups.mBits.mLeaf=1;
    theGroups.mBits.mWhiteSpace=1;
    return theGroups;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::InitializeLeaf(anElement,aTag,GetGroup(),GetContainedGroups());
  }


  CLeafElement(eHTMLTags aTag) : CElement(aTag) {
    mProperties.mIsContainer=0;
  }

};

/**********************************************************
  This defines elements that are deprecated
 **********************************************************/
class CDeprecatedElement: public CElement {
public:

  CDeprecatedElement(eHTMLTags aTag) : CElement(aTag) {
    CElement::Initialize(*this,aTag);
  }

};

/**********************************************************
  This defines elements that are for use only by the DTD
 **********************************************************/
class CInlineElement: public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mInlineEntity=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroup={0};
    static PRBool initialized=PR_FALSE;
    if(!initialized) {
      initialized=PR_TRUE;
      theGroup.mBits.mFormControl=1;
      theGroup.mBits.mFontStyle =1;
      theGroup.mBits.mPhrase=1;
      theGroup.mBits.mSpecial=1;
      theGroup.mBits.mList=1;
      theGroup.mBits.mPreformatted=1;
      theGroup.mBits.mSelf=1;
      theGroup.mBits.mLeaf=1;
      theGroup.mBits.mWhiteSpace=1;
      theGroup.mBits.mComment=1;
      theGroup.mBits.mInlineEntity=1;
    }
    return theGroup;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }



  CInlineElement(eHTMLTags aTag) : CElement(aTag) {
    CInlineElement::Initialize(*this,aTag);
  }

};

/**********************************************************
  This defines the Block element group
 **********************************************************/
class CBlockElement : public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theBlockGroup={0};
    theBlockGroup.mBits.mBlock=1;
    return theBlockGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups=CInlineElement::GetContainedGroups();
    theGroups.mBits.mSelf=0;
    return theGroups;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CBlockElement(eHTMLTags aTag) : CElement(aTag) {
    CBlockElement::Initialize(*this,aTag);
  }

};


/************************************************************
  This defines flowEntity elements that contain block+inline
 ************************************************************/
class CFlowElement: public CInlineElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mFlowEntity=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroup={0};
    theGroup=CInlineElement::GetContainedGroups();
    theGroup.mBits.mBlock=1;
    theGroup.mBits.mBlockEntity=1;
    return theGroup; 
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CFlowElement(eHTMLTags aTag) : CInlineElement(aTag) {
    CFlowElement::Initialize(*this,aTag);
  }

};

/**********************************************************
  This defines the Phrase element group
 **********************************************************/
class CPhraseElement: public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers thePhraseGroup={0};
    thePhraseGroup.mBits.mPhrase=1;
    return thePhraseGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups=CInlineElement::GetContainedGroups();
    return theGroups;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CPhraseElement(eHTMLTags aTag) : CElement(aTag) {
    CPhraseElement::Initialize(*this,aTag);
  }

};

/**********************************************************
  This defines the formcontrol element group
 **********************************************************/
class CFormControlElement: public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mFormControl=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mFormControl=1;
    theGroup.mBits.mLeaf=1;
    theGroup.mBits.mWhiteSpace=1;
    return theGroup;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CFormControlElement(eHTMLTags aTag) : CElement(aTag) {
    CFormControlElement::Initialize(*this,aTag);
  }

};

/**********************************************************
  This defines the fontstyle element group
 **********************************************************/
class CFontStyleElement: public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mFontStyle=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups=CInlineElement::GetContainedGroups();
    return theGroups;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CFontStyleElement(eHTMLTags aTag) : CElement(aTag) {
    CFontStyleElement::Initialize(*this,aTag);
  }

};

/**********************************************************
  This defines the preformatted element group
 **********************************************************/
class CPreformattedElement: public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers thePreformattedGroup={0};
    thePreformattedGroup.mBits.mPreformatted=1;
    return thePreformattedGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups={0};
    theGroups.mBits.mPreformatted=1;
    return theGroups;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CPreformattedElement(eHTMLTags aTag) : CElement(aTag) {
    mGroup=GetGroup();
    mContainsGroups=GetContainedGroups();
    mProperties.mIsContainer=1;
  }

};


/**********************************************************
  This defines the special-inline element group
 **********************************************************/
class CSpecialElement : public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mSpecial=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups=CInlineElement::GetContainedGroups();
    return theGroups;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CSpecialElement(eHTMLTags aTag) : CElement(aTag) {
    CSpecialElement::Initialize(*this,aTag);
  }

};


/**********************************************************
  This defines elements that belong to tables
 **********************************************************/
class CTableElement: public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theTableGroup={0};
    theTableGroup.mBits.mTable=1;
    return theTableGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups={0};
    theGroups.mBits.mTable=1;
    return theGroups;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CTableElement(eHTMLTags aTag) : CElement(aTag) {
    CElement::Initialize(*this,aTag,CBlockElement::GetGroup(),CTableElement::GetContainedGroups());
  }

};

/**********************************************************
  This defines the List element group (ol,ul,dir,menu)
 **********************************************************/
class CListElement: public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theListGroup={0};
    theListGroup.mBits.mList=1;
    return theListGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups=CInlineElement::GetContainedGroups();
    return theGroups;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CListElement(eHTMLTags aTag) : CElement(aTag) {
    CListElement::Initialize(*this,aTag);
  }

};

/**********************************************************
  This defines the heading element group (h1..h6)
 **********************************************************/
class CHeadingElement: public CElement {
public:


  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mHeading=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups=CInlineElement::GetContainedGroups();
    return theGroups;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CHeadingElement(eHTMLTags aTag) : CElement(aTag) {
    CHeadingElement::Initialize(*this,aTag);
  }

};

/**********************************************************
  This defines the tags that relate to frames
 **********************************************************/
class CFrameElement: public CElement {
public:


  static void Initialize(CElement& anElement,eHTMLTags aTag){
    anElement.mProperties.mIsContainer=1;
    anElement.mTag=aTag;
    anElement.mGroup.mAllBits=0;
    anElement.mGroup.mBits.mFrame=1;
    anElement.mContainsGroups.mAllBits=0;
    anElement.mContainsGroups.mBits.mFrame=1;
  }

  CFrameElement(eHTMLTags aTag) : CElement(aTag) {
    CFrameElement::Initialize(*this,aTag);
  }

};


/**********************************************************
  This defines elements that are for use only by the DTD
 **********************************************************/
class CDTDInternalElement: public CElement {
public:

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    anElement.mProperties.mIsContainer=1;
    anElement.mTag=aTag;
    anElement.mContainsGroups.mAllBits=0;
    anElement.mGroup.mBits.mDTDInternal=1;
  }

  CDTDInternalElement(eHTMLTags aTag) : CElement(aTag) {
    CDTDInternalElement::Initialize(*this,aTag);
  }


};

/**********************************************************
  Here comes the head element
 **********************************************************/
class CHeadElement: public CElement {
public:


  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theHeadGroup={0};
    theHeadGroup.mBits.mTopLevel=1;
    return theHeadGroup;
  }

  static CGroupMembers& GetContentGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mHeadContent=1;
    return theGroup;
  }

  static CGroupMembers& GetMiscGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mHeadMisc=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroupsContainedByHead={0};
    theGroupsContainedByHead.mBits.mHeadMisc=1;
    theGroupsContainedByHead.mBits.mHeadContent=1;
    return theGroupsContainedByHead;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CHeadElement(eHTMLTags aTag) : CElement(aTag) {
    CHeadElement::Initialize(*this,aTag);
  }

  virtual nsresult HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {

      case eHTMLTag_script:
      case eHTMLTag_style:
      case eHTMLTag_title:
        aContext->Push(aNode); //force the title onto the stack
        break;

      case eHTMLTag_base: //nothing to do for these empty tags...      
      case eHTMLTag_isindex:
      case eHTMLTag_link:
      case eHTMLTag_meta:
        result=aSink->AddLeaf(*aNode);
      default:
        break;
    }
    return result;
  }

  virtual nsresult HandleEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {

      case eHTMLTag_script:
        break;

      case eHTMLTag_style:
        break;

      case eHTMLTag_title:
        {
          nsEntryStack* theStack=0;
          aContext->Pop(theStack); //force the title onto the stack
        }
        break;

      case eHTMLTag_head:
        {
          nsEntryStack* theStack=0;
          aContext->Pop(theStack);
        }
        break;

      case eHTMLTag_base:     //nothing to do for these empty tags...
      case eHTMLTag_isindex:
      case eHTMLTag_link:
      case eHTMLTag_meta:
      default:
        break;
    }

    return result;
  }


};

/**********************************************************
  This class is for use with title, script, style
 **********************************************************/
class CTextContainer : public CElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mTextContainer=1; 
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theContainedGroups={0};
    theContainedGroups.mBits.mLeaf=1;
    return theContainedGroups;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CTextContainer(eHTMLTags aTag) : CElement(aTag) {
    CTextContainer::Initialize(*this,aTag);
  }

  virtual nsresult HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_whitespace:
      case eHTMLTag_text:
        mText.Append(aNode->GetText());
        break;
      default:
        break;
    }

    return result;
  }

  virtual nsresult  EmitText(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    if(aNode) {
      aSink->OpenHead(*aNode);
      nsCParserNode* theNode=(nsCParserNode*)aNode;
      theNode->SetSkippedContent(mText);
      aSink->AddLeaf(*aNode);
      aSink->CloseHead(*aNode);
    }
    mText.Truncate(0);
    return result;
  }

  virtual nsresult HandleEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    if(mTag==aTag) {
      result=EmitText(aNode,aTag,aContext,aSink);
      nsEntryStack* theStack=0;
      aContext->Pop(theStack);
    }
    return result;
  }

  nsString  mText;
};


/**********************************************************
  This class is for the title element
 **********************************************************/
class CTitleElement : public CTextContainer {
public:

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CTextContainer::Initialize(anElement,aTag);
  }

  CTitleElement() : CTextContainer(eHTMLTag_title) {
    mGroup.mBits.mHeadMisc=1;
  }


  virtual nsresult HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_whitespace:
      case eHTMLTag_text:
        mText.Append(aNode->GetText());
        break;
      default:
        break;
    }

    return result;
  }

  virtual nsresult  EmitText(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    aSink->OpenHead(*aNode);
    aSink->SetTitle(mText);
    aSink->CloseHead(*aNode);
    mText.Truncate(0);
    return result;
  }

  virtual nsresult HandleEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    if(mTag==aTag) {
      result=EmitText(aNode,aTag,aContext,aSink);
      nsEntryStack* theStack=0;
      aContext->Pop(theStack);
    }
    return result;
  }

  nsString  mText;
};

/**********************************************************
  This class is for use with style
 **********************************************************/
class CStyleElement: public CTextContainer {
public:

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CTextContainer::Initialize(anElement,aTag);
  }

  CStyleElement() : CTextContainer(eHTMLTag_style) {
    mGroup.mBits.mHeadMisc=1;
  }


};

/**********************************************************
  This class is for use with script
 **********************************************************/
class CScriptElement: public CTextContainer {
public:

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CTextContainer::Initialize(anElement,aTag);
  }

  CScriptElement() : CTextContainer(eHTMLTag_script) {
    mGroup.mBits.mHeadMisc=1;
    mGroup.mBits.mBlock=1;
  }

};


/**********************************************************
  This is for FRAMESET, etc.
 **********************************************************/
class CTopLevelElement: public CElement {
public:


  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mTopLevel=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroup=CFlowElement::GetContainedGroups();
    return theGroup;
  }


  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CTopLevelElement(eHTMLTags aTag) : CElement(aTag) {
    CTopLevelElement::Initialize(*this,aTag);
  }

  virtual nsresult HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_unknown:
      default:
        result=CElement::HandleStartToken(aNode,aTag,aContext,aSink);
        break;
    }//switch

    return result;
  }

  virtual nsresult HandleEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

    nsEntryStack* theStack=0;
    nsresult      result=NS_OK;

    switch(aTag) {
      case eHTMLTag_html:
        if(aContext->HasOpenContainer(aTag)) {
          result=aSink->CloseHTML(*aNode);
          aContext->Pop(theStack);
        }
        break;

      case eHTMLTag_body:
        if(aContext->HasOpenContainer(aTag)) {
          result=aSink->CloseBody(*aNode);
          aContext->Pop(theStack);
        }
        break;

      case eHTMLTag_frameset:
        if(aContext->HasOpenContainer(aTag)) {
          result=aSink->OpenFrameset(*aNode); 
          aContext->Pop(theStack);
        }
        break;

      default:
        result=CElement::HandleEndToken(aNode,aTag,aContext,aSink);
        break;
    }//switch
    
    return result;
  }

};


/**********************************************************
  This is for HTML only...
 **********************************************************/
class CHTMLElement: public CTopLevelElement{
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theBlockGroup={0};
    theBlockGroup.mBits.mTopLevel=1;
    return theBlockGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups={0};
    theGroups.mBits.mTopLevel=1;
    return theGroups;
  }

  CHTMLElement(eHTMLTags aTag) : CTopLevelElement(aTag) {   
    CElement::Initialize(*this,aTag,CHTMLElement::GetGroup(),CHTMLElement::GetContainedGroups());
  }

  nsresult AddHeadElement(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink);

  virtual nsresult HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_body:
        if(!aContext->HasOpenContainer(aTag)) {
          result=aSink->OpenBody(*aNode);
          aContext->Push(aNode);
        }
        break;

      case eHTMLTag_frameset:
        if(!aContext->HasOpenContainer(aTag)) {
          result=aSink->OpenFrameset(*aNode); 
          aContext->Push(aNode);
        }
        break;

      case eHTMLTag_base:
      case eHTMLTag_meta:
      case eHTMLTag_style:
      case eHTMLTag_script:
        result=AddHeadElement(aNode,aTag,aContext,aSink);
        break;

      case eHTMLTag_head:
        aContext->Push(aNode);
        break;

      default:
        //for now, let's drop other elements onto the floor.
        break;
    }//switch

    return result;
  }

  virtual nsresult HandleEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_unknown:
      default:
        result=CTopLevelElement::HandleEndToken(aNode,aTag,aContext,aSink);
    }
    return result;
  }

};

/**********************************************************
  This is for the body element...
 **********************************************************/
class CBodyElement: public CElement {
public:


  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mTopLevel=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    static CGroupMembers theGroups={0};
    theGroups.mBits.mBlock=1;
#ifdef  TRANSITIONAL
    theGroups.mBits.mComment=1;
    theGroups.mBits.mSpecial=1;
    theGroups.mBits.mPhrase=1;
    theGroups.mBits.mFontStyle=1;
    theGroups.mBits.mFormControl=1;
    theGroups.mBits.mLeaf=1;

#endif
    return theGroups;
  }


  CBodyElement(eHTMLTags aTag=eHTMLTag_body) : CElement(aTag) {    
    CElement::Initialize(*this,aTag,CBodyElement::GetGroup(),CBodyElement::GetContainedGroups());
  }

  virtual nsresult HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {

      case eHTMLTag_div:
    
      default:
        //for now, let's drop other elements onto the floor.
        result=CElement::HandleStartToken(aNode,aTag,aContext,aSink);
        break;
    }//switch

    return result;
  }

  virtual nsresult HandleEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_div:
      default:
        result=CElement::HandleEndToken(aNode,aTag,aContext,aSink);
    }
    return result;
  }

};


/************************************************************************
  This describes each group that each HTML element belongs to
 ************************************************************************/
class CElementTable {
public:
  enum  {eGroupCount=4};  

  CElementTable() :

    mBodyElement(eHTMLTag_body),
    mFramesetElement(eHTMLTag_frameset),
    mHeadElement(eHTMLTag_head),
    mHTMLElement(eHTMLTag_html),
    mScriptElement(),
    mStyleElement(),
    mTitleElement()

  {
    memset(mElements,0,sizeof(mElements));
    InitializeElements();
    //DebugDumpContainment("all elements");
  }


  void InitializeElements();
  void DebugDumpGroups(CElement* aParent);
  void DebugDumpContainment(const char* aTitle);
  void DebugDumpContainment(CElement* aParent);

  CElement* mElements[150];  //add one here for special handling of a given element
  CElement  mDfltElements[150];

  CBodyElement    mBodyElement;
  CFrameElement   mFramesetElement;
  CHeadElement    mHeadElement;
  CHTMLElement    mHTMLElement;
  CScriptElement  mScriptElement;
  CStyleElement   mStyleElement;
  CTitleElement   mTitleElement;
};


static CElementTable *gElementTable = 0;

static eHTMLTags kDLKids[]={eHTMLTag_dd,eHTMLTag_dt,eHTMLTag_unknown};
static eHTMLTags kAutoCloseDD[]={eHTMLTag_dd,eHTMLTag_dt,eHTMLTag_dl,eHTMLTag_unknown};
static eHTMLTags kButtonExcludeKids[]={ eHTMLTag_a,eHTMLTag_button,eHTMLTag_select,eHTMLTag_textarea,
                                        eHTMLTag_input,eHTMLTag_iframe,eHTMLTag_form,eHTMLTag_isindex,
                                        eHTMLTag_fieldset,eHTMLTag_unknown};
static eHTMLTags kColgroupKids[]={eHTMLTag_col,eHTMLTag_unknown};
static eHTMLTags kDirKids[]={eHTMLTag_li,eHTMLTag_unknown};
static eHTMLTags kOptionGroupKids[]={eHTMLTag_option,eHTMLTag_unknown};
static eHTMLTags kFieldsetKids[]={eHTMLTag_legend,eHTMLTag_unknown};
static eHTMLTags kFormKids[]={eHTMLTag_button,eHTMLTag_input,eHTMLTag_select,eHTMLTag_textarea,eHTMLTag_unknown};
static eHTMLTags kFormExcludeKids[]={eHTMLTag_form,eHTMLTag_unknown};
static eHTMLTags kLIExcludeKids[]={eHTMLTag_dir,eHTMLTag_menu,eHTMLTag_unknown};
static eHTMLTags kMapKids[]={eHTMLTag_area,eHTMLTag_unknown};
static eHTMLTags kPreExcludeKids[]={eHTMLTag_image,eHTMLTag_object,eHTMLTag_applet,
                                    eHTMLTag_big,eHTMLTag_small,eHTMLTag_sub,eHTMLTag_sup,
                                    eHTMLTag_font,eHTMLTag_basefont,eHTMLTag_unknown};
static eHTMLTags kSelectKids[]={eHTMLTag_optgroup,eHTMLTag_option,eHTMLTag_unknown};
static eHTMLTags kBlockQuoteKids[]={eHTMLTag_script,eHTMLTag_unknown};
static eHTMLTags kFramesetKids[]={eHTMLTag_noframes,eHTMLTag_unknown};
static eHTMLTags kObjectKids[]={eHTMLTag_param,eHTMLTag_unknown};

/***********************************************************************************
  This method is pretty interesting, because it's where the elements all get 
  initialized for this elementtable.
 ***********************************************************************************/
void CElementTable::InitializeElements() {

  int max=sizeof(mElements)/sizeof(mElements[0]);
  int index=0;
  for(index=0;index<max;index++){
    mElements[index]=&mDfltElements[index];
  }

  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_a],          eHTMLTag_a);
  mDfltElements[eHTMLTag_a].mContainsGroups.mBits.mSelf=0;

  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_abbr],       eHTMLTag_abbr);
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_acronym],    eHTMLTag_acronym);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_address],    eHTMLTag_address);

  CElement::Initialize(             mDfltElements[eHTMLTag_applet],     eHTMLTag_applet,CSpecialElement::GetGroup(), CFlowElement::GetContainedGroups());

  CElement::Initialize(             mDfltElements[eHTMLTag_area],       eHTMLTag_area);
  mDfltElements[eHTMLTag_area].mContainsGroups.mBits.mSelf=0;

  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_b],          eHTMLTag_b);
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_base],       eHTMLTag_base,  CHeadElement::GetGroup(), CLeafElement::GetContainedGroups());

  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_basefont],   eHTMLTag_basefont, CSpecialElement::GetGroup(), CLeafElement::GetContainedGroups());

  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_bdo],        eHTMLTag_bdo);
  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_big],        eHTMLTag_big);
  CLeafElement::Initialize(         mDfltElements[eHTMLTag_bgsound],    eHTMLTag_bgsound);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_blockquote], eHTMLTag_blockquote);
  mDfltElements[eHTMLTag_blockquote].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_blockquote].mContainsGroups.mBits.mBlock=1;
  mDfltElements[eHTMLTag_blockquote].mIncludeKids=kBlockQuoteKids;

  //CBodyElement::Initialize(       mDfltElements[eHTMLTag_body],       eHTMLTag_body);
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_br],         eHTMLTag_br,  CSpecialElement::GetGroup(), CLeafElement::GetContainedGroups());

  CElement::Initialize(             mDfltElements[eHTMLTag_button],     eHTMLTag_button,  CFormControlElement::GetGroup(), CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_button].mExcludeKids=kButtonExcludeKids;
  

  CElement::Initialize(             mDfltElements[eHTMLTag_caption],    eHTMLTag_caption, CTableElement::GetGroup(), CSpecialElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_center],     eHTMLTag_center, CBlockElement::GetGroup(), CFlowElement::GetContainedGroups());

  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_cite],       eHTMLTag_cite);
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_code],       eHTMLTag_code);
  CElement::Initialize(             mDfltElements[eHTMLTag_col],        eHTMLTag_col, CTableElement::GetGroup(), CLeafElement::GetContainedGroups());
  CTableElement::Initialize(        mDfltElements[eHTMLTag_colgroup],   eHTMLTag_colgroup);
  mDfltElements[eHTMLTag_colgroup].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_colgroup].mIncludeKids=kColgroupKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_dd],         eHTMLTag_dd,  CListElement::GetGroup(),   CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_dd].mAutoClose=kAutoCloseDD;
  mDfltElements[eHTMLTag_dd].mContainsGroups.mBits.mSelf=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_del],        eHTMLTag_del, CPhraseElement::GetGroup(),  CFlowElement::GetContainedGroups());

  CElement::Initialize(             mDfltElements[eHTMLTag_dfn],        eHTMLTag_dfn, CPhraseElement::GetGroup(), CInlineElement::GetContainedGroups());
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_dir],        eHTMLTag_dir);
  mDfltElements[eHTMLTag_dir].mGroup.mBits.mList=1;
  mDfltElements[eHTMLTag_dir].mIncludeKids=kDirKids;
  mDfltElements[eHTMLTag_dir].mContainsGroups.mAllBits=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_div],        eHTMLTag_div, CBlockElement::GetGroup(),  CFlowElement::GetContainedGroups());
 
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_dl],         eHTMLTag_dl);  
  mDfltElements[eHTMLTag_dl].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_dl].mIncludeKids=kDLKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_dt],         eHTMLTag_dt, CListElement::GetGroup(), CInlineElement::GetContainedGroups());
  mDfltElements[eHTMLTag_dt].mContainsGroups.mBits.mLeaf=1;
  mDfltElements[eHTMLTag_dt].mAutoClose=kAutoCloseDD;
  
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_em],         eHTMLTag_em);
  CElement::Initialize(   mDfltElements[eHTMLTag_embed],      eHTMLTag_embed);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_endnote],    eHTMLTag_endnote);

  CElement::Initialize(             mDfltElements[eHTMLTag_fieldset],   eHTMLTag_fieldset, CBlockElement::GetGroup(),  CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_fieldset].mIncludeKids=kFieldsetKids;

  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_font],       eHTMLTag_font);
  CElement::Initialize(             mDfltElements[eHTMLTag_form],       eHTMLTag_form, CFormControlElement::GetGroup(), CBlockElement::GetContainedGroups());
  mDfltElements[eHTMLTag_form].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_form].mContainsGroups.mBits.mBlock=1;
  mDfltElements[eHTMLTag_form].mIncludeKids=kFormKids;
  mDfltElements[eHTMLTag_form].mExcludeKids=kFormExcludeKids;

  CFrameElement::Initialize(        mDfltElements[eHTMLTag_frame],      eHTMLTag_frame);
  CFrameElement::Initialize(        mDfltElements[eHTMLTag_frameset],   eHTMLTag_frameset);
  mDfltElements[eHTMLTag_frameset].mIncludeKids=kFramesetKids;
  
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_h1],         eHTMLTag_h1);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_h2],         eHTMLTag_h2);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_h3],         eHTMLTag_h3);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_h4],         eHTMLTag_h4);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_h5],         eHTMLTag_h5);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_h6],         eHTMLTag_h6);
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_hr],         eHTMLTag_hr,        CBlockElement::GetGroup(), CLeafElement::GetContainedGroups());


  CElement::Initialize(             mDfltElements[eHTMLTag_head],       eHTMLTag_head,      CHeadElement::GetGroup(), CHeadElement::GetContainedGroups());
  // InitializeElement(             mDfltElements[eHTMLTag_head],       eHTMLTag_html,      CTopLevelElement::GetGroup(), CTopLevelElement::GetContainedGroups());

  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_i],          eHTMLTag_i);
  CElement::Initialize(             mDfltElements[eHTMLTag_iframe],     eHTMLTag_iframe,    CSpecialElement::GetGroup(),  CFlowElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_ilayer],     eHTMLTag_ilayer);
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_img],        eHTMLTag_img,       CSpecialElement::GetGroup(),  CLeafElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_image],      eHTMLTag_image);
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_input],      eHTMLTag_input,     CFormControlElement::GetGroup(),CLeafElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_ins],        eHTMLTag_ins,       CPhraseElement::GetGroup(),  CFlowElement::GetContainedGroups());
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_isindex],    eHTMLTag_isindex,   CHeadElement::GetMiscGroup(), CLeafElement::GetContainedGroups());

  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_kbd],        eHTMLTag_kbd);
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_keygen],     eHTMLTag_keygen);

  CElement::Initialize(             mDfltElements[eHTMLTag_label],      eHTMLTag_label,     CFormControlElement::GetGroup(), CInlineElement::GetContainedGroups());
  mDfltElements[eHTMLTag_label].mContainsGroups.mBits.mSelf=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_layer],      eHTMLTag_layer);
  CElement::Initialize(             mDfltElements[eHTMLTag_legend],     eHTMLTag_legend,CFormControlElement::GetGroup(), CInlineElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_li],         eHTMLTag_li,    CListElement::GetGroup(), CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_li].mExcludeKids=kLIExcludeKids;

  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_link],       eHTMLTag_link,  CHeadElement::GetMiscGroup(), CLeafElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_listing],    eHTMLTag_listing);

  CElement::Initialize(             mDfltElements[eHTMLTag_map],        eHTMLTag_map,   CSpecialElement::GetGroup(), CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_map].mProperties.mIsContainer=1;
  mDfltElements[eHTMLTag_map].mIncludeKids=kMapKids;

  CBlockElement::Initialize(        mDfltElements[eHTMLTag_menu],       eHTMLTag_menu);
  mDfltElements[eHTMLTag_menu].mGroup.mBits.mList=1;
  mDfltElements[eHTMLTag_menu].mIncludeKids=kDirKids;
  mDfltElements[eHTMLTag_menu].mContainsGroups.mAllBits=0;

  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_meta],       eHTMLTag_meta,  CHeadElement::GetMiscGroup(), CLeafElement::GetContainedGroups());

  CElement::Initialize(             mDfltElements[eHTMLTag_multicol],   eHTMLTag_multicol);
  CElement::Initialize(             mDfltElements[eHTMLTag_nobr],       eHTMLTag_nobr);
  CElement::Initialize(             mDfltElements[eHTMLTag_noembed],    eHTMLTag_noembed);
    
  CElement::Initialize(             mDfltElements[eHTMLTag_noframes],   eHTMLTag_noframes,  CBlockElement::GetGroup(),  CFlowElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_nolayer],    eHTMLTag_nolayer);
  CElement::Initialize(             mDfltElements[eHTMLTag_noscript],   eHTMLTag_noscript,  CBlockElement::GetGroup(),  CFlowElement::GetContainedGroups());

  CElement::Initialize(             mDfltElements[eHTMLTag_object],     eHTMLTag_object,    CBlockElement::GetGroup(),  CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_object].mGroup.mBits.mHeadMisc=1;
  mDfltElements[eHTMLTag_object].mIncludeKids=kObjectKids;

  CBlockElement::Initialize(        mDfltElements[eHTMLTag_ol],         eHTMLTag_ol);
  mDfltElements[eHTMLTag_ol].mGroup.mBits.mList=1;
  mDfltElements[eHTMLTag_ol].mIncludeKids=kDirKids;
  mDfltElements[eHTMLTag_ol].mContainsGroups.mAllBits=0;

  CFormControlElement::Initialize(  mDfltElements[eHTMLTag_optgroup],   eHTMLTag_optgroup);
  mDfltElements[eHTMLTag_optgroup].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_optgroup].mIncludeKids=kOptionGroupKids;

  CFormControlElement::Initialize(  mDfltElements[eHTMLTag_option],     eHTMLTag_option);
  mDfltElements[eHTMLTag_optgroup].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_optgroup].mContainsGroups.mBits.mLeaf=1;

  CElement::Initialize(             mDfltElements[eHTMLTag_p],          eHTMLTag_p, CBlockElement::GetGroup(), CInlineElement::GetContainedGroups());
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_param],      eHTMLTag_param, CSpecialElement::GetGroup(), CLeafElement::GetContainedGroups());
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_parsererror],eHTMLTag_parsererror);
  CElement::Initialize(             mDfltElements[eHTMLTag_plaintext],  eHTMLTag_plaintext);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_pre],        eHTMLTag_pre);
  mDfltElements[eHTMLTag_pre].mExcludeKids=kPreExcludeKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_plaintext],  eHTMLTag_plaintext);
  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_q],          eHTMLTag_q);
  
  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_s],          eHTMLTag_s);
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_samp],       eHTMLTag_samp );
  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_script],     eHTMLTag_script);
  mDfltElements[eHTMLTag_script].mGroup.mBits.mHeadMisc=1;

  CFormControlElement::Initialize(  mDfltElements[eHTMLTag_select],     eHTMLTag_select);
  mDfltElements[eHTMLTag_select].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_select].mIncludeKids=kSelectKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_server],     eHTMLTag_server);
  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_small],      eHTMLTag_small);
  CElement::Initialize(             mDfltElements[eHTMLTag_sourcetext], eHTMLTag_sourcetext);
  CElement::Initialize(             mDfltElements[eHTMLTag_spacer],     eHTMLTag_spacer);
  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_span],       eHTMLTag_span);
  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_strike],     eHTMLTag_strike);
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_strong],     eHTMLTag_strong);
  CHeadElement::Initialize(         mDfltElements[eHTMLTag_style],      eHTMLTag_style);
  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_sub],        eHTMLTag_sub);
  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_sup],        eHTMLTag_sup);

  CElement::Initialize(             mDfltElements[eHTMLTag_table],      eHTMLTag_table,  CBlockElement::GetGroup(), CTableElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_tbody],      eHTMLTag_tbody);
  CElement::Initialize(             mDfltElements[eHTMLTag_td],         eHTMLTag_td, CTableElement::GetGroup(), CFlowElement::GetContainedGroups());

  CFormControlElement::Initialize(  mDfltElements[eHTMLTag_textarea],   eHTMLTag_textarea);
  mDfltElements[eHTMLTag_textarea].mGroup.mBits.mLeaf=1;
  mDfltElements[eHTMLTag_textarea].mGroup.mBits.mWhiteSpace=1;

  CTableElement::Initialize(        mDfltElements[eHTMLTag_tfoot],      eHTMLTag_tfoot);
  CElement::Initialize(             mDfltElements[eHTMLTag_th],         eHTMLTag_th, CTableElement::GetGroup(), CFlowElement::GetContainedGroups());
  CTableElement::Initialize(        mDfltElements[eHTMLTag_thead],      eHTMLTag_thead);
  CTableElement::Initialize(        mDfltElements[eHTMLTag_tr],         eHTMLTag_tr);
  CElement::Initialize(             mDfltElements[eHTMLTag_title],      eHTMLTag_title);

  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_tt],         eHTMLTag_tt);
  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_u],          eHTMLTag_u);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_ul],         eHTMLTag_ul);
  mDfltElements[eHTMLTag_ul].mGroup.mBits.mList=1;
  mDfltElements[eHTMLTag_ul].mIncludeKids=kDirKids;
  mDfltElements[eHTMLTag_ul].mContainsGroups.mAllBits=0;
  
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_var],        eHTMLTag_var);
  CElement::Initialize(             mDfltElements[eHTMLTag_wbr],        eHTMLTag_wbr);
  CElement::Initialize(             mDfltElements[eHTMLTag_xmp],        eHTMLTag_xmp);

  CLeafElement::Initialize(         mDfltElements[eHTMLTag_text],      eHTMLTag_text);
  CLeafElement::Initialize(         mDfltElements[eHTMLTag_comment],   eHTMLTag_comment);
  CLeafElement::Initialize(         mDfltElements[eHTMLTag_newline],   eHTMLTag_newline);
  CLeafElement::Initialize(         mDfltElements[eHTMLTag_whitespace],eHTMLTag_whitespace);
  CLeafElement::Initialize(         mDfltElements[eHTMLTag_unknown],   eHTMLTag_unknown);


  /************************************************************
     Now let's initialize the elements that we created directly 
     to handle special cases.
   ************************************************************/

  mElements[eHTMLTag_body]=&mBodyElement;
  mElements[eHTMLTag_frameset]=&mFramesetElement;
  mElements[eHTMLTag_html]=&mHTMLElement;
  mElements[eHTMLTag_head]=&mHeadElement;
  mElements[eHTMLTag_script]=&mScriptElement;
  mElements[eHTMLTag_style]=&mStyleElement;
  mElements[eHTMLTag_title]=&mTitleElement;
  
}

void CElementTable::DebugDumpGroups(CElement* aTag){

  const char* tag=nsHTMLTags::GetStringValue(aTag->mTag);
  const char* prefix="     ";
  printf("\n\nTag: <%s>\n",tag);
  printf(prefix);

  if(aTag->IsContainer()) {
    if(aTag->mContainsGroups.mBits.mHead)           printf("head ");    
    if(aTag->mContainsGroups.mBits.mHeadMisc)       printf("headmisc ");  
    if(aTag->mContainsGroups.mBits.mHeadContent)    printf("headcontent ");  
    if(aTag->mContainsGroups.mBits.mTable)          printf("table ");
    if(aTag->mContainsGroups.mBits.mTextContainer)  printf("text ");
    if(aTag->mContainsGroups.mBits.mTopLevel)       printf("toplevel ");
    if(aTag->mContainsGroups.mBits.mDTDInternal)    printf("internal ");

    if(aTag->mContainsGroups.mBits.mFlowEntity) {
      printf("block inline ");
    }
    else {
      PRBool done=PR_FALSE;

      if (aTag->mContainsGroups.mBits.mBlockEntity)  {
        printf("blockEntity ");
      }

      if (aTag->mContainsGroups.mBits.mBlock)  {
        printf("block ");
      }

      if(aTag->mContainsGroups.mBits.mInlineEntity)   {
        printf("inline ");
      }

      else {

        if(aTag->mContainsGroups.mBits.mFontStyle )     printf("fontstyle ");
        if(aTag->mContainsGroups.mBits.mPhrase)         printf("phrase ");
        if(aTag->mContainsGroups.mBits.mSpecial)        printf("special ");
        if(aTag->mContainsGroups.mBits.mFormControl)    printf("form ");
        if(aTag->mContainsGroups.mBits.mHeading)        printf("heading ");  
        if(aTag->mContainsGroups.mBits.mFrame)          printf("frame ");
        if(aTag->mContainsGroups.mBits.mList)           printf("list ");
        if(aTag->mContainsGroups.mBits.mPreformatted)   printf("pre ");
        if(aTag->mContainsGroups.mBits.mSelf)           printf("self ");
        if(aTag->mContainsGroups.mBits.mLeaf)           printf("leaf ");
        if(aTag->mContainsGroups.mBits.mWhiteSpace)     printf("ws ");
        if(aTag->mContainsGroups.mBits.mComment)        printf("comment ");
      }
    
    }

    if(aTag->mIncludeKids) {
      printf("\n%s",prefix);
      eHTMLTags *theKid=aTag->mIncludeKids;
      printf("+ ");
      while(eHTMLTag_unknown!=*theKid){
        tag=nsHTMLTags::GetCStringValue(*theKid++);
        printf("%s ",tag);
      }
    }

    if(aTag->mExcludeKids) {
      printf("\n%s",prefix);
      eHTMLTags *theKid=aTag->mExcludeKids;
      printf("- ");
      while(eHTMLTag_unknown!=*theKid){
        tag=nsHTMLTags::GetCStringValue(*theKid++);
        printf("%s ",tag);
      }
    }

    if(!aTag->mContainsGroups.mBits.mSelf){
      printf("\n%s - self",prefix);
    }
 
  }
  else {
    printf("not container\n");
  }
}

void CElementTable::DebugDumpContainment(CElement* anElement){
  const char* tag=nsHTMLTags::GetStringValue(anElement->mTag);
  const char* prefix="     ";
  printf("\n\nTag: <%s>\n",tag);
  printf(prefix);

  int count=0;
  for(int i=0;i<eHTMLTag_unknown;i++){
    CElement* theChild=mElements[i];
    if(anElement->CanContain(theChild,0)){
      tag=nsHTMLTags::GetCStringValue(theChild->mTag);
      printf("%s ",tag);
      count++;
      if(10==count) {
        count=0;
        printf("\n%s",prefix);
      }
    }
  }
}

void CElementTable::DebugDumpContainment(const char* aTitle){

  DebugDumpGroups(mElements[eHTMLTag_blockquote]);
  DebugDumpGroups(mElements[eHTMLTag_button]);
  DebugDumpGroups(mElements[eHTMLTag_dfn]);
  DebugDumpGroups(mElements[eHTMLTag_dt]);
  DebugDumpGroups(mElements[eHTMLTag_form]);
  DebugDumpGroups(mElements[eHTMLTag_frameset]);
  DebugDumpGroups(mElements[eHTMLTag_label]);
  DebugDumpGroups(mElements[eHTMLTag_li]);
  DebugDumpGroups(mElements[eHTMLTag_map]);
  DebugDumpGroups(mElements[eHTMLTag_object]);

  printf("==================================================\n");
  printf("%s\n",aTitle);
  printf("==================================================\n");
  int i,j=0;

  for(i=1;i<eHTMLTag_text;i++){
    //DebugDumpContainment(mElements[i]);
    DebugDumpGroups(mElements[i]);
  } //for
}

/******************************************************************************
  Yes, I know it's inconvenient to find this methods here, but it's easier
  for the compiler -- and making it's life easier is my top priority.
 ******************************************************************************/
PRBool CElement::CanBeClosedByStartTag(CElement* anElement,nsDTDContext* aContext) {
  PRBool result=PR_FALSE;

    //first, let's see if we can contain the given tag based on group info...
  if(anElement) {
    if(ListContainsTag(mAutoClose,anElement->mTag)) {
      return PR_TRUE;
    }
    else if((this==anElement) && (!mContainsGroups.mBits.mSelf)){
      return PR_TRUE;
    }
    else {
      eHTMLTags theTag=aContext->Last();
      CElement* theElement=gElementTable->mElements[theTag];
      if(HasOptionalEndTag(theElement)) {
        if(anElement->CanContain(theElement,aContext)){
          result=PR_TRUE;
        }
      }
    }
  }
  return result;
}

/******************************************************************************
  Yes, I know it's inconvenient to find this methods here, but it's easier
  for the compiler -- and making it's life easier is my top priority.
 ******************************************************************************/
PRBool CElement::CanBeClosedByEndTag(CElement* anElement,nsDTDContext* aContext) {
  PRBool result=PR_FALSE;

    //first, let's see if we can contain the given tag based on group info...
  if(anElement) {
    if(ListContainsTag(mAutoClose,anElement->mTag)) {
      return PR_TRUE;
    }
    else if((this==anElement) && (!mContainsGroups.mBits.mSelf)){
      return PR_TRUE;
    }
    else {
      eHTMLTags theTag=aContext->Last();
      CElement* theElement=gElementTable->mElements[theTag];
      if(HasOptionalEndTag(theElement)) {
        if(anElement->CanContain(theElement,aContext)){
          result=PR_TRUE;
        }
      }
    }
  }
  return result;
}

/******************************************************************************
  Yes, I know it's inconvenient to find this methods here, but it's easier
  for the compiler -- and making it's life easier is my top priority.
 ******************************************************************************/
PRBool CElement::CanContain(CElement* anElement,nsDTDContext* aContext) {
  PRBool result=PR_FALSE;

    //first, let's see if we can contain the given tag based on group info...
  if(anElement) {
    if(anElement!=this) {
      if(ListContainsTag(mExcludeKids,anElement->mTag)) {
        return PR_FALSE;
      }
      else if(ContainsGroup(mContainsGroups,anElement->mGroup)) {
        result=PR_TRUE;
      }
      else if(ListContainsTag(mIncludeKids,anElement->mTag)) {
        return PR_TRUE;
      }
    }
    else result=mContainsGroups.mBits.mSelf;
  }
  return result;
}

nsresult CElement::HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
  nsresult result=NS_OK;

  CElement* theElement=gElementTable->mElements[aTag];

#if 0
  CElement* theDelegate=theElement->GetDelegate();
  if(theDelegate) {
    result=theDelegate->HandleStartToken(aNode,aTag,aContext,aSink);
  }
  else 
#endif
  {
    if(theElement) {
      if(CanContain(theElement,aContext)) {
    
        if(theElement->IsContainer()) {
          aContext->Push(aNode);
          result=aSink->OpenContainer(*aNode);
        }
        else {
          result=aSink->AddLeaf(*aNode); 
        }
      }
      else if(theElement->IsContainer()){

        //Ok, so we have a start token that is misplaced. Before handing this off
        //to a default container (parent), let's check the autoclose condition.
        if(ListContainsTag(theElement->mAutoClose,theElement->mTag)) {
          if(HasOptionalEndTag(theElement)) {
            //aha! We have a case where this tag is autoclosed by anElement.
            //Let's close this container, then try to open theElement.
            eHTMLTags theParentTag=aContext->Last();
            CElement* theParent=gElementTable->mElements[theParentTag];
            while(NS_SUCCEEDED(result) && theParent->CanBeClosedByStartTag(this,aContext)) {
              result=theParent->HandleEndToken(aNode,theParentTag,aContext,aSink);
              theParentTag=aContext->Last();
              theParent=gElementTable->mElements[theParentTag];
            }

            if(NS_SUCCEEDED(result)){
              theParentTag=aContext->Last();
              CElement* theParent=gElementTable->mElements[theParentTag];
              return theParent->HandleStartToken(aNode,aTag,aContext,aSink);
            }
            else return result;
          }
        }
        else {
          
          PRBool theElementCanOpen=PR_FALSE;

            //the logic here is simple:
            //  This operation can only succeed if the given tag is open, AND
            //  all the tags below it have optional end tags.
            //  If these conditions aren't met, we bail out, leaving the tag open.

          PRInt32 theLastPos=aContext->LastOf(aTag); //see if it's already open...
          if(-1<theLastPos) {
            PRInt32 theCount=aContext->GetCount();
            result=HandleEndToken(aNode,mTag,aContext,aSink);
            theElementCanOpen=PRBool(aContext->GetCount()<theCount);
          }

          if(theElementCanOpen) {
            if(NS_SUCCEEDED(result)){
              eHTMLTags theParentTag=aContext->Last();
              CElement* theParent=gElementTable->mElements[theParentTag];
              return theParent->HandleStartToken(aNode,aTag,aContext,aSink);
            }            
          }
        }

          //ok, here's our last recourse -- let's let the parent handle it.
        CElement* theContainer=GetDefaultContainerFor(theElement);
        if(theContainer) {
          result=theContainer->HandleMisplacedStartToken(aNode,aTag,aContext,aSink);
        }
      }
    }
  }
  return result;
}

static nsresult CloseContainer(eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

  nsEntryStack* aChildStyleStack=0;
  nsIParserNode* theNode=aContext->Pop(aChildStyleStack);

  nsresult result=NS_OK;
  switch(aTag) {
    case eHTMLTag_body:
      result=aSink->CloseBody(*theNode);
      break;

    case eHTMLTag_frameset:
      result=aSink->CloseFrameset(*theNode); 
      break;

    default:
      result=aSink->CloseContainer(*theNode);
      break;
  }//switch
  return result;
}


nsresult CElement::HandleEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
  nsresult result=NS_OK;

  CElement* theCloser=gElementTable->mElements[aTag];

  if(mTag==aTag) {
    result=CloseContainer(aTag,aContext,aSink);
  }
  else {

    PRInt32 theCount=aContext->GetCount();
    PRInt32 theIndex=0;
    PRInt32 theLastOptionalTag=-1;

    for(theIndex=theCount-1;theIndex>=0;theIndex--) {
      eHTMLTags theTag=aContext->TagAt(theIndex);
      CElement* theElement=gElementTable->mElements[theTag];
      if(HasOptionalEndTag(theElement)){
        theLastOptionalTag=theIndex;
      }
      else break;
    }

    if(-1<theLastOptionalTag) {
      PRInt32 theLastInstance=aContext->LastOf(aTag);
      if(-1<theLastInstance) {
        if(theLastOptionalTag-1==theLastInstance) {          
          for(theIndex=theCount-1;theIndex>=theLastInstance;theIndex--){
            CloseContainer(aTag,aContext,aSink);
          }
        }
      }
      //there's nothing to do...
    }
    //there's nothing to do...
  }

  return result;
}


inline CElement* CElement::GetDelegate(void) {
  if(eHTMLTag_unknown!=mDelegate) {
    return gElementTable->mElements[mDelegate];
  }
  return 0;
}


inline CElement* CElement::GetDefaultContainerFor(CElement* anElement) {
  CElement* result=0;

  if(anElement) {
    if(anElement->mGroup.mBits.mBlock) {
      result=gElementTable->mElements[eHTMLTag_body];
    }
    else if(anElement->mGroup.mBits.mHeadContent) {
      result=gElementTable->mElements[eHTMLTag_head];
    }
    else if(anElement->mGroup.mBits.mHeadMisc) {
      result=gElementTable->mElements[eHTMLTag_head];
    }
  }
  return result;
}

nsresult CHTMLElement::AddHeadElement(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
  nsresult result=NS_OK;

  CElement* theElement=gElementTable->mElements[eHTMLTag_head];
  if(theElement) {
    result=theElement->HandleStartToken(aNode,aTag,aContext,aSink);
  }

  return result;
}



#endif
