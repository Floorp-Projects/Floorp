/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   rickg@netscape.com
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

#include "nsDTDUtils.h"

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
    result=(aGroupSet.mAllBits & aGroup.mAllBits) ? PR_TRUE : PR_FALSE;
  }
  return result;
}

inline PRBool ListContainsTag(const eHTMLTags* aTagList,eHTMLTags aTag) {
  if(aTagList) {
    const eHTMLTags *theNextTag=aTagList;
    while(eHTMLTag_unknown!=*theNextTag) {
      if(aTag==*theNextTag) {
        return PR_TRUE;
      }
      ++theNextTag;
    }
  }
  return PR_FALSE;
}


/**********************************************************
  Begin with the baseclass for all elements...
 **********************************************************/
class CElement {
public:

    //break this struct out separately so that lame compilers don't gack.
  struct CFlags {
    PRUint32  mOmitEndTag:1;
    PRUint32  mIsContainer:1;
    PRUint32  mIsSinkContainer:1;
    PRUint32  mDeprecated:1;
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

  static CGroupMembers& GetEmptyGroup(void) {
    static CGroupMembers theGroup={0};
    return theGroup;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    anElement.mProperties.mIsContainer=0;
    anElement.mProperties.mIsSinkContainer=0;
    anElement.mTag=aTag;
    anElement.mGroup.mAllBits=0;;
    anElement.mContainsGroups.mAllBits=0;
  }

  static void InitializeLeaf(CElement& anElement,eHTMLTags aTag,CGroupMembers& aGroup,CGroupMembers& aContainsGroups) {
    anElement.mProperties.mIsContainer=PR_FALSE;
    anElement.mProperties.mIsSinkContainer=PR_FALSE;
    anElement.mTag=aTag;
    anElement.mGroup.mAllBits=aGroup.mAllBits;
    anElement.mContainsGroups.mAllBits=aContainsGroups.mAllBits;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag,CGroupMembers& aGroup,CGroupMembers& aContainsGroups) {
    anElement.mProperties.mIsContainer=PR_TRUE;
    anElement.mProperties.mIsSinkContainer=PR_TRUE;
    anElement.mTag=aTag;
    anElement.mGroup.mAllBits=aGroup.mAllBits;
    anElement.mContainsGroups.mAllBits=aContainsGroups.mAllBits;
  }

  inline CElement*  GetDelegate(void);
  inline CElement*  GetDefaultContainerFor(CElement* anElement);

  virtual PRBool    CanContain(CElement* anElement,nsDTDContext* aContext);
  virtual PRInt32   FindAutoCloseIndexForStartTag(CElement* anElement,PRInt32 aParentIndex,nsDTDContext* aContext);
  virtual PRBool    CanBeClosedByEndTag(CElement* anElement,nsDTDContext* aContext);

    //This tells us whether this tag can potentially close other blocks.
    //That DOES NOT mean that this tag is necessarily a block itself (condsider TBODY,TR,TD...)
  virtual PRBool    IsBlockCloser(void) {
    PRBool result=IsBlockElement(eHTMLTag_body);
    if(!result) {
      if(IsInlineElement(eHTMLTag_body) ||
         mGroup.mBits.mHead || 
         mGroup.mBits.mHeadMisc || 
         mGroup.mBits.mFormControl || 
         mGroup.mBits.mFrame || 
         mGroup.mBits.mLeaf || 
         mGroup.mBits.mComment || 
         mGroup.mBits.mTextContainer || 
         mGroup.mBits.mWhiteSpace)
         result=PR_FALSE;
      else result=PR_TRUE;
    }
    return result;
  }

    //this tells us whether this tag is a block tag within the given parent
  virtual PRBool    IsBlockElement(eHTMLTags aParentID);

  //this tells us whether this tag is an inline tag within the given parent
  //NOTE: aParentID is currently ignored, but shouldn't be.
  virtual PRBool    IsInlineElement(eHTMLTags aParentID);

    //this tells us whether the tag is a container as defined by HTML
    //NOTE: aParentID is currently ignored, but shouldn't be.
  virtual PRBool    IsContainer(void) {return mProperties.mIsContainer;  }

  //this tells us whether the tag should be opened as a container in the sink (script doesn't, for example).
  virtual PRBool    IsSinkContainer(void) { return mProperties.mIsSinkContainer; }

  virtual eHTMLTags GetSkipTarget(void)   {return eHTMLTag_unknown;}
  

  virtual nsresult  WillHandleStartToken( CElement* anElement,
                                          nsIParserNode* aNode,
                                          eHTMLTags aTag,
                                          nsDTDContext* aContext,
                                          nsIHTMLContentSink* aSink);

  virtual nsresult  HandleStartToken(     nsCParserNode* aNode,
                                          eHTMLTags aTag,
                                          nsDTDContext* aContext,
                                          nsIHTMLContentSink* aSink);

  virtual nsresult  HandleEndToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink);

