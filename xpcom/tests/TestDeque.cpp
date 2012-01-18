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

#include "nsDeque.h"
#include "nsCRT.h"
#include <stdio.h>

/**************************************************************
  Now define the token deallocator class...
 **************************************************************/
class _TestDeque {
public:
  _TestDeque() {
    SelfTest();
  }
  int SelfTest();
  nsresult OriginalTest();
  nsresult OriginalFlaw();
  nsresult AssignFlaw();
};
static _TestDeque sTestDeque;

class _Dealloc: public nsDequeFunctor {
  virtual void* operator()(void* aObject) {
    return 0;
  }
};

/**
 * conduct automated self test for this class
 *
 * @param
 * @return
 */
int _TestDeque::SelfTest() {
  /* the old deque should have failed a bunch of these tests */
  int results=0;
  results+=OriginalTest();
  results+=OriginalFlaw();
  results+=AssignFlaw();
  return results;
}

nsresult _TestDeque::OriginalTest() {
  int ints[200];
  int count=sizeof(ints)/sizeof(int);
  int i=0;
  int* temp;
  nsDeque theDeque(new _Dealloc); //construct a simple one...
 
  for (i=0;i<count;i++) { //initialize'em
    ints[i]=10*(1+i);
  }
  for (i=0;i<70;i++) {
    theDeque.Push(&ints[i]);
  }
  for (i=0;i<56;i++) {
    temp=(int*)theDeque.Pop();
  }
  for (i=0;i<55;i++) {
    theDeque.Push(&ints[i]);
  }
  for (i=0;i<35;i++) {
    temp=(int*)theDeque.Pop();
  }
  for (i=0;i<35;i++) {
    theDeque.Push(&ints[i]);
  }
  for (i=0;i<38;i++) {
    temp=(int*)theDeque.Pop();
  }
  return NS_OK;
}

nsresult _TestDeque::OriginalFlaw() {
  int ints[200];
  int i=0;
  int* temp;
  nsDeque secondDeque(new _Dealloc);
  /**
   * Test 1. Origin near end, semi full, call Peek().
   * you start, mCapacity is 8
   */
  printf("fill array\n");
  for (i=32; i; --i)
    ints[i]=i*3+10;
  printf("push 6 times\n");
  for (i=0; i<6; i++)
    secondDeque.Push(&ints[i]);
  printf("popfront 4 times:\n");
  for (i=4; i; --i) {
    temp=(int*)secondDeque.PopFront();
    printf("%d\t",*temp);
  }
  printf("push 4 times\n");
  for (int j=4; j; --j)
    secondDeque.Push(&ints[++i]);
  printf("origin should now be about 4\n");
  printf("and size should be 6\n");
  printf("origin+size>capacity\n");

   /*<akk> Oh, I see ... it's a circular buffer */
  printf("but the old code wasn't behaving accordingly.\n");

   /*right*/
  printf("we shouldn't crash or anything interesting, ");

  temp=(int*)secondDeque.Peek();
  printf("peek: %d\n",*temp);
  return NS_OK;
}

nsresult _TestDeque::AssignFlaw() {
  nsDeque src(new _Dealloc),dest(new _Dealloc);
  return NS_OK;
}

int main (void) {
  _TestDeque test;
  return 0;
}
