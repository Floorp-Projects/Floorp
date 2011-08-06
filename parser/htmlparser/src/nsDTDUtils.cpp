/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
       
  
#include "nsIAtom.h"
#include "nsDTDUtils.h" 
#include "CNavDTD.h" 
#include "nsIParserNode.h"
#include "nsParserNode.h" 
#include "nsIChannel.h"
#include "nsIServiceManager.h"
#include "nsUnicharUtils.h"

/**************************************************************************************
  A few notes about how residual style handling is performed:
   
    1. The style stack contains nsTagEntry elements. 
    2. Every tag on the containment stack can have it's own residual style stack.
    3. When a style leaks, it's mParent member is set to the level on the stack where 
       it originated. A node with an mParent of 0 is not opened on tag stack, 
       but is open on stylestack.
    4. An easy way to tell that a container on the element stack is a residual style tag
       is that it's use count is >1.

 **************************************************************************************/


/**
 * Default constructor
 * @update	harishd 04/04/99
 * @update  gess 04/22/99
 */
nsEntryStack::nsEntryStack()  {

  MOZ_COUNT_CTOR(nsEntryStack);
  
  mCapacity=0;
  mCount=0;
  mEntries=0;
}

/**
 * Default destructor
 * @update  harishd 04/04/99
 * @update  gess 04/22/99
 */
nsEntryStack::~nsEntryStack() {

  MOZ_COUNT_DTOR(nsEntryStack);

  if(mEntries) {
    //add code here to recycle the node if you have one...  
    delete [] mEntries;
    mEntries=0;
  }

  mCount=mCapacity=0;
}

/**
 * Release all objects in the entry stack
 */
void 
nsEntryStack::ReleaseAll(nsNodeAllocator* aNodeAllocator)
{
  NS_ASSERTION(aNodeAllocator,"no allocator? - potential leak!");

  if(aNodeAllocator) {
    NS_ASSERTION(mCount >= 0,"count should not be negative");
    while(mCount > 0) {
      nsCParserNode* node=this->Pop();
      IF_FREE(node,aNodeAllocator);
    }
  }
}

/**
 * Resets state of stack to be empty.
 * @update harishd 04/04/99
 */
void nsEntryStack::Empty(void) {
  mCount=0;
}


/**
 * 
 * @update  gess 04/22/99
 */
void nsEntryStack::EnsureCapacityFor(PRInt32 aNewMax,PRInt32 aShiftOffset) {
  if(mCapacity<aNewMax){ 

    const int kDelta=16;

    PRInt32 theSize = kDelta * ((aNewMax / kDelta) + 1);
    nsTagEntry* temp=new nsTagEntry[theSize]; 
    mCapacity=theSize;

    if(temp){ 
      PRInt32 index=0; 
      for(index=0;index<mCount;++index) {
        temp[aShiftOffset+index]=mEntries[index];
      }
      if(mEntries) delete [] mEntries;
      mEntries=temp;
    }
    else{
      //XXX HACK! This is very bad! We failed to get memory.
    }
  } //if
}

/**
 * 
 * @update  gess 04/22/99
 */
void nsEntryStack::Push(nsCParserNode* aNode,
                        nsEntryStack* aStyleStack, 
                        PRBool aRefCntNode) 
{
  if(aNode) {
    EnsureCapacityFor(mCount+1);
    mEntries[mCount].mTag = (eHTMLTags)aNode->GetNodeType();
    if (aRefCntNode) {
      aNode->mUseCount++;
      mEntries[mCount].mNode = const_cast<nsCParserNode*>(aNode);
      IF_HOLD(mEntries[mCount].mNode);
    }
    mEntries[mCount].mParent=aStyleStack;
    mEntries[mCount++].mStyles=0;
  }
}

void nsEntryStack::PushTag(eHTMLTags aTag)
{
  EnsureCapacityFor(mCount + 1);
  mEntries[mCount].mTag = aTag;
  mEntries[mCount].mParent = nsnull;
  mEntries[mCount].mStyles = nsnull;
  ++mCount;
}


/**
 * This method inserts the given node onto the front of this stack
 *
 * @update  gess 11/10/99
 */