  virtual nsresult  HandleMisplacedStartToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    return result;
  }

  virtual PRInt32 FindAutoCloseTargetForEndTag(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink, PRInt32& anIndex) {
    PRInt32 result=-1;

    if(mTag!=aTag) {
      if(HasOptionalEndTag(mTag) && (0<anIndex)) {
        eHTMLTags theGrandParentTag=aContext->TagAt(--anIndex);
        CElement *theGrandParent=GetElement(theGrandParentTag);
        if(theGrandParent) {
          result=theGrandParent->FindAutoCloseTargetForEndTag(aNode,aTag,aContext,aSink,anIndex); //give the parent a chance...
        }
      }
    }
    else result=anIndex;

    return result;
  }

  virtual nsresult  HandleMisplacedEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    return result;
  }

  nsresult  AutoGenerateStructure(eHTMLTags *aTagList,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult  result=NS_OK;

    CStartToken theToken(*aTagList);
    nsCParserNode theNode(&theToken, 0 /*stack token*/);
    result=OpenContainer(&theNode,*aTagList,aContext,aSink);
    if(eHTMLTag_unknown!=*(aTagList+1)) {
      AutoGenerateStructure(++aTagList,aContext,aSink);
    }
    
    CEndToken theEndToken(*aTagList--);
    nsCParserNode theEndNode(&theEndToken, 0 /*stack token*/);
    result=CloseContainer(&theEndNode,*aTagList,aContext,aSink);

    return result;
  }
        

  /**********************************************************
    Call this for each element as it get's opened on the stack
   **********************************************************/
  virtual nsresult  NotifyOpen(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    return NS_OK;
  }

  /**********************************************************
    Call this for each element as it get's closed
   **********************************************************/
  virtual nsresult  NotifyClose(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    return NS_OK;
  }

  /**********************************************************
   this gets called after each tag is opened in the given context
   **********************************************************/
  virtual nsresult  OpenContainer(nsIParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {
    return aSink->OpenContainer(*aNode);
  }

  /**********************************************************
    this gets called after each tag is opened in the given context
   **********************************************************/
  virtual nsresult  OpenContext(nsCParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {
    aContext->Push(aNode, 0, PR_FALSE);
    CElement *theElement = (aTag == mTag) ? this : GetElement(aTag);
    theElement->NotifyOpen(aNode, aTag, aContext,aSink);
    return NS_OK;
  }

  /**********************************************************
    this gets called after each tag is opened in the given context
   **********************************************************/
  virtual nsresult OpenContainerInContext(nsCParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {    
    OpenContext(aNode,aTag,aContext,aSink);
    return OpenContainer(aNode,aTag,aContext,aSink);
  }

  /**********************************************************
    this gets called to close a given tag in the sink
   **********************************************************/
  virtual nsresult  CloseContainer(nsIParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {
    return aSink->CloseContainer(aTag);
  }

  /**********************************************************
    this gets called to close a tag in the given context
   **********************************************************/
  virtual nsresult  CloseContext(nsCParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {
    nsresult result=NS_OK;
    nsEntryStack  *theStack=0;
    nsCParserNode *theNode=aContext->Pop(theStack);

    CElement *theElement=(aTag==mTag) ? this : GetElement(aTag);
    result=theElement->NotifyClose(theNode,aTag,aContext,aSink);

    IF_FREE(aNode, aContext->mNodeAllocator);
    return result;
  }

  /**********************************************************
    this gets called to close a tag in the sink and in the context
   **********************************************************/
  virtual nsresult CloseContainerInContext(nsCParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {    
    nsresult result=NS_OK;
    if(mTag!=aTag) {
      CElement *theElement=GetElement(aTag);
      return theElement->CloseContainerInContext(aNode,aTag,aContext,aSink);
    }
    result=CloseContainer(aNode,aTag,aContext,aSink);
    CloseContext(aNode,aTag,aContext,aSink);
    return result;
  }


  CElement* GetElement(eHTMLTags aTag);

  eHTMLTags       mTag;
  eHTMLTags       mDelegate;
  CGroupMembers   mGroup;
  CGroupMembers   mContainsGroups;
  const eHTMLTags       *mIncludeKids;
  const eHTMLTags       *mExcludeKids;
  const eHTMLTags       *mAutoClose;    //other start tags that close this container
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

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag);
    anElement.mProperties.mDeprecated=1;
  }

  CDeprecatedElement(eHTMLTags aTag) : CElement(aTag) {
    CDeprecatedElement::Initialize(*this,aTag);
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
      theGroup.mBits.mList=0;  //intentionally remove list from inline group
      theGroup.mBits.mPreformatted=0;
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

  /**********************************************************
    by default,members of the block group contain inline children
   **********************************************************/
  static CGroupMembers& GetContainedGroups(PRBool aCanContainSelf = PR_TRUE) {
    static CGroupMembers theGroups=CInlineElement::GetContainedGroups();
    theGroups.mBits.mSelf=aCanContainSelf;
    return theGroups;
  }

  /**********************************************************
    call this if you want a group that contains only block elements...
   **********************************************************/
  static CGroupMembers& GetBlockGroupMembers(void) {
    static CGroupMembers theGroups={0};
    theGroups.mBits.mBlock=1;
    theGroups.mBits.mSelf=1;
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
  This defines the form element itself
 **********************************************************/
class CFormElement: public CBlockElement {
public:

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,CBlockElement::GetGroup(),CBlockElement::GetBlockGroupMembers());
  }

  CFormElement() : CBlockElement(eHTMLTag_form) {
    CFormElement::Initialize(*this,eHTMLTag_form);
    mContainsGroups.mBits.mSelf=0;
    mContainsGroups.mBits.mFormControl=1;
  }

  virtual PRBool CanContain(CElement* anElement,nsDTDContext* aContext) {
    PRBool result=CElement::CanContain(anElement,aContext);
    if((!result) && (aContext->mFlags.mTransitional)) {

      //If we're in transitional mode, then also allow inline elements...
        
      CGroupMembers& theFlowGroup=CFlowElement::GetContainedGroups();
      result=ContainsGroup(theFlowGroup,anElement->mGroup);            
    }
    return result;
  }

  /**********************************************************
   this gets called after each tag is opened in the given context
   **********************************************************/
  virtual nsresult  OpenContainer(nsIParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {
    nsresult result=aSink->OpenForm(*aNode);
    return result;
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
  This defines the Table block itself, not it's children.
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

  CTableElement(eHTMLTags aTag=eHTMLTag_table) : CElement(aTag) {
    CElement::Initialize(*this,aTag,CBlockElement::GetGroup(),CTableElement::GetContainedGroups());
  }

  PRBool CanContain(CElement* anElement,nsDTDContext* aContext) {
    PRBool result=PR_FALSE;

    switch(anElement->mTag) {

      case eHTMLTag_caption:
        result=(aContext->mTableStates && aContext->mTableStates->CanOpenCaption()); 
        break;

      case eHTMLTag_colgroup:
        result=(aContext->mTableStates && aContext->mTableStates->CanOpenCols());
        break;

      case eHTMLTag_thead:      //nothing to do for these empty tags...      
        result=(aContext->mTableStates && aContext->mTableStates->CanOpenTHead());
        break;

      case eHTMLTag_tfoot:
        result=(aContext->mTableStates && aContext->mTableStates->CanOpenTFoot());
        break;

      case eHTMLTag_tr:
      case eHTMLTag_th:
        result=(aContext->mTableStates && aContext->mTableStates->CanOpenTBody());
        break;

      default:
        result=CElement::CanContain(anElement,aContext);
        break;
    }
    return result; 
  }

  /**********************************************************
    Table needs to be notified so it can manage table states.
   **********************************************************/
  virtual nsresult  NotifyOpen(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    aContext->mTableStates=new CTableState(aContext->mTableStates); //create and prepend a new state
    return NS_OK;
  }

  /**********************************************************
    Table needs to be notified so it can manage table states.
   **********************************************************/
  virtual nsresult  NotifyClose(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

    nsresult result=NS_OK;
    if(aContext->mTableStates) {

      if(!aContext->mTableStates->mHasTBody) {
        //so let's open a tbody, a TR and a TD for good measure...

        eHTMLTags theTags[]={eHTMLTag_tbody,eHTMLTag_tr,eHTMLTag_td,eHTMLTag_unknown};
        AutoGenerateStructure(theTags,aContext,aSink);
      }
      
      //pop the current state and restore it's predecessor, if any...
      CTableState *theState=aContext->mTableStates;      
      aContext->mTableStates=theState->mPrevious; 
      delete theState;
    }
    return result;
  }
 
  /**********************************************************
    Table handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleStartToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {

      case eHTMLTag_caption:
        if(aContext->mTableStates && aContext->mTableStates->CanOpenCaption()) {
          result=OpenContainerInContext(aNode,aTag,aContext,aSink);  //force the title onto the stack
        }
        break;

      case eHTMLTag_col:
        result=aSink->AddLeaf(*aNode); 
        break;

      case eHTMLTag_colgroup:
        if(aContext->mTableStates && aContext->mTableStates->CanOpenCols()) {
          result=OpenContainerInContext(aNode,aTag,aContext,aSink);  //force the title onto the stack
        }
        break;

      case eHTMLTag_thead:      //nothing to do for these empty tags...      
        if(aContext->mTableStates && aContext->mTableStates->CanOpenTHead()) {
          aContext->mTableStates->mHasTHead=PR_TRUE;
          result=OpenContainerInContext(aNode,aTag,aContext,aSink);  //force the title onto the stack
        }
        break;

      case eHTMLTag_tbody:
        aContext->mTableStates->mHasTBody=PR_TRUE;
        result=OpenContainerInContext(aNode,aTag,aContext,aSink);  //force the title onto the stack
        break;

      case eHTMLTag_tfoot:
        if(aContext->mTableStates && aContext->mTableStates->CanOpenTFoot()) {
          aContext->mTableStates->mHasTFoot=PR_TRUE;
          result=OpenContainerInContext(aNode,aTag,aContext,aSink);  //force the title onto the stack
        }
        break;

      case eHTMLTag_tr:
      case eHTMLTag_th:
    
        if(aContext->mTableStates) {
          if(aContext->mTableStates->CanOpenTBody()) {
            CToken* theToken=(CStartToken*)aContext->mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_tbody);
            nsCParserNode* theNode=aContext->mNodeAllocator->CreateNode(theToken, 0);

            result=HandleStartToken(theNode,eHTMLTag_tbody,aContext,aSink);
          }
          if(NS_SUCCEEDED(result)) {
            CElement *theElement=GetElement(eHTMLTag_tbody);
            if(theElement) {
              result=theElement->HandleStartToken(aNode,aTag,aContext,aSink);
            }
          }
        }

        break;

      default:
        break;
    }
    return result; 
  }

  /**********************************************************
    Table handles the closing of it's own children
   **********************************************************/
  virtual nsresult HandleEndToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    if(aContext->HasOpenContainer(aTag)) {
      switch(aTag) {
        case eHTMLTag_caption:
        case eHTMLTag_col:
        case eHTMLTag_colgroup:
        case eHTMLTag_tr:
        case eHTMLTag_thead:
        case eHTMLTag_tfoot:
        case eHTMLTag_tbody:      
          result=CloseContainerInContext(aNode,aTag,aContext,aSink);  //force the title onto the stack
          break;

        default:
          break;
      } //switch
    } //if
       

    return result;
  }

  /**********************************************************
    If you're here, then children below you have optional
    end tags, can't deal with the given tag, and want you
    to handle it.
   **********************************************************/
  virtual PRInt32 FindAutoCloseTargetForEndTag(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink, PRInt32& anIndex) {

    // XXXldb This method is completely unused because the |aNode|
    // parameter is declared as |nsIParserNode| rather than
    // |nsCParserNode| so it doesn't override the member function of
    // CElement.
    NS_NOTREACHED("This isn't used.  Should it be?");

    PRInt32 result=kNotFound;

    switch(aTag) {
      case eHTMLTag_table:
      case eHTMLTag_caption:
      case eHTMLTag_col:
      case eHTMLTag_colgroup:
      case eHTMLTag_thead:
      case eHTMLTag_tfoot:
      case eHTMLTag_tbody:      
      case eHTMLTag_tr:      
      case eHTMLTag_td:     
        {
          PRInt32 theTablePos=aContext->LastOf(eHTMLTag_table);
          PRInt32 theTagPos=aContext->LastOf(aTag);
          if((kNotFound!=theTagPos) && (theTablePos<=theTagPos)) {
            result=theTagPos;
          }
        }
        break;

      default:
        break;
    } //switch

    return result;
  }

};

/**********************************************************
  This defines the Table block itself, not it's children.
 **********************************************************/

class CTableRowElement: public CElement {
public:

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,CTableElement::GetGroup(),CElement::GetEmptyGroup());
    
    static eHTMLTags kTRKids[]={eHTMLTag_td,eHTMLTag_th,eHTMLTag_unknown};
    anElement.mIncludeKids=kTRKids;
  }

  CTableRowElement(eHTMLTags aTag=eHTMLTag_tr) : CElement(aTag) {
    CTableRowElement::Initialize(*this,aTag);
    mContainsGroups.mBits.mSelf=0;
  }

  virtual nsresult  HandleEndTokenForChild(CElement *aChild,nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    return result;
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
  This defines the LI element...

  An interesting problem here is that LI normally contains
  Block+inline, unless it's inside a MENU or DIR, in which
  case it contains only inline.
 **********************************************************/
class CLIElement: public CElement {
public:

  CLIElement(eHTMLTags aTag=eHTMLTag_li) : CElement(aTag) {
    CFlowElement::Initialize(*this,aTag);
    mGroup.mAllBits=0;
    mGroup.mBits.mList=1;
  }

};

/**********************************************************
  This defines the counter element, and is for debug use.

    Usage:  <counter name="xxx" reset=n>

    if you leave off the name key/value pair, we'll use the
    name of the element instead.
 **********************************************************/
class CCounterElement: public CInlineElement {
public:

  CCounterElement(eHTMLTags aTag=eHTMLTag_counter) : CInlineElement(aTag) {
    CInlineElement::Initialize(*this,aTag);
    mProperties.mIsSinkContainer=PR_FALSE;
  }

  /**********************************************************
    handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleStartToken(  nsCParserNode* aNode,
                                      eHTMLTags aTag,
                                      nsDTDContext* aContext,
                                      nsIHTMLContentSink* aSink) {
    return CElement::HandleStartToken(aNode,aTag,aContext,aSink);
  }


  /**********************************************************
    this gets called after each tag is opened in the given context
   **********************************************************/
  virtual nsresult OpenContext(nsCParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {    
    CElement::OpenContext(aNode,aTag,aContext,aSink);
    
    nsresult result=NS_OK;
    PRInt32   theCount=aContext->GetCount();

    nsCParserNode *theNode = (nsCParserNode*)aNode;

#ifdef DEBUG
    eHTMLTags theGrandParentTag=aContext->TagAt(theCount-2);
    nsAutoString  theNumber;
    aContext->IncrementCounter(theGrandParentTag,*theNode,theNumber);

    CTextToken theToken(theNumber);
    nsCParserNode theNewNode(&theToken, 0 /*stack token*/);
    *theNode = theNewNode;
#endif
    result=aSink->AddLeaf(*theNode);
    return result;
  }

  /**********************************************************
    handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleEndToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    return CElement::HandleEndToken(aNode,aTag,aContext,aSink);
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


  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mFrame=1;
    return theGroup;
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    anElement.mProperties.mIsContainer=1;
    anElement.mProperties.mIsSinkContainer=1;
    anElement.mTag=aTag;
    anElement.mGroup.mAllBits=0;
    anElement.mGroup.mBits.mFrame=1;
    anElement.mContainsGroups.mAllBits=0;
    anElement.mContainsGroups.mBits.mFrame=1;
    anElement.mContainsGroups.mBits.mSelf=1;
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

    static eHTMLTags kHeadKids[]={eHTMLTag_isindex,eHTMLTag_unknown};

    anElement.mIncludeKids=kHeadKids;
  }

  virtual nsresult OpenContext(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

    // XXXldb This method is completely unused because the |aNode|
    // parameter is declared as |nsIParserNode| rather than
    // |nsCParserNode| so it doesn't override the member function of
    // CElement.
    NS_NOTREACHED("This isn't used.  Should it be?");

    NS_ASSERTION(aContext!=nsnull,"cannot make a decision without a context");

    nsresult result=NS_OK;
    if(aSink && aContext) {
      if(aContext->mFlags.mHasOpenHead==PR_FALSE) {
        result=aSink->OpenHead(*aNode);
        aContext->mFlags.mHasOpenHead=PR_TRUE;
      }
    }
    return result;
  }

  virtual nsresult CloseContext(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

    // XXXldb This method is completely unused because the |aNode|
    // parameter is declared as |nsIParserNode| rather than
    // |nsCParserNode| so it doesn't override the member function of
    // CElement.
    NS_NOTREACHED("This isn't used.  Should it be?");

    NS_ASSERTION(aContext!=nsnull,"cannot make a decision without a context");

    nsresult result=NS_OK;
    if(aSink && aContext) {
      if(aContext->mFlags.mHasOpenHead==PR_TRUE) {
        result = aSink->CloseHead();
        aContext->mFlags.mHasOpenHead=PR_FALSE;
      }
    }
    return result;
  }
      
  CHeadElement(eHTMLTags aTag) : CElement(aTag) {
    CHeadElement::Initialize(*this,aTag);
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

  virtual ~CTextContainer() {
  }

  /**********************************************************
    Call this for each element as it get's closed
   **********************************************************/
  virtual nsresult  NotifyClose(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    if(aNode) {
#if 0
      CStartToken   theToken(aTag);
      nsCParserNode theNode(&theToken);
      theNode.SetSkippedContent(mText);
      result=aSink->AddLeaf(theNode);
#endif
      nsCParserNode *theNode=(nsCParserNode*)aNode;
      //theNode->SetSkippedContent(mText); XXX why do we need this?
      result=aSink->AddLeaf(*theNode);
    }
    mText.Truncate(0);
    return result;
  }

  /**********************************************************
    Textcontainer handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

    // XXXldb This method is completely unused because the |aNode|
    // parameter is declared as |nsIParserNode| rather than
    // |nsCParserNode| so it doesn't override the member function of
    // CElement.
    NS_NOTREACHED("This isn't used.  Should it be?");

    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_text:
      case eHTMLTag_whitespace:
        mText.Append(aNode->GetText());
        break;
      default:
        break;
    }

    return result;
  }

  virtual nsresult HandleEndToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

    // XXXldb This method is completely unused because the |aNode|
    // parameter is declared as |nsIParserNode| rather than
    // |nsCParserNode| so it doesn't override the member function of
    // CElement.
    NS_NOTREACHED("This isn't used.  Should it be?");

    nsresult result=NS_OK;
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

  /**********************************************************
    Call this for each element as it get's closed
   **********************************************************/
  virtual nsresult  NotifyClose(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

    // XXXldb This method is completely unused because the |aNode|
    // parameter is declared as |nsCParserNode| rather than
    // |nsIParserNode| so it doesn't override the member function of
    // CTextContainer.
    NS_NOTREACHED("This isn't used.  Should it be?");

    nsresult result=NS_OK;
    CElement* theHead=GetElement(eHTMLTag_head);
    if(theHead) {
      result=theHead->OpenContext(aNode,aTag,aContext,aSink);
      if(NS_SUCCEEDED(result)) {
        result=aSink->SetTitle(mText);
        mText.Truncate(0);
        if(NS_SUCCEEDED(result)) {
          result=theHead->CloseContext(aNode,aTag,aContext,aSink);
        }
      }
    }
    return result;
  }

  /**********************************************************
    Title handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_text:
        if(aNode && aNode->GetTokenType()==eToken_entity) {
          nsAutoString tmp;
          aNode->TranslateToUnicodeStr(tmp);
          mText.Append(tmp);
          break;
        }
      case eHTMLTag_whitespace:
        mText.Append(aNode->GetText());
        break;
      default:
        break;
    }

    return result;
  }

};

/**********************************************************
  This class is for the title element
 **********************************************************/
class CTextAreaElement: public CTextContainer {
public:

  CTextAreaElement() : CTextContainer(eHTMLTag_textarea) {
    mGroup.mBits.mHeadMisc=1;
    mGroup=CFormControlElement::GetGroup();
    mProperties.mIsSinkContainer=0;
  }

  virtual nsresult HandleStartToken(nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
     case eHTMLTag_text:
       if(aNode && aNode->GetTokenType()==eToken_entity) {
          nsAutoString tmp;
          aNode->TranslateToUnicodeStr(tmp);
          mText.Append(tmp);
          break;
        }
      case eHTMLTag_whitespace:
      case eHTMLTag_newline:
        mText.Append(aNode->GetText());
        break;
      default:
        break;
    }

    return result;
  }

 
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

  /**********************************************************
    Call this for each element as it get's closed
   **********************************************************/
  virtual nsresult  NotifyClose(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

    // XXXldb This method is completely unused because the |aNode|
    // parameter is declared as |nsCParserNode| rather than
    // |nsIParserNode| so it doesn't override the member function of
    // CTextContainer.
    NS_NOTREACHED("This isn't used.  Should it be?");

    nsresult result=NS_OK;
    CElement* theHead=GetElement(eHTMLTag_head);
    if(theHead) {
      result=theHead->OpenContext(aNode,aTag,aContext,aSink);
      if(NS_SUCCEEDED(result)) {
        result=CTextContainer::NotifyClose(aNode,aTag,aContext,aSink);
        mText.Truncate(0);
        if(NS_SUCCEEDED(result)) {
          result=theHead->CloseContext(aNode,aTag,aContext,aSink);
        }
      }
    }
    return result;
  }

};

/**********************************************************
  This class is for use with script
 **********************************************************/
class CScriptElement: public CTextContainer {
public:

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CTextContainer::Initialize(anElement,aTag);
    anElement.mProperties.mIsSinkContainer=PR_FALSE;
  }

  CScriptElement() : CTextContainer(eHTMLTag_script) {
    mGroup.mBits.mHeadMisc=1;
    mGroup.mBits.mInlineEntity=1;
    mGroup.mBits.mSpecial=1;
    mProperties.mIsSinkContainer=PR_FALSE;
  }

  /**********************************************************
    this gets called after each tag is opened in the given context
   **********************************************************/
  virtual nsresult OpenContainerInContext(nsCParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {    
    OpenContext(aNode,aTag,aContext,aSink);
    return NS_OK;
  }

  /**********************************************************
    this gets called to close a tag in the given context
   **********************************************************/
  virtual nsresult  CloseContext(nsIParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {

    // XXXldb This method is completely unused because the |aNode|
    // parameter is declared as |nsIParserNode| rather than
    // |nsCParserNode| so it doesn't override the member function of
    // CElement.
    NS_NOTREACHED("This isn't used.  Should it be?");

    nsEntryStack* theStack=0;
    nsIParserNode *theNode=aContext->Pop(theStack);

    CElement *theElement=(aTag==mTag) ? this : GetElement(aTag);
    theElement->NotifyClose(theNode,aTag,aContext,aSink);

    return NS_OK;
  }

  /**********************************************************
    Call this for each element as it get's closed
   **********************************************************/
  virtual nsresult  NotifyClose(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

    // XXXldb This method is completely unused because the |aNode|
    // parameter is declared as |nsCParserNode| rather than
    // |nsIParserNode| so it doesn't override the member function of
    // CTextContainer.
    NS_NOTREACHED("This isn't used.  Should it be?");

    nsresult result=NS_OK;

    if(aContext->HasOpenContainer(eHTMLTag_body)) {
      //add the script to the body
      result=CTextContainer::NotifyClose(aNode,aTag,aContext,aSink);
    }
    else {
        //add it to the head...
      CElement* theHead=GetElement(eHTMLTag_head);
      if(theHead) {
        result=theHead->OpenContext(aNode,aTag,aContext,aSink);
        if(NS_SUCCEEDED(result)) {
          result=CTextContainer::NotifyClose(aNode,aTag,aContext,aSink);
          if(NS_SUCCEEDED(result)) {
            result=theHead->CloseContext(aNode,aTag,aContext,aSink);
          }
        }
      }
    }
    mText.Truncate(0);
    return result;
  }


};

/**********************************************************
  This defines the preformatted element group, (PRE).
 **********************************************************/
class CPreformattedElement: public CBlockElement {
public:

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CBlockElement::Initialize(anElement,aTag);
  }

  CPreformattedElement(eHTMLTags aTag) : CBlockElement(aTag) {
    mGroup=GetGroup();
    mContainsGroups=GetContainedGroups();
    mProperties.mIsContainer=1;
  }

  /**********************************************************
    Pre handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleStartToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=CElement::HandleStartToken(aNode,aTag,aContext,aSink);
    return result;
  }


  /**********************************************************
    Pre handles the closing of it's own children
   **********************************************************/
  virtual nsresult HandleEndToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=CElement::HandleEndToken(aNode,aTag,aContext,aSink);
    return result;
  }

};

/**********************************************************
  This is used for both applet and object elements
 **********************************************************/
class CAppletElement: public CSpecialElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mSpecial=1;
    theGroup.mBits.mBlock=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    return CFlowElement::GetContainedGroups();
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());

    static eHTMLTags kSpecialKids[]={eHTMLTag_param,eHTMLTag_unknown};
    anElement.mIncludeKids=kSpecialKids;
    anElement.mProperties.mIsContainer=1;
  }

  CAppletElement(eHTMLTags aTag) : CSpecialElement(aTag) {
    Initialize(*this,aTag);
  }

  /**********************************************************
    handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleStartToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    nsIParserNode *theNode=aContext->PeekNode();
    if(theNode) {
      PRBool  theContentsHaveArrived=theNode->GetGenericState();
      switch(aTag) {
        case eHTMLTag_param:
          if(!theContentsHaveArrived) {
            result=CElement::HandleStartToken(aNode,aTag,aContext,aSink);  
          }
          break;
           
        case eHTMLTag_newline:
        case eHTMLTag_whitespace:
          result=CElement::HandleStartToken(aNode,aTag,aContext,aSink);  
          break;

        default:
          theNode->SetGenericState(PR_TRUE);
          result=CElement::HandleStartToken(aNode,aTag,aContext,aSink);  
          break;
      } //switch
    }
    return result;
  }

};

/**********************************************************
  This defines the fieldset element...
 **********************************************************/
class CFieldsetElement: public CBlockElement {
public:

  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mBlock=1;
    return theGroup;
  }

  static CGroupMembers& GetContainedGroups(void) {
    return CFlowElement::GetContainedGroups();
  }

  static void Initialize(CElement& anElement,eHTMLTags aTag){
    CElement::Initialize(anElement,aTag,GetGroup(),GetContainedGroups());
  }

  CFieldsetElement() : CBlockElement(eHTMLTag_fieldset) {
    mGroup=GetGroup();
    mContainsGroups=GetContainedGroups();
    mProperties.mIsContainer=1;
  }

  /**********************************************************
    fieldset  handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleStartToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    nsIParserNode *theNode=aContext->PeekNode();
    if(theNode) {
      PRBool  theLegendExists=theNode->GetGenericState();
      switch(aTag) {
        case eHTMLTag_legend:
          if(!theLegendExists) {
            theNode->SetGenericState(PR_TRUE);
            result=OpenContainerInContext(aNode,aTag,aContext,aSink);  //force the title onto the stack
          }
          break;
        default:
          if(theLegendExists) {
            result=CElement::HandleStartToken(aNode,aTag,aContext,aSink);  //force the title onto the stack
          }
          break;
      } //switch
    }
    return result;
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


  /**********************************************************
    Toplevel handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleStartToken(  nsCParserNode* aNode,
                                      eHTMLTags aTag,
                                      nsDTDContext* aContext,
                                      nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_unknown:
      default:
        result=CElement::HandleStartToken(aNode,aTag,aContext,aSink);
        break;
    }//switch

    return result;
  }

  /**********************************************************
    TopLevel handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleEndToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {

    nsresult      result=NS_OK;

    switch(aTag) {
      case eHTMLTag_html:
        if(aContext->HasOpenContainer(aTag)) {
          result=aSink->CloseHTML();
          CloseContext(aNode,aTag,aContext,aSink);
        }
        break;

      case eHTMLTag_body:
        if(aContext->HasOpenContainer(aTag)) {
          result=aSink->CloseBody();
          CloseContext(aNode,aTag,aContext,aSink);
        }
        break;

      case eHTMLTag_frameset:
        if(aContext->HasOpenContainer(aTag)) {
          result=aSink->OpenFrameset(*aNode); 
          CloseContext(aNode,aTag,aContext,aSink);
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

  /**********************************************************
    HTML handles the opening of it's own children
   **********************************************************/
  nsresult HandleDoctypeDecl( nsIParserNode* aNode,
                              eHTMLTags aTag,
                              nsDTDContext* aContext,
                              nsIHTMLContentSink* aSink) {

    nsCParserNode *theNode=(nsCParserNode*)aNode;
    nsresult result=NS_OK;
    if(theNode) {
      nsAutoString  theStr(theNode->mToken->GetStringValue());
      PRInt32   theLen=theStr.Length();
      //PRInt32   thePos=theStr.RFindChar(kGreaterThan);

      theStr.Truncate(theLen-1);
      theStr.Cut(0,2);
  
      result = aSink->AddDocTypeDecl(*aNode);
    }
    return result;
  }

  /**********************************************************
    HTML handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleStartToken(  nsCParserNode* aNode,
                                      eHTMLTags aTag,
                                      nsDTDContext* aContext,
                                      nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_doctypeDecl:
        result=HandleDoctypeDecl(aNode,aTag,aContext,aSink);
        break;

      case eHTMLTag_frameset:
        result=aSink->OpenFrameset(*aNode); 
        result=OpenContext(aNode,aTag,aContext,aSink);
        aContext->mFlags.mHadFrameset=PR_TRUE;
        break;

      case eHTMLTag_base: //nothing to do for these empty tags...      
      case eHTMLTag_isindex:
      case eHTMLTag_link:
      case eHTMLTag_meta:
        {
          CElement* theHead=GetElement(eHTMLTag_head);
          if(theHead) {
            result=theHead->OpenContext(aNode,aTag,aContext,aSink);
            if(NS_SUCCEEDED(result)) {
              result=aSink->AddLeaf(*aNode);
              if(NS_SUCCEEDED(result)) {
                result=theHead->CloseContext(aNode,aTag,aContext,aSink);
              }
            }
          }
        }
        break;

      case eHTMLTag_object:
        {
          CElement* theHead=GetElement(eHTMLTag_head);
          if(theHead) {
            result=theHead->OpenContext(aNode,aTag,aContext,aSink);
            if(NS_SUCCEEDED(result)) {
              result=OpenContainerInContext(aNode,aTag,aContext,aSink);
            }
          }
        }
        break;

      case eHTMLTag_script:
      case eHTMLTag_style:
      case eHTMLTag_title:
        result=OpenContext(aNode,aTag,aContext,aSink);  //force the title onto the context stack
        break;

      case eHTMLTag_newline:
      case eHTMLTag_whitespace:
      case eHTMLTag_comment:
        break;


      default:
        CElement* theBody=GetElement(eHTMLTag_body);
        if(theBody) {
          CElement *theChildElement=GetElement(aTag);
          if(theBody->CanContain(theChildElement,aContext)) {
            //let's auto open the body            
            
            CToken* theToken=(CStartToken*)aContext->mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_body);
            nsCParserNode* theNode=aContext->mNodeAllocator->CreateNode(theToken, 0);

            result=theBody->HandleStartToken(theNode,eHTMLTag_body,aContext,aSink);

            if(NS_SUCCEEDED(result)) {
              if(eHTMLTag_body==aContext->Last()) {
                result=theBody->HandleStartToken(aNode,aTag,aContext,aSink);
              }
            }
          }
        }
        //for now, let's drop other elements onto the floor.
        break;
    }//switch

    return result;
  }

  /**********************************************************
    HTML handles the closing of it's own children
   **********************************************************/
  virtual nsresult HandleEndToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;

    switch(aTag) {
      case eHTMLTag_body:
        aSink->CloseBody();
        result=CloseContext(aNode,aTag,aContext,aSink);
        break;

      case eHTMLTag_frameset:
        aSink->CloseFrameset();
        result=CloseContext(aNode,aTag,aContext,aSink);
        break;

      case eHTMLTag_object:
        result=CloseContainerInContext(aNode,aTag,aContext,aSink);
        aSink->CloseHead();
        break;

      case eHTMLTag_script:
      case eHTMLTag_style:
      case eHTMLTag_title:
        result=CloseContext(aNode,aTag,aContext,aSink);  //close the title
        break;

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
static const eHTMLTags gBodyKids[] = {eHTMLTag_button, eHTMLTag_del, eHTMLTag_ins, eHTMLTag_map,eHTMLTag_script, eHTMLTag_unknown};
static const eHTMLTags gBodyExcludeKids[] = {eHTMLTag_applet, eHTMLTag_button, eHTMLTag_iframe, eHTMLTag_object, eHTMLTag_unknown};

class CBodyElement: public CElement {
public:

 
  static CGroupMembers& GetGroup(void) {
    static CGroupMembers theGroup={0};
    theGroup.mBits.mTopLevel=1;
    return theGroup;
  }

  CBodyElement(eHTMLTags aTag=eHTMLTag_body) : CElement(aTag) {    
    CGroupMembers theGroups=CBlockElement::GetBlockGroupMembers();
    CElement::Initialize(*this,aTag,CBodyElement::GetGroup(),theGroups);
    mIncludeKids=gBodyKids;
    mExcludeKids=gBodyExcludeKids;
  }

  virtual PRBool CanContain(CElement* anElement,nsDTDContext* aContext) {
    PRBool result=CElement::CanContain(anElement,aContext);
    if((!result) && (aContext->mFlags.mTransitional)) {
      //let's try so additions that are specific to the body tag,
      //and only work in transitional mode...
        
      CGroupMembers& theFlowGroup=CFlowElement::GetContainedGroups();
      result=ContainsGroup(theFlowGroup,anElement->mGroup);            
    }
    return result;
  }

   //this gets called after each tag is opened in the given context
  virtual nsresult  OpenContainer(nsCParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {

    // XXXldb This method is completely unused because the |aNode|
    // parameter is declared as |nsCParserNode| rather than
    // |nsIParserNode| so it doesn't override the member function of
    // CElement.
    NS_NOTREACHED("This isn't used.  Should it be?");

    nsresult result=NS_OK;
    if(mTag==aTag) {
      // Close the head before opening a body.
      CElement* theHead=GetElement(eHTMLTag_head);
      result=theHead->CloseContext(aNode,aTag,aContext,aSink);
      if(NS_SUCCEEDED(result)) {
        result=aSink->OpenBody(*aNode);
      }
    }
    else result=CElement::OpenContainer(aNode,aTag,aContext,aSink);
    return result;
  }

  /**********************************************************
    this gets called after each tag is opened in the given context
   **********************************************************/
  virtual nsresult OpenContainerInContext(nsCParserNode *aNode,eHTMLTags aTag,nsDTDContext *aContext,nsIHTMLContentSink *aSink) {
    NS_ASSERTION(aContext!=nsnull,"need a valid context");
    nsresult result=NS_OK;
    // Since BODY is optional, we might come across more than one BODY!.
    // That is, one that's auto opened and one that came from the document itself.
    // If that's the case then make sure that we don't open up multiple contexts, however,
    // don't forget to inform the sink because it needs to account for the BODY attributes.
    if(aContext) {
      if(!aContext->mFlags.mHadBody) {
        result=OpenContext(aNode,aTag,aContext,aSink);
        aContext->mFlags.mHadBody=PR_TRUE;
      }
    }
    return (NS_SUCCEEDED(result))? OpenContainer(aNode,aTag,aContext,aSink):result;
  }

  /**********************************************************
    Body handles the opening of it's own children
   **********************************************************/
  virtual nsresult HandleStartToken(  nsCParserNode* aNode,
                                      eHTMLTags aTag,
                                      nsDTDContext* aContext,
                                      nsIHTMLContentSink* aSink) {
    //for now, let's drop other elements onto the floor.

    nsresult result=CElement::HandleStartToken(aNode,aTag,aContext,aSink);

    if(NS_SUCCEEDED(result)) {
      if(aNode) {
        nsCParserNode*  theNode=(nsCParserNode*)aNode;
        eHTMLTokenTypes theType=eHTMLTokenTypes(theNode->GetTokenType());
        if(theType==eToken_start) {
          CStartToken *theToken=(CStartToken*)theNode->mToken;
          if(theToken && theToken->IsEmpty() && (aTag==aContext->Last())){
            result=CElement::HandleEndToken(aNode,aTag,aContext,aSink);
          }
        }
      }
    }

    return result;
  }

  /**********************************************************
    Body doesnt really need to handle it's own kids, but it's
    a really convenient break point for debugging purposes.
   **********************************************************/
  virtual nsresult HandleEndToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    switch(aTag) {
      case eHTMLTag_script:
        result=CloseContext(aNode,aTag,aContext,aSink);
        break;
      default:
        result=CElement::HandleEndToken(aNode,aTag,aContext,aSink);
    }
    return result;
  }

  /**********************************************************
    Body is the default place where forwarding stops.
   **********************************************************/   
  virtual nsresult  HandleEndTokenForChild(CElement *aChild,nsIParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
    nsresult result=NS_OK;
    return result;
  }

protected:


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
    mHTMLElement(eHTMLTag_html),
    mScriptElement(),
    mStyleElement(),
    mTitleElement(),
    mTextAreaElement(),
    mPreElement(eHTMLTag_pre),
    mLIElement(eHTMLTag_li),
    mAppletElement(eHTMLTag_applet),
    mObjectElement(eHTMLTag_object),
    mFieldsetElement(),
    mCounterElement(),
    mFormElement(),
    mHeadElement(eHTMLTag_head)
  {
    memset(mElements,0,sizeof(mElements));
    InitializeElements();

    //DebugDumpBlockElements("test");
    //DebugDumpInlineElements("test");

    //DebugDumpContainment("all elements");
  }

    //call this to get a ptr to an element prototype...
  CElement* GetElement(eHTMLTags aTagID) {
    if(aTagID>eHTMLTag_unknown) {
      if(aTagID<eHTMLTag_userdefined) {
        return mElements[aTagID];
      } 
    }
    return 0;
  }

  void InitializeElements();
#ifdef DEBUG
  void DebugDumpGroups(CElement* aParent);
  void DebugDumpContainment(const char* aTitle);
  void DebugDumpContainment(CElement* aParent);

  void DebugDumpInlineElements(const char* aTitle);
  void DebugDumpBlockElements(const char* aTitle);
#endif

  CElement* mElements[150];  //add one here for special handling of a given element
  CElement  mDfltElements[150];

  CBodyElement      mBodyElement;
  CFrameElement     mFramesetElement;
  CHTMLElement      mHTMLElement;
  CScriptElement    mScriptElement;
  CStyleElement     mStyleElement;
  CTitleElement     mTitleElement;
  CTextAreaElement  mTextAreaElement;
  CPreformattedElement mPreElement;
  CTableElement     mTableElement;
  CLIElement        mLIElement;
  CAppletElement    mAppletElement;
  CAppletElement    mObjectElement;
  CFieldsetElement  mFieldsetElement;
  CCounterElement   mCounterElement;
  CFormElement      mFormElement;
  CHeadElement      mHeadElement;
};


static CElementTable *gElementTable = 0;

static const eHTMLTags kDLKids[]={eHTMLTag_dd,eHTMLTag_dt,eHTMLTag_unknown};
static const eHTMLTags kAutoCloseDD[]={eHTMLTag_dd,eHTMLTag_dt,eHTMLTag_dl,eHTMLTag_unknown};
static const eHTMLTags kButtonExcludeKids[]={ eHTMLTag_a,eHTMLTag_button,eHTMLTag_select,eHTMLTag_textarea,
                                        eHTMLTag_input,eHTMLTag_iframe,eHTMLTag_form,eHTMLTag_isindex,
                                        eHTMLTag_fieldset,eHTMLTag_unknown};
static const eHTMLTags kColgroupKids[]={eHTMLTag_col,eHTMLTag_unknown};
static const eHTMLTags kDirKids[]={eHTMLTag_li,eHTMLTag_unknown};
static const eHTMLTags kOptionGroupKids[]={eHTMLTag_option,eHTMLTag_unknown};
static const eHTMLTags kFieldsetKids[]={eHTMLTag_legend,eHTMLTag_unknown};
static const eHTMLTags kFormKids[]={eHTMLTag_script,eHTMLTag_unknown};
static const eHTMLTags kLIExcludeKids[]={eHTMLTag_dir,eHTMLTag_menu,eHTMLTag_unknown};
static const eHTMLTags kMapKids[]={eHTMLTag_area,eHTMLTag_unknown};
static const eHTMLTags kPreExcludeKids[]={eHTMLTag_image,eHTMLTag_object,eHTMLTag_applet,
                                    eHTMLTag_big,eHTMLTag_small,eHTMLTag_sub,eHTMLTag_sup,
                                    eHTMLTag_font,eHTMLTag_basefont,eHTMLTag_unknown};
static const eHTMLTags kSelectKids[]={eHTMLTag_optgroup,eHTMLTag_option,eHTMLTag_unknown};
static const eHTMLTags kBlockQuoteKids[]={eHTMLTag_script,eHTMLTag_unknown};
static const eHTMLTags kFramesetKids[]={eHTMLTag_noframes,eHTMLTag_unknown};
static const eHTMLTags kObjectKids[]={eHTMLTag_param,eHTMLTag_unknown};
static const eHTMLTags kTBodyKids[]={eHTMLTag_tr,eHTMLTag_unknown};
static const eHTMLTags kUnknownKids[]={eHTMLTag_html,eHTMLTag_unknown};


inline CElement* CElement::GetElement(eHTMLTags aTag) {
  return gElementTable->mElements[aTag];
}


/***********************************************************************************
  This method is pretty interesting, because it's where the elements all get 
  initialized for this elementtable.
 ***********************************************************************************/
void CElementTable::InitializeElements() {

  int max=sizeof(mElements)/sizeof(mElements[0]);
  int index=0;
  for(index=0;index<max;++index){
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
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_base],       eHTMLTag_base,  CHeadElement::GetMiscGroup(), CLeafElement::GetContainedGroups());

  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_basefont],   eHTMLTag_basefont, CSpecialElement::GetGroup(), CLeafElement::GetContainedGroups());

  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_bdo],        eHTMLTag_bdo);
  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_big],        eHTMLTag_big);
  CDeprecatedElement::Initialize(   mDfltElements[eHTMLTag_bgsound],    eHTMLTag_bgsound);
  CElement::Initialize(             mDfltElements[eHTMLTag_blockquote], eHTMLTag_blockquote, CBlockElement::GetGroup(), CBlockElement::GetBlockGroupMembers());
  mDfltElements[eHTMLTag_blockquote].mIncludeKids=kBlockQuoteKids;

  //CBodyElement::Initialize(       mDfltElements[eHTMLTag_body],       eHTMLTag_body);
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_br],         eHTMLTag_br,  CSpecialElement::GetGroup(), CLeafElement::GetContainedGroups());

  CElement::Initialize(             mDfltElements[eHTMLTag_button],     eHTMLTag_button,  CFormControlElement::GetGroup(), CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_button].mGroup.mBits.mBlock=1;  //make this a member of the block group.
  mDfltElements[eHTMLTag_button].mExcludeKids=kButtonExcludeKids;
  

  CElement::Initialize(             mDfltElements[eHTMLTag_caption],    eHTMLTag_caption, CTableElement::GetGroup(), CSpecialElement::GetContainedGroups());
  mDfltElements[eHTMLTag_tr].mContainsGroups.mBits.mSelf=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_center],     eHTMLTag_center, CBlockElement::GetGroup(), CFlowElement::GetContainedGroups());

  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_cite],       eHTMLTag_cite);
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_code],       eHTMLTag_code);
  CElement::Initialize(             mDfltElements[eHTMLTag_col],        eHTMLTag_col, CTableElement::GetGroup(), CLeafElement::GetContainedGroups());
  mDfltElements[eHTMLTag_col].mProperties.mIsContainer=0;

  CTableElement::Initialize(        mDfltElements[eHTMLTag_colgroup],   eHTMLTag_colgroup);
  mDfltElements[eHTMLTag_colgroup].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_colgroup].mIncludeKids=kColgroupKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_counter],    eHTMLTag_counter);

  CElement::Initialize(             mDfltElements[eHTMLTag_dd],         eHTMLTag_dd,  CElement::GetEmptyGroup(),   CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_dd].mAutoClose=kAutoCloseDD;
  mDfltElements[eHTMLTag_dd].mContainsGroups.mBits.mSelf=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_del],        eHTMLTag_del, CPhraseElement::GetGroup(),  CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_del].mGroup.mBits.mBlock=1;  //make this a member of the block group.

  CElement::Initialize(             mDfltElements[eHTMLTag_dfn],        eHTMLTag_dfn, CPhraseElement::GetGroup(), CInlineElement::GetContainedGroups());
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_dir],        eHTMLTag_dir);
  mDfltElements[eHTMLTag_dir].mGroup.mBits.mList=1;
  mDfltElements[eHTMLTag_dir].mIncludeKids=kDirKids;
  mDfltElements[eHTMLTag_dir].mContainsGroups.mAllBits=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_div],        eHTMLTag_div, CBlockElement::GetGroup(),  CFlowElement::GetContainedGroups());
 
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_dl],         eHTMLTag_dl);  
  mDfltElements[eHTMLTag_dl].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_dl].mIncludeKids=kDLKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_dt],         eHTMLTag_dt, CElement::GetEmptyGroup(), CInlineElement::GetContainedGroups());
  mDfltElements[eHTMLTag_dt].mContainsGroups.mBits.mLeaf=1;
  mDfltElements[eHTMLTag_dt].mAutoClose=kAutoCloseDD;
  
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_em],         eHTMLTag_em);
  CElement::Initialize(   mDfltElements[eHTMLTag_embed],                eHTMLTag_embed);
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_endnote],    eHTMLTag_endnote);

  CElement::Initialize(             mDfltElements[eHTMLTag_fieldset],   eHTMLTag_fieldset, CBlockElement::GetGroup(),  CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_fieldset].mIncludeKids=kFieldsetKids;

  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_font],       eHTMLTag_font);
  CElement::Initialize(             mDfltElements[eHTMLTag_form],       eHTMLTag_form, CBlockElement::GetGroup(), CBlockElement::GetBlockGroupMembers());
  mDfltElements[eHTMLTag_form].mContainsGroups.mBits.mFormControl=1;
  mDfltElements[eHTMLTag_form].mIncludeKids=kFormKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_frame],      eHTMLTag_frame, CFrameElement::GetGroup(), CLeafElement::GetContainedGroups());
  mDfltElements[eHTMLTag_frame].mProperties.mIsContainer=0;

  CFrameElement::Initialize(        mDfltElements[eHTMLTag_frameset],   eHTMLTag_frameset);
  mDfltElements[eHTMLTag_frameset].mIncludeKids=kFramesetKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_h1],         eHTMLTag_h1, CBlockElement::GetGroup(), CBlockElement::GetContainedGroups(PR_FALSE));
  CElement::Initialize(             mDfltElements[eHTMLTag_h2],         eHTMLTag_h2, CBlockElement::GetGroup(), CBlockElement::GetContainedGroups(PR_FALSE));
  CElement::Initialize(             mDfltElements[eHTMLTag_h3],         eHTMLTag_h3, CBlockElement::GetGroup(), CBlockElement::GetContainedGroups(PR_FALSE));
  CElement::Initialize(             mDfltElements[eHTMLTag_h4],         eHTMLTag_h4, CBlockElement::GetGroup(), CBlockElement::GetContainedGroups(PR_FALSE));
  CElement::Initialize(             mDfltElements[eHTMLTag_h5],         eHTMLTag_h5, CBlockElement::GetGroup(), CBlockElement::GetContainedGroups(PR_FALSE));
  CElement::Initialize(             mDfltElements[eHTMLTag_h6],         eHTMLTag_h6, CBlockElement::GetGroup(), CBlockElement::GetContainedGroups(PR_FALSE));

  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_hr],         eHTMLTag_hr,        CBlockElement::GetGroup(), CLeafElement::GetContainedGroups());


  CElement::Initialize(             mDfltElements[eHTMLTag_head],       eHTMLTag_head,      CHeadElement::GetGroup(), CHeadElement::GetContainedGroups());
  // InitializeElement(             mDfltElements[eHTMLTag_head],       eHTMLTag_html,      CTopLevelElement::GetGroup(), CTopLevelElement::GetContainedGroups());

  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_i],          eHTMLTag_i);
  CElement::Initialize(             mDfltElements[eHTMLTag_iframe],     eHTMLTag_iframe,    CSpecialElement::GetGroup(),  CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_iframe].mGroup.mBits.mBlock=1;  //make this a member of the block group.

  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_img],        eHTMLTag_img,       CSpecialElement::GetGroup(),  CLeafElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_image],      eHTMLTag_image);
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_input],      eHTMLTag_input,     CFormControlElement::GetGroup(),CLeafElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_ins],        eHTMLTag_ins,       CPhraseElement::GetGroup(),  CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_ins].mGroup.mBits.mBlock=1;  //make this a member of the block group.

  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_isindex],    eHTMLTag_isindex,   CBlockElement::GetGroup(), CLeafElement::GetContainedGroups());

  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_kbd],        eHTMLTag_kbd);
  CDeprecatedElement::Initialize(   mDfltElements[eHTMLTag_keygen],     eHTMLTag_keygen);

  CElement::Initialize(             mDfltElements[eHTMLTag_label],      eHTMLTag_label,     CFormControlElement::GetGroup(), CInlineElement::GetContainedGroups());
  mDfltElements[eHTMLTag_label].mContainsGroups.mBits.mSelf=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_legend],     eHTMLTag_legend,    CElement::GetEmptyGroup(), CInlineElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_li],         eHTMLTag_li,        CListElement::GetGroup(), CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_li].mExcludeKids=kLIExcludeKids;
  mDfltElements[eHTMLTag_li].mContainsGroups.mBits.mSelf=0;
  
  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_link],       eHTMLTag_link,  CHeadElement::GetMiscGroup(), CLeafElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_listing],    eHTMLTag_listing);

  CElement::Initialize(             mDfltElements[eHTMLTag_map],        eHTMLTag_map,   CSpecialElement::GetGroup(), CBlockElement::GetBlockGroupMembers());
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
  CElement::Initialize(             mDfltElements[eHTMLTag_noscript],   eHTMLTag_noscript,  CBlockElement::GetGroup(),  CFlowElement::GetContainedGroups());

  CElement::Initialize(             mDfltElements[eHTMLTag_object],     eHTMLTag_object,    CBlockElement::GetGroup(),  CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_object].mGroup.mBits.mBlock=1;  //make this a member of the block group.
  mDfltElements[eHTMLTag_object].mGroup.mBits.mHeadMisc=1;
  mDfltElements[eHTMLTag_object].mIncludeKids=kObjectKids;

  CBlockElement::Initialize(        mDfltElements[eHTMLTag_ol],         eHTMLTag_ol);
  mDfltElements[eHTMLTag_ol].mGroup.mBits.mList=1;
  mDfltElements[eHTMLTag_ol].mIncludeKids=kDirKids;
  mDfltElements[eHTMLTag_ol].mContainsGroups.mAllBits=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_optgroup],   eHTMLTag_optgroup, CElement::GetEmptyGroup(), CElement::GetEmptyGroup());
  mDfltElements[eHTMLTag_optgroup].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_optgroup].mIncludeKids=kOptionGroupKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_option],     eHTMLTag_option, CElement::GetEmptyGroup(), CElement::GetEmptyGroup());
  mDfltElements[eHTMLTag_option].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_option].mContainsGroups.mBits.mLeaf=1;

  CElement::Initialize(             mDfltElements[eHTMLTag_p],          eHTMLTag_p, CBlockElement::GetGroup(), CInlineElement::GetContainedGroups());
  mDfltElements[eHTMLTag_p].mContainsGroups.mBits.mSelf=0;

  CElement::InitializeLeaf(         mDfltElements[eHTMLTag_param],      eHTMLTag_param, CElement::GetEmptyGroup(), CLeafElement::GetContainedGroups());
  CBlockElement::Initialize(        mDfltElements[eHTMLTag_pre],        eHTMLTag_pre);
  mDfltElements[eHTMLTag_pre].mExcludeKids=kPreExcludeKids;

  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_q],          eHTMLTag_q);
  
  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_s],          eHTMLTag_s);
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_samp],       eHTMLTag_samp );
  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_script],     eHTMLTag_script);
  mDfltElements[eHTMLTag_script].mGroup.mBits.mBlock=1;  //make this a member of the block group.
  mDfltElements[eHTMLTag_script].mGroup.mBits.mHeadMisc=1;

  CFormControlElement::Initialize(  mDfltElements[eHTMLTag_select],     eHTMLTag_select);
  mDfltElements[eHTMLTag_select].mContainsGroups.mAllBits=0;
  mDfltElements[eHTMLTag_select].mIncludeKids=kSelectKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_server],     eHTMLTag_server);
  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_small],      eHTMLTag_small);
  CElement::Initialize(             mDfltElements[eHTMLTag_spacer],     eHTMLTag_spacer);
  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_span],       eHTMLTag_span);
  CFontStyleElement::Initialize(    mDfltElements[eHTMLTag_strike],     eHTMLTag_strike);
  CPhraseElement::Initialize(       mDfltElements[eHTMLTag_strong],     eHTMLTag_strong);
  CHeadElement::Initialize(         mDfltElements[eHTMLTag_style],      eHTMLTag_style);
  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_sub],        eHTMLTag_sub);
  CSpecialElement::Initialize(      mDfltElements[eHTMLTag_sup],        eHTMLTag_sup);

  CElement::Initialize(             mDfltElements[eHTMLTag_table],      eHTMLTag_table,  CBlockElement::GetGroup(), CTableElement::GetContainedGroups());
  CElement::Initialize(             mDfltElements[eHTMLTag_tbody],      eHTMLTag_tbody,  CTableElement::GetGroup(), CLeafElement::GetContainedGroups());
  mDfltElements[eHTMLTag_tbody].mIncludeKids=kTBodyKids;

  CElement::Initialize(             mDfltElements[eHTMLTag_td],         eHTMLTag_td,      CElement::GetEmptyGroup(), CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_td].mContainsGroups.mBits.mSelf=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_textarea],   eHTMLTag_textarea);
  
  CElement::Initialize(             mDfltElements[eHTMLTag_tfoot],      eHTMLTag_tfoot,   CTableElement::GetGroup(), CLeafElement::GetContainedGroups());
  mDfltElements[eHTMLTag_tfoot].mIncludeKids=kTBodyKids;
  mDfltElements[eHTMLTag_tfoot].mContainsGroups.mBits.mSelf=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_th],         eHTMLTag_th,      CElement::GetEmptyGroup(), CFlowElement::GetContainedGroups());
  mDfltElements[eHTMLTag_th].mContainsGroups.mBits.mSelf=0;

  CElement::Initialize(             mDfltElements[eHTMLTag_thead],      eHTMLTag_thead,   CTableElement::GetGroup(), CLeafElement::GetContainedGroups());
  mDfltElements[eHTMLTag_thead].mIncludeKids=kTBodyKids;

  CTableRowElement::Initialize(     mDfltElements[eHTMLTag_tr],         eHTMLTag_tr);
  mDfltElements[eHTMLTag_tr].mContainsGroups.mBits.mSelf=0;

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

  CElement::Initialize(mDfltElements[eHTMLTag_userdefined], 
                       eHTMLTag_userdefined, 
                       CElement::GetEmptyGroup(),
                       CFlowElement::GetContainedGroups()); // allow userdefined tag to contain anything.

  mDfltElements[eHTMLTag_unknown].mIncludeKids=kUnknownKids;


  /************************************************************
     Now let's initialize the elements that we created directly 
     to handle special cases.
   ************************************************************/

  mElements[eHTMLTag_body]=&mBodyElement;
  mElements[eHTMLTag_frameset]=&mFramesetElement;
  mElements[eHTMLTag_html]=&mHTMLElement;
  mElements[eHTMLTag_script]=&mScriptElement;
  mElements[eHTMLTag_style]=&mStyleElement;
  mElements[eHTMLTag_title]=&mTitleElement;
  mElements[eHTMLTag_textarea]=&mTextAreaElement;
  mElements[eHTMLTag_pre]=&mPreElement;
  mElements[eHTMLTag_table]=&mTableElement;
  mElements[eHTMLTag_li]=&mLIElement;
  mElements[eHTMLTag_applet]=&mAppletElement;
  mElements[eHTMLTag_object]=&mObjectElement;
  mElements[eHTMLTag_fieldset]=&mFieldsetElement;
  mElements[eHTMLTag_counter]=&mCounterElement;
  mElements[eHTMLTag_form]=&mFormElement;
  mElements[eHTMLTag_head]=&mHeadElement;
}

