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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsAVLTree.h"
 

enum eLean      {eLeft,eNeutral,eRight};

struct NS_COM nsAVLNode {
public:

  nsAVLNode(void* aValue) {
    mLeft=0;
    mRight=0;
    mSkew=eNeutral;
    mValue=aValue;
  }

  nsAVLNode*  mLeft;
  nsAVLNode*  mRight;
  eLean       mSkew;
  void*       mValue;
};


/************************************************************
  Now begin the tree class. Don't forget that the comparison
  between nodes must occur via the comparitor function, 
  otherwise all you're testing is pointer addresses.
 ************************************************************/

/** ------------------------------------------------
 * 
 * 
 * @update gess 4/22/98
 * @param  
 * @return 
 */ //----------------------------------------------
nsAVLTree::nsAVLTree(nsAVLNodeComparitor& aComparitor, 
                     nsAVLNodeFunctor* aDeallocator) :
  mComparitor(aComparitor), mDeallocator(aDeallocator) {
  mRoot=0;
  mCount=0;
}


static void 
avlDeleteTree(nsAVLNode* aNode){
  if (aNode) {
    avlDeleteTree(aNode->mLeft);
    avlDeleteTree(aNode->mRight);
    delete aNode;
  }
}

/**
 * 
 * @update	gess12/27/98
 * @param 
 * @return
 */
nsAVLTree::~nsAVLTree(){
  if (mDeallocator) {
    ForEachDepthFirst(*mDeallocator);
  }
  avlDeleteTree(mRoot);
}


class CDoesntExist: public nsAVLNodeFunctor {
public:
  CDoesntExist(const nsAVLTree& anotherTree) : mOtherTree(anotherTree) {
  }
  virtual void* operator()(void* anItem) {
    void* result=mOtherTree.FindItem(anItem);
    if(result)
      return nsnull;
    return anItem;
  }
protected:
  const nsAVLTree& mOtherTree;
};

/**
 * This method compares two trees (members by identity).
 * @update	gess12/27/98
 * @param   tree to compare against
 * @return  true if they are identical (contain same stuff).
 */
PRBool nsAVLTree::operator==(const nsAVLTree& aCopy) const{
  CDoesntExist functor(aCopy);
  void* theItem=FirstThat(functor);
  PRBool result=PRBool(!theItem);
  return result;
}


/**
 * 
 * @update	gess12/27/98
 * @param 
 * @return
 */
static void 
avlRotateRight(nsAVLNode*& aRootNode){
  nsAVLNode* ptr2;
  nsAVLNode* ptr3;

  ptr2=aRootNode->mRight;
  if(ptr2->mSkew==eRight) {
    aRootNode->mRight=ptr2->mLeft;
    ptr2->mLeft=aRootNode;
    aRootNode->mSkew=eNeutral;
    aRootNode=ptr2;
  }
  else {
    ptr3=ptr2->mLeft;
    ptr2->mLeft=ptr3->mRight;
    ptr3->mRight=ptr2;
    aRootNode->mRight=ptr3->mLeft;
    ptr3->mLeft=aRootNode;
    if(ptr3->mSkew==eLeft)
      ptr2->mSkew=eRight;
    else ptr2->mSkew=eNeutral;
    if(ptr3->mSkew==eRight)
      aRootNode->mSkew=eLeft;
    else aRootNode->mSkew=eNeutral;
    aRootNode=ptr3;
  }
  aRootNode->mSkew=eNeutral;
  return;
}



/**
 * 
 * @update	gess12/27/98
 * @param 
 * @return
 */
