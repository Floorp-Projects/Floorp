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

#ifndef nsAVLTree_h___
#define nsAVLTree_h___


#include "nscore.h"


enum eAVLStatus {eAVL_unknown,eAVL_ok,eAVL_fail,eAVL_duplicate};


struct nsAVLNode;

/**
 * 
 * @update	gess12/26/98
 * @param anObject1 is the first object to be compared
 * @param anObject2 is the second object to be compared
 * @return -1,0,1 if object1 is less, equal, greater than object2
 */
class NS_COM nsAVLNodeComparitor {
public:
  virtual PRInt32 operator()(void* anItem1,void* anItem2)=0;
}; 

class NS_COM nsAVLNodeFunctor {
public:
  virtual void* operator()(void* anItem)=0;
};

class NS_COM nsAVLTree {
public:
              nsAVLTree(nsAVLNodeComparitor& aComparitor, nsAVLNodeFunctor* aDeallocator);
              ~nsAVLTree(void);

  PRBool      operator==(const nsAVLTree& aOther) const;
  PRInt32     GetCount(void) const {return mCount;}

              //main functions...
  eAVLStatus  AddItem(void* anItem);
  eAVLStatus  RemoveItem(void* anItem);
  void*       FindItem(void* anItem) const;
  void        ForEach(nsAVLNodeFunctor& aFunctor) const;
  void        ForEachDepthFirst(nsAVLNodeFunctor& aFunctor) const;
  void*       FirstThat(nsAVLNodeFunctor& aFunctor) const;

protected: 

  nsAVLNode*  mRoot;
  PRInt32     mCount;
  nsAVLNodeComparitor&  mComparitor;
  nsAVLNodeFunctor*     mDeallocator;
};


#endif /* nsAVLTree_h___ */