#ifdef DEBUG
void CElementTable::DebugDumpGroups(CElement* aTag){

  const PRUnichar* uctag=nsHTMLTags::GetStringValue(aTag->mTag);

  const char* prefix="     ";
  printf("\n\nTag: <%s>\n", NS_ConvertUCS2toUTF8(uctag).get());
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
      const eHTMLTags *theKid=aTag->mIncludeKids;
      printf("+ ");
      while(eHTMLTag_unknown!=*theKid){
        const PRUnichar *t = nsHTMLTags::GetStringValue(*theKid++);
        printf("%s ", NS_ConvertUCS2toUTF8(t).get());
      }
    }

    if(aTag->mExcludeKids) {
      printf("\n%s",prefix);
      const eHTMLTags *theKid=aTag->mExcludeKids;
      printf("- ");
      while(eHTMLTag_unknown!=*theKid){
        const PRUnichar *t = nsHTMLTags::GetStringValue(*theKid++);
        printf("%s ", NS_ConvertUCS2toUTF8(t).get());
      }
    }

    if(!aTag->mContainsGroups.mBits.mSelf){
      printf("\n%s - self",prefix);
    }
 
  }
  else {
    printf("empty\n");
  }
}

void CElementTable::DebugDumpContainment(CElement* anElement){
  const PRUnichar *uctag = nsHTMLTags::GetStringValue(anElement->mTag);
  const char* prefix="     ";
  printf("\n\nTag: <%s>\n", NS_ConvertUCS2toUTF8(uctag).get());
  printf(prefix);

  int count=0;
  int i=0;
  for(i=0;i<NS_HTML_TAG_MAX;++i){
    CElement* theChild=mElements[i];
    if(anElement->CanContain(theChild,0)){
      const PRUnichar *t = nsHTMLTags::GetStringValue(theChild->mTag);
      printf("%s ", NS_ConvertUCS2toUTF8(t).get());
      ++count;
      if(18==count) {
        count=0;
        printf("\n%s",prefix);
      }
    }
  }
}