void nsEntryStack::PushFront(nsCParserNode* aNode,
                             nsEntryStack* aStyleStack, 
                             PRBool aRefCntNode) 
{
  if(aNode) {
    if(mCount<mCapacity) {
      PRInt32 index=0; 
      for(index=mCount;index>0;index--) {
        mEntries[index]=mEntries[index-1];
      }
    }
    else {
      EnsureCapacityFor(mCount+1,1);
    }
    mEntries[0].mTag = (eHTMLTags)aNode->GetNodeType();
    if (aRefCntNode) {
      aNode->mUseCount++;
      mEntries[0].mNode = const_cast<nsCParserNode*>(aNode);
      IF_HOLD(mEntries[0].mNode);
    }
    mEntries[0].mParent=aStyleStack;
    mEntries[0].mStyles=0;
    ++mCount;
  }
}

/**
 * 
 * @update  gess 11/10/99
 */
void nsEntryStack::Append(nsEntryStack *aStack) {
  if(aStack) {

    PRInt32 theCount=aStack->mCount;

    EnsureCapacityFor(mCount+aStack->mCount,0);

    PRInt32 theIndex=0;
    for(theIndex=0;theIndex<theCount;++theIndex){
      mEntries[mCount]=aStack->mEntries[theIndex];
      mEntries[mCount++].mParent=0;
    }
  }
} 
 
/**
 * This method removes the node for the given tag
 * from anywhere within this entry stack, and shifts
 * other entries down.
 * 
 * NOTE: It's odd to be removing an element from the middle
 *       of a stack, but it's necessary because of how MALFORMED
 *       html can be. 
 * 
 * anIndex: the index within the stack of the tag to be removed
 * aTag: the id of the tag to be removed
 * @update  gess 02/25/00
 */
nsCParserNode* nsEntryStack::Remove(PRInt32 anIndex,
                                    eHTMLTags aTag) 
{
  nsCParserNode* result = 0;
  if (0 < mCount && anIndex < mCount){
    result = mEntries[anIndex].mNode;
    if (result)
      result->mUseCount--;
    PRInt32 theIndex = 0;
    mCount -= 1;
    for( theIndex = anIndex; theIndex < mCount; ++theIndex){
      mEntries[theIndex] = mEntries[theIndex+1];
    }
    mEntries[mCount].mNode = 0;
    mEntries[mCount].mStyles = 0;
    nsEntryStack* theStyleStack = mEntries[anIndex].mParent;
    if (theStyleStack) {
      //now we have to tell the residual style stack where this tag
      //originated that it's no longer in use.
      PRUint32 scount = theStyleStack->mCount;
#ifdef DEBUG_mrbkap
      NS_ASSERTION(scount != 0, "RemoveStyles has a bad style stack");
#endif
      nsTagEntry *theStyleEntry = theStyleStack->mEntries;
      for (PRUint32 sindex = scount-1;; --sindex) {            
        if (theStyleEntry->mTag == aTag) {
          // This tells us that the style is not open at any level.
          theStyleEntry->mParent = nsnull;
          break;
        }
        if (sindex == 0) {
#ifdef DEBUG_mrbkap
          NS_ERROR("Couldn't find the removed style on its parent stack");
#endif
          break;
        }
        ++theStyleEntry;
      }
    }
  }
  return result;
}

/**
 * Pops an entry from this style stack. If the entry has a parent stack, it
 * updates the entry so that we know not to try to remove it from the parent
 * stack since it's no longer open.
 */
nsCParserNode* nsEntryStack::Pop(void)
{
  nsCParserNode* result = 0;
  if (0 < mCount) {
    result = mEntries[--mCount].mNode;
    if (result)
      result->mUseCount--;
    mEntries[mCount].mNode = 0;
    mEntries[mCount].mStyles = 0;
    nsEntryStack* theStyleStack = mEntries[mCount].mParent;
    if (theStyleStack) {
      //now we have to tell the residual style stack where this tag
      //originated that it's no longer in use.
      PRUint32 scount = theStyleStack->mCount;

      // XXX If this NS_ENSURE_TRUE fails, it means that the style stack was
      //     empty before we were removed.
#ifdef DEBUG_mrbkap
      NS_ASSERTION(scount != 0, "preventing a potential crash.");
#endif
      NS_ENSURE_TRUE(scount != 0, result);

      nsTagEntry *theStyleEntry = theStyleStack->mEntries;
      for (PRUint32 sindex = scount - 1;; --sindex) {
        if (theStyleEntry->mTag == mEntries[mCount].mTag) {
          // This tells us that the style is not open at any level
          theStyleEntry->mParent = nsnull;
          break;
        }
        if (sindex == 0) {
#ifdef DEBUG_mrbkap
          NS_ERROR("Couldn't find the removed style on its parent stack");
#endif
          break;
        }
        ++theStyleEntry;
      }
    }
  }
  return result;
} 