static void 
avlRotateLeft(nsAVLNode*& aRootNode){
  nsAVLNode* ptr2;
  nsAVLNode* ptr3;

  ptr2=aRootNode->mLeft;
  if(ptr2->mSkew==eLeft) {
    aRootNode->mLeft=ptr2->mRight;
    ptr2->mRight=aRootNode;
    aRootNode->mSkew=eNeutral;
    aRootNode=ptr2;
  }
  else {
    ptr3=ptr2->mRight;
    ptr2->mRight=ptr3->mLeft;
    ptr3->mLeft=ptr2;
    aRootNode->mLeft=ptr3->mRight;
    ptr3->mRight=aRootNode;
    if(ptr3->mSkew==eRight)
      ptr2->mSkew=eLeft;
    else ptr2->mSkew=eNeutral;
    if(ptr3->mSkew==eLeft)
      aRootNode->mSkew=eRight;
    else aRootNode->mSkew=eNeutral;
    aRootNode=ptr3;
  }
  aRootNode->mSkew=eNeutral;
  return;
}


/** ------------------------------------------------
 * 
 * 
 * @update gess 4/22/98
 * @param  
 * @return 
 */ //----------------------------------------------
static eAVLStatus 
avlInsert(nsAVLNode*& aRootNode, nsAVLNode* aNewNode, 
          nsAVLNodeComparitor& aComparitor) {
  eAVLStatus result=eAVL_unknown;
  
  if(!aRootNode) {
    aRootNode = aNewNode;
    return eAVL_ok;
  }
  
  if(aNewNode==aRootNode->mValue) {
    return eAVL_duplicate;
  }

  PRInt32 theCompareResult=aComparitor(aRootNode->mValue,aNewNode->mValue);
  if(0 < theCompareResult) { //if(anItem<aRootNode->mValue)
    result=avlInsert(aRootNode->mLeft,aNewNode,aComparitor);
    if(eAVL_ok==result) {
      switch(aRootNode->mSkew){
        case eLeft:
          avlRotateLeft(aRootNode);
          result=eAVL_fail;
          break;
        case eRight:
          aRootNode->mSkew=eNeutral;
          result=eAVL_fail;
          break;
        case eNeutral:
          aRootNode->mSkew=eLeft;
          break;
      } //switch
    }//if
  } //if
  else { 
    result=avlInsert(aRootNode->mRight,aNewNode,aComparitor);
    if(eAVL_ok==result) {
      switch(aRootNode->mSkew){
        case eLeft:
          aRootNode->mSkew=eNeutral;
          result=eAVL_fail;
          break;
        case eRight:
          avlRotateRight(aRootNode);
          result=eAVL_fail;
          break;
        case eNeutral:
          aRootNode->mSkew=eRight;
          break;
      } //switch
    }
  } //if
  return result;
}

/** ------------------------------------------------
 * 
 * 
 * @update gess 4/22/98
 * @param  
 * @return 
 */ //----------------------------------------------
static void 
avlBalanceLeft(nsAVLNode*& aRootNode, PRBool& delOk){
  nsAVLNode*  ptr2;
  nsAVLNode*  ptr3;
  eLean       balnc2;
  eLean       balnc3;

  switch(aRootNode->mSkew){
    case eLeft:
      ptr2=aRootNode->mLeft;
      balnc2=ptr2->mSkew;
      if(balnc2!=eRight) {
        aRootNode->mLeft=ptr2->mRight;
        ptr2->mRight=aRootNode;
        if(balnc2==eNeutral){
          aRootNode->mSkew=eLeft;
          ptr2->mSkew=eRight;
          delOk=PR_FALSE;
        }
        else{
          aRootNode->mSkew=eNeutral;
          ptr2->mSkew=eNeutral;
        }
        aRootNode=ptr2;
      }
      else{
        ptr3=ptr2->mRight;
        balnc3=ptr3->mSkew;
        ptr2->mRight=ptr3->mLeft;
        ptr3->mLeft=ptr2;
        aRootNode->mLeft=ptr3->mRight;
        ptr3->mRight=aRootNode;
        if(balnc3==eRight) {
          ptr2->mSkew=eLeft;
        }
        else {
          ptr2->mSkew=eNeutral;
        }
        if(balnc3==eLeft) {
          aRootNode->mSkew=eRight;
        }
        else {
          aRootNode->mSkew=eNeutral;
        }
        aRootNode=ptr3;
        ptr3->mSkew=eNeutral;
      }
      break;

    case eRight:
      aRootNode->mSkew=eNeutral;
      break;
    
    case eNeutral:
      aRootNode->mSkew=eLeft;
      delOk=PR_FALSE;
      break;
  }//switch
  return;
}