void CElementTable::DebugDumpInlineElements(const char* aTitle) {
  PRInt32   theTagID=eHTMLTag_unknown;
  PRBool    result=PR_FALSE;

  printf("Inline Elements -- %s: \n",aTitle);
  while(theTagID<=eHTMLTag_userdefined) {
    CElement *theTag=GetElement((eHTMLTags)theTagID);
    if(theTag) {
      result=theTag->IsInlineElement(eHTMLTag_unknown);
      if(result) {
        const PRUnichar *t = nsHTMLTags::GetStringValue(theTag->mTag);
        printf("  %s\n", NS_ConvertUCS2toUTF8(t).get());
      }
    }
    theTagID++;
  }
}

void CElementTable::DebugDumpBlockElements(const char* aTitle) {
  PRInt32   theTagID=eHTMLTag_unknown;
  PRBool    result=PR_FALSE;

  printf("Block Elements -- %s: \n",aTitle);
  while(theTagID<=eHTMLTag_userdefined) {
    CElement *theTag=GetElement((eHTMLTags)theTagID);
    if(theTag) {
      result=theTag->IsBlockElement(eHTMLTag_unknown);
      if(result) {
        const PRUnichar *theName = nsHTMLTags::GetStringValue(theTag->mTag);
        printf("  %s\n", NS_ConvertUCS2toUTF8(theName).get());
      }
    }
    theTagID++;
  }
}