/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
eHTMLTags nsEntryStack::First() const 
{
  eHTMLTags result=eHTMLTag_unknown;
  if(0<mCount){
    result=mEntries[0].mTag;
  }
  return result;
}

/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
nsCParserNode* nsEntryStack::NodeAt(PRInt32 anIndex) const 
{
  nsCParserNode* result=0;
  if((0<mCount) && (anIndex<mCount)) {
    result=mEntries[anIndex].mNode;
  }
  return result;
}

/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
eHTMLTags nsEntryStack::TagAt(PRInt32 anIndex) const 
{
  eHTMLTags result=eHTMLTag_unknown;
  if((0<mCount) && (anIndex<mCount)) {
    result=mEntries[anIndex].mTag;
  }
  return result;
}

/**
 * 
 * @update  gess 04/21/99
 */
nsTagEntry* nsEntryStack::EntryAt(PRInt32 anIndex) const 
{
  nsTagEntry *result=0;
  if((0<mCount) && (anIndex<mCount)) {
    result=&mEntries[anIndex];
  }
  return result;
}


/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
eHTMLTags nsEntryStack::operator[](PRInt32 anIndex) const 
{
  eHTMLTags result=eHTMLTag_unknown;
  if((0<mCount) && (anIndex<mCount)) {
    result=mEntries[anIndex].mTag;
  }
  return result;
}


/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
eHTMLTags nsEntryStack::Last(void) const 
{
  eHTMLTags result=eHTMLTag_unknown;
  if(0<mCount) {
    result=mEntries[mCount-1].mTag;
  }
  return result;
}

nsTagEntry*
nsEntryStack::PopEntry() 
{
  nsTagEntry* entry = EntryAt(mCount-1);
  this->Pop();
  return entry;
}

void nsEntryStack::PushEntry(nsTagEntry* aEntry, 
                             PRBool aRefCntNode) 
{
  if (aEntry) {
    EnsureCapacityFor(mCount+1);
    mEntries[mCount].mNode   = aEntry->mNode;
    mEntries[mCount].mTag    = aEntry->mTag;
    mEntries[mCount].mParent = aEntry->mParent;
    mEntries[mCount].mStyles = aEntry->mStyles;
    if (aRefCntNode && mEntries[mCount].mNode) {
      mEntries[mCount].mNode->mUseCount++;
      IF_HOLD(mEntries[mCount].mNode);
    }
    mCount++;
  }
}

/***************************************************************
  Now define the dtdcontext class
 ***************************************************************/


/**
 * 
 * @update	gess 04.21.2000
 */
nsDTDContext::nsDTDContext() : mStack()
{
  MOZ_COUNT_CTOR(nsDTDContext);
  mResidualStyleCount=0;
  mContextTopIndex=-1;
  mTokenAllocator=0;
  mNodeAllocator=0;

#ifdef DEBUG
  memset(mXTags,0,sizeof(mXTags));
#endif
} 
 
/**
 * 
 * @update	gess9/10/98
 */
nsDTDContext::~nsDTDContext()
{
  MOZ_COUNT_DTOR(nsDTDContext);
}


/**
 * 
 * @update  gess7/9/98
 */
PRBool nsDTDContext::HasOpenContainer(eHTMLTags aTag) const {
  PRInt32 theIndex=mStack.LastOf(aTag);
  return PRBool(-1<theIndex);
}

/**
 * 
 * @update  gess7/9/98
 */