/** ------------------------------------------------
 * 
 * 
 * @update gess 4/22/98
 * @param  
 * @return 
 */ //----------------------------------------------
static void 
avlBalanceRight(nsAVLNode*& aRootNode, PRBool& delOk){
  nsAVLNode*  ptr2;
  nsAVLNode*  ptr3;
  eLean       balnc2;
  eLean       balnc3;

  switch(aRootNode->mSkew){
    case eLeft:
      aRootNode->mSkew=eNeutral;
      break;

    case eRight:
      ptr2=aRootNode->mRight;
      balnc2=ptr2->mSkew;
      if(balnc2!=eLeft) {
        aRootNode->mRight=ptr2->mLeft;
        ptr2->mLeft=aRootNode;
        if(balnc2==eNeutral){
          aRootNode->mSkew=eRight;
          ptr2->mSkew=eLeft;
          delOk=PR_FALSE;
        }
        else{
          aRootNode->mSkew=eNeutral;
          ptr2->mSkew=eNeutral;
        }
        aRootNode=ptr2;
      }
      else{
        ptr3=ptr2->mLeft;
        balnc3=ptr3->mSkew;
        ptr2->mLeft=ptr3->mRight;
        ptr3->mRight=ptr2;
        aRootNode->mRight=ptr3->mLeft;
        ptr3->mLeft=aRootNode;
        if(balnc3==eLeft) {
          ptr2->mSkew=eRight;
        }
        else {
          ptr2->mSkew=eNeutral;
        }
        if(balnc3==eRight) {
          aRootNode->mSkew=eLeft;
        }
        else {
          aRootNode->mSkew=eNeutral;
        }
        aRootNode=ptr3;
        ptr3->mSkew=eNeutral;
      }
      break;

    case eNeutral:
      aRootNode->mSkew=eRight;
      delOk=PR_FALSE;
      break;
  }//switch
  return;
}

/** ------------------------------------------------
 * 
 * 
 * @update gess 4/22/98
 * @param  
 * @return 
 */ //----------------------------------------------
static eAVLStatus 
avlRemoveChildren(nsAVLNode*& aRootNode,nsAVLNode*& anotherNode, PRBool& delOk){
  eAVLStatus result=eAVL_ok;
  
  if(!anotherNode->mRight){
    aRootNode->mValue=anotherNode->mValue; //swap
    anotherNode=anotherNode->mLeft;
    delOk=PR_TRUE;
  }
  else{
    avlRemoveChildren(aRootNode,anotherNode->mRight,delOk);
    if(delOk)
      avlBalanceLeft(anotherNode,delOk);
  }

  return result;
}


/** ------------------------------------------------
 * 
 * 
 * @update gess 4/22/98
 * @param  
 * @return 
 */ //----------------------------------------------
static eAVLStatus 
avlRemove(nsAVLNode*& aRootNode, void* anItem, PRBool& delOk,
          nsAVLNodeComparitor& aComparitor){
  eAVLStatus result=eAVL_ok;

  if(!aRootNode)
    delOk=PR_FALSE;
  else {
    PRInt32 cmp=aComparitor(anItem,aRootNode->mValue);
    if(cmp<0){
      avlRemove(aRootNode->mLeft,anItem,delOk,aComparitor);
      if(delOk)
        avlBalanceRight(aRootNode,delOk);
    }
    else if(cmp>0){
      avlRemove(aRootNode->mRight,anItem,delOk,aComparitor);
      if(delOk)
        avlBalanceLeft(aRootNode,delOk);
    }
    else{ //they match...
      nsAVLNode* temp=aRootNode;
      if(!aRootNode->mRight) {
        aRootNode=aRootNode->mLeft;
        delOk=PR_TRUE;
        delete temp;
      }
      else if(!aRootNode->mLeft) {
        aRootNode=aRootNode->mRight;
        delOk=PR_TRUE;
        delete temp;
      }
      else {
        avlRemoveChildren(aRootNode,aRootNode->mLeft,delOk);
        if(delOk)
          avlBalanceRight(aRootNode,delOk);
      }
    }
  }

  return result;
}