void CElementTable::DebugDumpContainment(const char* aTitle){
#if 0
  DebugDumpContainment(mElements[eHTMLTag_head]);
  DebugDumpContainment(mElements[eHTMLTag_html]);
  DebugDumpContainment(mElements[eHTMLTag_table]);
  printf("\n");
#endif

  printf("==================================================\n");
  printf("%s\n",aTitle);
  printf("==================================================\n");
  int i=0;

  for(i=1;i<NS_HTML_TAG_MAX;++i){
    DebugDumpContainment(mElements[i]);
    //DebugDumpGroups(mElements[i]);
  } //for
}
#endif

/******************************************************************************
  Yes, I know it's inconvenient to find this methods here, but it's easier
  for the compiler -- and making it's life easier is my top priority.
 ******************************************************************************/
PRInt32 CElement::FindAutoCloseIndexForStartTag(CElement* anElement,PRInt32 aParentIndex,nsDTDContext* aContext) {
  PRInt32 result=kNotFound;

  if(anElement) {
    eHTMLTags theParentTag=aContext->TagAt(aParentIndex);
    if(eHTMLTag_unknown!=theParentTag) {

      CElement* theParent=gElementTable->mElements[theParentTag];

      if(!theParent->CanContain(anElement,aContext)) {
        if(HasOptionalEndTag(theParentTag)) {

          if(ListContainsTag(theParent->mAutoClose,anElement->mTag)) {
            result=theParent->FindAutoCloseIndexForStartTag(anElement,aParentIndex-1,aContext);
          }
          else if((theParent->mTag==anElement->mTag) && (!theParent->mContainsGroups.mBits.mSelf)){
            result=aParentIndex;
          }
          else if(eHTMLTag_body!=theParent->mTag) {
            result=theParent->FindAutoCloseIndexForStartTag(anElement,aParentIndex-1,aContext);
          }
          else result=aParentIndex+1;
        }
        else result=kNotFound;
      } 
      else result=aParentIndex+1;
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
      if(HasOptionalEndTag(theTag)) {
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
    if(!anElement->mProperties.mDeprecated) {
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

      /***************************************************
        This is a (cheesy) exception table, that allows
        us to override containment for transitional 
        documents. A better implementation would be to
        create unique classes for each of the tags in 
        this table, and to override CanContain() there.
       ***************************************************/
    
    if((!result) && (aContext->mFlags.mTransitional)) {
      switch(mTag) {
        case eHTMLTag_address:
          if(eHTMLTag_p==anElement->mTag)
            result=PR_TRUE;
          break;

        case eHTMLTag_blockquote:
        case eHTMLTag_form:
        case eHTMLTag_iframe:
          result=ContainsGroup(CFlowElement::GetContainedGroups(),anElement->mGroup);            
          break;

        case eHTMLTag_button:
          if((eHTMLTag_iframe==anElement->mTag) || (eHTMLTag_isindex==anElement->mTag)) 
            result=PR_TRUE;
          break;

        default:
          break;
      }
    }
  }
  return result;
}

nsresult CElement::WillHandleStartToken(  CElement *anElement,
                                          nsIParserNode* aNode,
                                          eHTMLTags aTag,
                                          nsDTDContext* aContext,
                                          nsIHTMLContentSink* aSink) {
  nsresult result=NS_OK;
  return result;
}

nsresult CElement::HandleStartToken(  nsCParserNode* aNode,
                                      eHTMLTags aTag,
                                      nsDTDContext* aContext,
                                      nsIHTMLContentSink* aSink) {

  CElement* theElement=gElementTable->mElements[aTag];

  nsresult  result=WillHandleStartToken(theElement,aNode,aTag,aContext,aSink);

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
          if(theElement->IsSinkContainer()) {
            result=theElement->OpenContainerInContext(aNode,aTag,aContext,aSink);
          }
          else {
            result=theElement->OpenContext(aNode,aTag,aContext,aSink);
          }
        }
        else {
          result=aSink->AddLeaf(*aNode); 
        }
      }
      else if(theElement->IsBlockCloser()){

        //Ok, so we have a start token that is misplaced. Before handing this off
        //to a default container (parent), let's check the autoclose condition.
        if(HasOptionalEndTag(mTag)) {

          //aha! We have a case where this tag is autoclosed by anElement.
          //Let's close this container, then try to open theElement.
          PRInt32 theCount=aContext->GetCount();
          PRInt32 theIndex=FindAutoCloseIndexForStartTag(theElement,theCount-2,aContext);

          //continue ripping code out here...

          if(kNotFound!=theIndex) {
            eHTMLTags theParentTag=eHTMLTag_unknown;
            CElement* theParent=0;

            while(NS_SUCCEEDED(result) && (theCount>theIndex)) {

              theParentTag=aContext->Last();
              theParent=gElementTable->mElements[theParentTag];

              nsCParserNode *theNode=aContext->PeekNode(); //this will get popped later...
              if(theParent->IsSinkContainer()) {
                CloseContainerInContext(theNode,theParentTag,aContext,aSink);
              }
              else CloseContext(theNode,theParentTag,aContext,aSink);
              theCount--;
            }

            if(NS_SUCCEEDED(result)){
              theParentTag=aContext->Last();
              theParent=gElementTable->mElements[theParentTag];
              result=theParent->HandleStartToken(aNode,aTag,aContext,aSink);
            }
          }
          return result;
        }
        else {
          
          PRBool theElementCanOpen=PR_FALSE;

            //the logic here is simple:
            //  This operation can only succeed if the given tag is open, AND
            //  all the tags below it have optional end tags.
            //  If these conditions aren't met, we bail out, leaving the tag open.

          if(mTag!=aTag) {
            PRInt32 theLastPos=aContext->LastOf(aTag); //see if it's already open...
            if(-1!=theLastPos) {
              PRInt32 theCount=aContext->GetCount();
              result=HandleEndToken(aNode,aTag,aContext,aSink);
              theElementCanOpen=PRBool(aContext->GetCount()<theCount);
            }
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


nsresult CElement::HandleEndToken(nsCParserNode* aNode,eHTMLTags aTag,nsDTDContext* aContext,nsIHTMLContentSink* aSink) {
  nsresult result=NS_OK;

  if(aContext->Last()==aTag) {
    CElement* theElement=gElementTable->mElements[aTag];
    if(theElement) {
      if(theElement->IsSinkContainer()) {
        result=CloseContainerInContext(aNode,aTag,aContext,aSink);
      }
      else result=CloseContext(aNode,aTag,aContext,aSink);
      return result;
    }
  }

  PRInt32   theCount=aContext->GetCount();
  PRInt32   theIndex=theCount-1;

  PRInt32 theCloseTarget=FindAutoCloseTargetForEndTag(aNode,aTag,aContext,aSink,theIndex);

  if(-1!=theCloseTarget) {
    while(theCloseTarget<theCount) {
      eHTMLTags theTag=aContext->Last();
      eHTMLTags theGrandParentTag=aContext->TagAt(theCount-2);
      CElement *theGrandParent=GetElement(theGrandParentTag);
      result=theGrandParent->HandleEndToken(aNode,theTag,aContext,aSink);
      theCount--;
    }
    //return result;
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


  //this tells us whether this tag is a block tag within the given parent
  //NOTE: aParentID is currently ignored, but shouldn't be.
PRBool CElement::IsBlockElement(eHTMLTags aParentID) {
  CGroupMembers& theBlockGroup=CBlockElement::GetBlockGroupMembers();
  PRBool result=ContainsGroup(theBlockGroup,mGroup);
  return result;
}

  //this tells us whether this tag is an inline tag within the given parent
  //NOTE: aParentID is currently ignored, but shouldn't be.
PRBool CElement::IsInlineElement(eHTMLTags aParentID) {
  CGroupMembers& theInlineGroup=CInlineElement::GetContainedGroups();
  PRBool result=ContainsGroup(theInlineGroup,mGroup);
  return result;
}


#endif