void nsDTDContext::Push(nsCParserNode* aNode,
                        nsEntryStack* aStyleStack, 
                        PRBool aRefCntNode) {
  if(aNode) {
#ifdef  NS_DEBUG
    eHTMLTags theTag = (eHTMLTags)aNode->GetNodeType();
    int size = mStack.mCount;
    if (size < eMaxTags)
      mXTags[size] = theTag;
#endif
    mStack.Push(aNode, aStyleStack, aRefCntNode);
  }
}

void nsDTDContext::PushTag(eHTMLTags aTag)
{
#ifdef NS_DEBUG
  if (mStack.mCount < eMaxTags) {
    mXTags[mStack.mCount] = aTag;
  }
#endif

  mStack.PushTag(aTag);
}

nsTagEntry*
nsDTDContext::PopEntry()
{
  PRInt32 theSize = mStack.mCount;
  if(0<theSize) {
#ifdef  NS_DEBUG
    if (theSize <= eMaxTags)
      mXTags[theSize-1]=eHTMLTag_unknown;
#endif
    return mStack.PopEntry();
  }
  return 0;
}

void nsDTDContext::PushEntry(nsTagEntry* aEntry, 
                             PRBool aRefCntNode)
{
#ifdef  NS_DEBUG
    int size=mStack.mCount;
    if(size< eMaxTags && aEntry)
      mXTags[size]=aEntry->mTag;
#endif
    mStack.PushEntry(aEntry, aRefCntNode);
}

/* This method will move the top entries, in the entry-stack, into dest context.
 * @param aDest  - Destination context for the entries.
 * @param aCount - Number of entries, on top of the entry-stack, to be moved.
 */
void 
nsDTDContext::MoveEntries(nsDTDContext& aDest,
                          PRInt32 aCount)
{
  NS_ASSERTION(aCount > 0 && mStack.mCount >= aCount, "cannot move entries");
  if (aCount > 0 && mStack.mCount >= aCount) {
    while (aCount) {
      aDest.PushEntry(&mStack.mEntries[--mStack.mCount], PR_FALSE);
#ifdef  NS_DEBUG
      if (mStack.mCount < eMaxTags) {
        mXTags[mStack.mCount] = eHTMLTag_unknown;
      }
#endif
      --aCount;
    }
  }
}

/** 
 * @update  gess 11/11/99, 
 *          harishd 04/04/99
 */
nsCParserNode* nsDTDContext::Pop(nsEntryStack *&aChildStyleStack) {

  PRInt32         theSize=mStack.mCount;
  nsCParserNode*  result=0;

  if(0<theSize) {

#ifdef  NS_DEBUG
    if ((theSize>0) && (theSize <= eMaxTags))
      mXTags[theSize-1]=eHTMLTag_unknown;
#endif


    nsTagEntry* theEntry=mStack.EntryAt(mStack.mCount-1);
    aChildStyleStack=theEntry->mStyles;

    result=mStack.Pop();
    theEntry->mParent=0;
  }

  return result;
}
 
/**
 * 
 * @update  harishd 04/07/00
 */