/** ------------------------------------------------
 * 
 * 
 * @update gess 4/22/98
 * @param  
 * @return 
 */ //----------------------------------------------
eAVLStatus 
nsAVLTree::AddItem(void* anItem){
  eAVLStatus result=eAVL_ok;

  nsAVLNode* theNewNode=new nsAVLNode(anItem);
  result=avlInsert(mRoot,theNewNode,mComparitor);
  if(eAVL_duplicate!=result)
    mCount++;
  else {
    delete theNewNode;
  }
  return result;
}

/** ------------------------------------------------
 * 
 * 
 * @update gess 4/22/98
 * @param  
 * @return 
 */ //----------------------------------------------
void* nsAVLTree::FindItem(void* aValue) const{
  nsAVLNode* result=mRoot;
  PRInt32 count=0;
  while(result) {
    count++;
    PRInt32 cmp=mComparitor(aValue,result->mValue);
    if(0==cmp) {
      //we matched...
      break;
    }
    else if(0>cmp){
      //theNode was greater...
      result=result->mLeft;
    }
    else {
      //aValue is greater...
      result=result->mRight;
    }
  }
  if(result) {
    return result->mValue;
  }
  return nsnull;
}


/**
 * 
 * @update	gess12/30/98
 * @param 
 * @return
 */
eAVLStatus 
nsAVLTree::RemoveItem(void* aValue){
  PRBool delOk=PR_TRUE;
  eAVLStatus result=avlRemove(mRoot,aValue,delOk,mComparitor);
  if(eAVL_ok==result)
    mCount--;
  return result;
} 

/**
 * 
 * @update	gess9/11/98
 * @param 
 * @return
 */
static void 
avlForEachDepthFirst(nsAVLNode* aNode, nsAVLNodeFunctor& aFunctor){
  if(aNode) {
    avlForEachDepthFirst(aNode->mLeft,aFunctor);
    avlForEachDepthFirst(aNode->mRight,aFunctor);
    aFunctor(aNode->mValue);
  }
}


/**
 * 
 * @update	gess9/11/98
 * @param 
 * @return
 */
void 
nsAVLTree::ForEachDepthFirst(nsAVLNodeFunctor& aFunctor) const{
  ::avlForEachDepthFirst(mRoot,aFunctor);
}

/**
 * 
 * @update	gess9/11/98
 * @param 
 * @return
 */
static void 
avlForEach(nsAVLNode* aNode, nsAVLNodeFunctor& aFunctor) {
  if(aNode) {
    avlForEach(aNode->mLeft,aFunctor);
    aFunctor(aNode->mValue);
    avlForEach(aNode->mRight,aFunctor);
  }
}


/**
 * 
 * @update	gess9/11/98
 * @param 
 * @return
 */
void 
nsAVLTree::ForEach(nsAVLNodeFunctor& aFunctor) const{
  ::avlForEach(mRoot,aFunctor);
}


/**
 * 
 * @update	gess9/11/98
 * @param 
 * @return
 */
static void* 
avlFirstThat(nsAVLNode* aNode, nsAVLNodeFunctor& aFunctor) {
  void* result=nsnull;
  if(aNode) {
    result = avlFirstThat(aNode->mLeft,aFunctor);
    if (result) {
      return result;
    }
    result = aFunctor(aNode->mValue);
    if (result) {
      return result;
    }
    result = avlFirstThat(aNode->mRight,aFunctor);
  }
  return result;
}

/**
 * 
 * @update	gess9/11/98
 * @param 
 * @return
 */
void* 
nsAVLTree::FirstThat(nsAVLNodeFunctor& aFunctor) const{
  return ::avlFirstThat(mRoot,aFunctor);
}