nsCParserNode* nsDTDContext::Pop() {
  nsEntryStack   *theTempStyleStack=0; // This has no use here...
  return Pop(theTempStyleStack);
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::First(void) const {
  return mStack.First();
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::TagAt(PRInt32 anIndex) const {
  return mStack.TagAt(anIndex);
}

/**
 * 
 * @update  gess7/9/98
 */
nsTagEntry* nsDTDContext::LastEntry(void) const {
  return mStack.EntryAt(mStack.mCount-1);
}

/**
 *  
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::Last() const {
  return mStack.Last();
}


/**
 * 
 * @update  gess7/9/98
 */
nsEntryStack* nsDTDContext::GetStylesAt(PRInt32 anIndex) const {
  nsEntryStack* result=0;

  if(anIndex<mStack.mCount){
    nsTagEntry* theEntry=mStack.EntryAt(anIndex);
    if(theEntry) {
      result=theEntry->mStyles;
    }
  }
  return result;
}


/**
 * 
 * @update  gess 04/28/99
 */
void nsDTDContext::PushStyle(nsCParserNode* aNode){

  nsTagEntry* theEntry=mStack.EntryAt(mStack.mCount-1);
  if(theEntry ) {
    nsEntryStack* theStack=theEntry->mStyles;
    if(!theStack) {
      theStack=theEntry->mStyles=new nsEntryStack();
    }
    if(theStack) {
      theStack->Push(aNode);
      ++mResidualStyleCount;
    }
  } //if
}


/**
 * Call this when you have an EntryStack full of styles
 * that you want to push at this level.
 * 
 * @update  gess 04/28/99
 */
void nsDTDContext::PushStyles(nsEntryStack *aStyles){

  if(aStyles) {
    nsTagEntry* theEntry=mStack.EntryAt(mStack.mCount-1);
    if(theEntry ) {
      nsEntryStack* theStyles=theEntry->mStyles;
      if(!theStyles) {
        theEntry->mStyles=aStyles;

        PRUint32 scount=aStyles->mCount;
        PRUint32 sindex=0;

        theEntry=aStyles->mEntries;
        for(sindex=0;sindex<scount;++sindex){            
          theEntry->mParent=0;  //this tells us that the style is not open at any level
          ++theEntry;
          ++mResidualStyleCount;
        } //for

      }
      else {
        theStyles->Append(aStyles);
        //  Delete aStyles since it has been copied to theStyles...
        delete aStyles;
        aStyles=0;
      }
    } //if(theEntry )
    else if(mStack.mCount==0) {
      // If you're here it means that we have hit the rock bottom
      // ,of the stack, and there's no need to handle anymore styles.
      // Fix for bug 29048
      IF_DELETE(aStyles,mNodeAllocator);
    }
  }//if(aStyles)
}


/** 
 * 
 * @update  gess 04/28/99
 */
nsCParserNode* nsDTDContext::PopStyle(void){
  nsCParserNode *result=0;

  nsTagEntry *theEntry=mStack.EntryAt(mStack.mCount-1);
  if(theEntry && (theEntry->mNode)) {
    nsEntryStack* theStyleStack=theEntry->mParent;
    if(theStyleStack){
      result=theStyleStack->Pop();
      mResidualStyleCount--;
    }
  } //if
  return result;
}

/** 
 * 
 * @update  gess 04/28/99
 */
nsCParserNode* nsDTDContext::PopStyle(eHTMLTags aTag){

  PRInt32 theLevel=0;
  nsCParserNode* result=0;

  for(theLevel=mStack.mCount-1;theLevel>0;theLevel--) {
    nsEntryStack *theStack=mStack.mEntries[theLevel].mStyles;
    if(theStack) {
      if(aTag==theStack->Last()) {
        result=theStack->Pop();
        mResidualStyleCount--;
        break; // Fix bug 50710 - Stop after finding a style.
      } else {
        // NS_ERROR("bad residual style entry");
      }
    } 
  }

  return result;
}

/** 
 * 
 * This is similar to popstyle, except that it removes the
 * style tag given from anywhere in the style stack, and
 * not just at the top.
 *
 * @update  gess 01/26/00
 */
void nsDTDContext::RemoveStyle(eHTMLTags aTag){
  
  PRInt32 theLevel=mStack.mCount;
  
  while (theLevel) {
    nsEntryStack *theStack=GetStylesAt(--theLevel);
    if (theStack) {
      PRInt32 index=theStack->mCount;
      while (index){
        nsTagEntry *theEntry=theStack->EntryAt(--index);
        if (aTag==(eHTMLTags)theEntry->mNode->GetNodeType()) {
          mResidualStyleCount--;
          nsCParserNode* result=theStack->Remove(index,aTag);
          IF_FREE(result, mNodeAllocator);  
          return;
        } 
      }
    } 
  }
}

/**
 * This gets called when the parser module is getting unloaded
 * 
 * @return  nada
 */
void nsDTDContext::ReleaseGlobalObjects(void){
}


/**************************************************************
  Now define the nsTokenAllocator class...
 **************************************************************/

static const size_t  kTokenBuckets[]       ={sizeof(CStartToken),sizeof(CAttributeToken),sizeof(CCommentToken),sizeof(CEndToken)};
static const PRInt32 kNumTokenBuckets      = sizeof(kTokenBuckets) / sizeof(size_t);
static const PRInt32 kInitialTokenPoolSize = NS_SIZE_IN_HEAP(sizeof(CToken)) * 200;

/**
 * 
 * @update  gess7/25/98
 * @param 
 */
nsTokenAllocator::nsTokenAllocator() {

  MOZ_COUNT_CTOR(nsTokenAllocator);

  mArenaPool.Init("TokenPool", kTokenBuckets, kNumTokenBuckets, kInitialTokenPoolSize);

#ifdef NS_DEBUG
  int i=0;
  for(i=0;i<eToken_last-1;++i) {
    mTotals[i]=0;
  }
#endif

}

/**
 * Destructor for the token factory
 * @update  gess7/25/98
 */
nsTokenAllocator::~nsTokenAllocator() {

  MOZ_COUNT_DTOR(nsTokenAllocator);

}

class CTokenFinder: public nsDequeFunctor{
public:
  CTokenFinder(CToken* aToken) {mToken=aToken;}
  virtual void* operator()(void* anObject) {
    if(anObject==mToken) {
      return anObject;
    }
    return 0;
  }
  CToken* mToken;
};

/**
 * Let's get this code ready to be reused by all the contexts.
 * 
 * @update	rickg 12June2000
 * @param   aType -- tells you the type of token to create
 * @param   aTag  -- tells you the type of tag to init with this token
 * @param   aString -- gives a default string value for the token
 *
 * @return  ptr to new token (or 0).
 */
CToken* nsTokenAllocator::CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag, const nsAString& aString) {

  CToken* result=0;

#ifdef  NS_DEBUG
    mTotals[aType-1]++;
#endif
  switch(aType){
    case eToken_start:            result=new(mArenaPool) CStartToken(aString, aTag); break;
    case eToken_end:              result=new(mArenaPool) CEndToken(aString, aTag); break;
    case eToken_comment:          result=new(mArenaPool) CCommentToken(aString); break;
    case eToken_entity:           result=new(mArenaPool) CEntityToken(aString); break;
    case eToken_whitespace:       result=new(mArenaPool) CWhitespaceToken(aString); break;
    case eToken_newline:          result=new(mArenaPool) CNewlineToken(); break;
    case eToken_text:             result=new(mArenaPool) CTextToken(aString); break;
    case eToken_attribute:        result=new(mArenaPool) CAttributeToken(aString); break;
    case eToken_instruction:      result=new(mArenaPool) CInstructionToken(aString); break;
    case eToken_cdatasection:     result=new(mArenaPool) CCDATASectionToken(aString); break;
    case eToken_doctypeDecl:      result=new(mArenaPool) CDoctypeDeclToken(aString); break;
    case eToken_markupDecl:       result=new(mArenaPool) CMarkupDeclToken(aString); break;
      default:
        NS_ASSERTION(PR_FALSE, "nsDTDUtils::CreateTokenOfType: illegal token type"); 
        break;
  }

  return result;
}

/**
 * Let's get this code ready to be reused by all the contexts.
 * 
 * @update	rickg 12June2000
 * @param   aType -- tells you the type of token to create
 * @param   aTag  -- tells you the type of tag to init with this token
 *
 * @return  ptr to new token (or 0).
 */
CToken* nsTokenAllocator::CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag) {

  CToken* result=0;

#ifdef  NS_DEBUG
    mTotals[aType-1]++;
#endif
  switch(aType){
    case eToken_start:            result=new(mArenaPool) CStartToken(aTag); break;
    case eToken_end:              result=new(mArenaPool) CEndToken(aTag); break;
    case eToken_comment:          result=new(mArenaPool) CCommentToken(); break;
    case eToken_attribute:        result=new(mArenaPool) CAttributeToken(); break;
    case eToken_entity:           result=new(mArenaPool) CEntityToken(); break;
    case eToken_whitespace:       result=new(mArenaPool) CWhitespaceToken(); break;
    case eToken_newline:          result=new(mArenaPool) CNewlineToken(); break;
    case eToken_text:             result=new(mArenaPool) CTextToken(); break;
    case eToken_instruction:      result=new(mArenaPool) CInstructionToken(); break;
    case eToken_cdatasection:     result=new(mArenaPool) CCDATASectionToken(aTag); break;
    case eToken_doctypeDecl:      result=new(mArenaPool) CDoctypeDeclToken(aTag); break;
    case eToken_markupDecl:       result=new(mArenaPool) CMarkupDeclToken(); break;
    default:
      NS_ASSERTION(PR_FALSE, "nsDTDUtils::CreateTokenOfType: illegal token type"); 
      break;
   }

  return result;
}

#ifdef DEBUG_TRACK_NODES 

static nsCParserNode* gAllNodes[100];
static int gAllNodeCount=0;

int FindNode(nsCParserNode *aNode) {
  int theIndex=0;
  for(theIndex=0;theIndex<gAllNodeCount;++theIndex) {
    if(gAllNodes[theIndex]==aNode) {
      return theIndex;
    }
  }
  return -1;
}

void AddNode(nsCParserNode *aNode) {
  if(-1==FindNode(aNode)) {
    gAllNodes[gAllNodeCount++]=aNode;
  }
  else {
    //you tried to recycle a node twice!
  }
}

void RemoveNode(nsCParserNode *aNode) {
  int theIndex=FindNode(aNode);
  if(-1<theIndex) {
    gAllNodes[theIndex]=gAllNodes[--gAllNodeCount];
  }
}

#endif 


#ifdef HEAP_ALLOCATED_NODES
nsNodeAllocator::nsNodeAllocator():mSharedNodes(0){
#ifdef DEBUG_TRACK_NODES
  mCount=0;
#endif
#else 
  static const size_t  kNodeBuckets[]       = { sizeof(nsCParserNode), sizeof(nsCParserStartNode) };
  static const PRInt32 kNumNodeBuckets      = sizeof(kNodeBuckets) / sizeof(size_t);
  static const PRInt32 kInitialNodePoolSize = NS_SIZE_IN_HEAP(sizeof(nsCParserNode)) * 35; // optimal size based on space-trace data
nsNodeAllocator::nsNodeAllocator() {
  mNodePool.Init("NodePool", kNodeBuckets, kNumNodeBuckets, kInitialNodePoolSize);
#endif
  MOZ_COUNT_CTOR(nsNodeAllocator);
}
  
nsNodeAllocator::~nsNodeAllocator() {
  MOZ_COUNT_DTOR(nsNodeAllocator);

#ifdef HEAP_ALLOCATED_NODES
  nsCParserNode* theNode = 0;

  while((theNode=(nsCParserNode*)mSharedNodes.Pop())){
#ifdef DEBUG_TRACK_NODES
    RemoveNode(theNode);
#endif
    ::operator delete(theNode); 
    theNode=nsnull;
  }
#ifdef DEBUG_TRACK_NODES
  if(mCount) {
    printf("**************************\n");
    printf("%i out of %i nodes leaked!\n",gAllNodeCount,mCount);
    printf("**************************\n");
  }
#endif
#endif
}
  
nsCParserNode* nsNodeAllocator::CreateNode(CToken* aToken,  
                                           nsTokenAllocator* aTokenAllocator) 
{
  nsCParserNode* result = 0;
#ifdef HEAP_ALLOCATED_NODES
#if 0
  if(gAllNodeCount!=mSharedNodes.GetSize()) {
    int x=10; //this is very BAD!
  }
#endif
  result = static_cast<nsCParserNode*>(mSharedNodes.Pop());
  if (result) {
    result->Init(aToken, aTokenAllocator,this);
  }
  else{
    result = nsCParserNode::Create(aToken, aTokenAllocator,this);
#ifdef DEBUG_TRACK_NODES
    ++mCount;
    AddNode(static_cast<nsCParserNode*>(result));
#endif
    IF_HOLD(result);
  }
#else
  eHTMLTokenTypes type = aToken ? eHTMLTokenTypes(aToken->GetTokenType()) : eToken_unknown;
  switch (type) {
    case eToken_start:
      result = nsCParserStartNode::Create(aToken, aTokenAllocator,this); 
      break;
    default :
      result = nsCParserNode::Create(aToken, aTokenAllocator,this); 
      break;
  }
  IF_HOLD(result);
#endif
  return result;
}

#ifdef DEBUG
void DebugDumpContainmentRules(nsIDTD& theDTD,const char* aFilename,const char* aTitle) {
}
#endif

/**************************************************************
  This defines the topic object used by the observer service.
  The observerService uses a list of these, 1 per topic when
  registering tags.
 **************************************************************/
NS_IMPL_ISUPPORTS1(nsObserverEntry, nsIObserverEntry)

nsObserverEntry::nsObserverEntry(const nsAString& aTopic) : mTopic(aTopic) 
{
  memset(mObservers, 0, sizeof(mObservers));
}

nsObserverEntry::~nsObserverEntry() {
  for (PRInt32 i = 0; i <= NS_HTML_TAG_MAX; ++i){
    delete mObservers[i];
  }
}

NS_IMETHODIMP
nsObserverEntry::Notify(nsIParserNode* aNode,
                        nsIParser* aParser,
                        nsISupports* aDocShell,
                        const PRUint32 aFlags) 
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aParser);

  nsresult result = NS_OK;
  eHTMLTags theTag = (eHTMLTags)aNode->GetNodeType();
 
  if (theTag <= NS_HTML_TAG_MAX) {
    nsCOMArray<nsIElementObserver>* theObservers = mObservers[theTag];
    if (theObservers) {
      PRInt32   theCharsetSource;
      nsCAutoString      charset;
      aParser->GetDocumentCharset(charset,theCharsetSource);
      NS_ConvertASCIItoUTF16 theCharsetValue(charset);

      PRInt32 theAttrCount = aNode->GetAttributeCount(); 
      PRInt32 theObserversCount = theObservers->Count();
      if (0 < theObserversCount){
        nsTArray<nsString> keys(theAttrCount + 4), values(theAttrCount + 4);

        // XXX this and the following code may be a performance issue.
        // Every key and value is copied and added to an voidarray (causing at
        // least 2 allocations for mImpl, usually more, plus at least 1 per
        // string (total = 2*(keys+3) + 2(or more) array allocations )).
        PRInt32 index;
        for (index = 0; index < theAttrCount; ++index) {
          keys.AppendElement(aNode->GetKeyAt(index));
          values.AppendElement(aNode->GetValueAt(index));
        } 

        nsAutoString intValue;

        keys.AppendElement(NS_LITERAL_STRING("charset")); 
        values.AppendElement(theCharsetValue);       
      
        keys.AppendElement(NS_LITERAL_STRING("charsetSource")); 
        intValue.AppendInt(PRInt32(theCharsetSource),10);
        values.AppendElement(intValue); 

        keys.AppendElement(NS_LITERAL_STRING("X_COMMAND"));
        values.AppendElement(NS_LITERAL_STRING("text/html")); 

        nsCOMPtr<nsIChannel> channel;
        aParser->GetChannel(getter_AddRefs(channel));

        for (index=0;index<theObserversCount;++index) {
          nsIElementObserver* observer = theObservers->ObjectAt(index);
          if (observer) {
            result = observer->Notify(aDocShell, channel,
                                      nsHTMLTags::GetStringValue(theTag),
                                      &keys, &values, aFlags);
            if (NS_FAILED(result)) {
              break;
            }

            if (result == NS_HTMLPARSER_VALID_META_CHARSET) {
              // Inform the parser that this meta tag contained a valid
              // charset. See bug 272815
              aParser->SetDocumentCharset(charset, kCharsetFromMetaTag);
              result = NS_OK;
            }
          }
        } 
      } 
    }
  }
  return result;
}

PRBool 
nsObserverEntry::Matches(const nsAString& aString) {
  PRBool result = aString.Equals(mTopic);
  return result;
}

nsresult
nsObserverEntry::AddObserver(nsIElementObserver *aObserver,
                             eHTMLTags aTag) 
{
  if (aObserver) {
    if (!mObservers[aTag]) {
      mObservers[aTag] = new nsCOMArray<nsIElementObserver>();
      if (!mObservers[aTag]) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    mObservers[aTag]->AppendObject(aObserver);
  }
  return NS_OK;
}

void 
nsObserverEntry::RemoveObserver(nsIElementObserver *aObserver)
{
  for (PRInt32 i=0; i <= NS_HTML_TAG_MAX; ++i){
    if (mObservers[i]) {
      mObservers[i]->RemoveObject(aObserver);
    }
  }
}
