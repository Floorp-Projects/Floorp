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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "nsDeque.h"
#include "nsCRT.h"
#include <stdio.h>

/**************************************************************
  Now define the token deallocator class...
 **************************************************************/
class _TestDeque: public nsDequeFunctor{
public:
  _TestDeque() {
    SelfTest();
  }
  int SelfTest();
  nsresult OriginalTest();
  nsresult OriginalFlaw();
  nsresult AssignFlaw();
  nsresult StupidIterations();
  virtual void* operator()(void* aObject) {
    return 0;
  }
};
static _TestDeque gDeallocator;

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
  results+=StupidIterations();
  return results;
}

nsresult _TestDeque::OriginalTest() {
  int ints[200];
  int count=sizeof(ints)/sizeof(int);
  int i=0;
  int* temp;
  nsDeque theDeque(&gDeallocator); //construct a simple one...
 
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
  int count=sizeof(ints)/sizeof(int);
  int i=0;
  int* temp;
  nsDeque secondDeque(&gDeallocator);
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
  int ints[200];
  int count=sizeof(ints)/sizeof(int);
  int i=0;
  nsDeque src(&gDeallocator),dest(&gDeallocator);
  /**
   * Test 2. Assignment doesn't do the right things.
   */
  printf("fill array\n");
  for (i=32; i; --i)
    ints[i]=i*3+7;
  printf("push 10 times\n"); //Capacity => 16
  for (i=0; i<10; i++)
    src.Push(&ints[i]);
  nsDequeIterator first(src.Begin()), second(src.End());
  nsDequeIterator third(dest.Begin()), fourth=dest.Begin();
  char sF[]="failure: ";
  char s1[]="first [ src.Begin]";
  char s2[]="second[ src.End  ]";
  char s3[]="third [dest.Begin]";
  char s4[]="fourth[dest.Begin]";
  if (first ==second) printf("%s%s==%s",sF, s1, s2);
  if (first ==third ) printf("%s%s==%s",sF, s1, s3);
  if (first ==fourth) printf("%s%s==%s",sF, s1, s4);
  if (second==third ) printf("%s%s==%s",sF, s2, s3);
  if (second==fourth) printf("%s%s==%s",sF, s2, s4);
  if (third !=fourth) printf("%s%s!=%s",sF, s3, s4);
  return NS_OK;
}

nsresult _TestDeque::StupidIterations() {
  /**
   * Transaction manager defined a peek which insisted on
   * (a) doing its own peek
   * (b) peeking at an empty deque
   */
  nsDeque stupid(&gDeallocator);
  stupid.End()++;
  ++stupid.End();
  stupid.End()--;
  --stupid.End();
  stupid.Begin()++;
  ++stupid.Begin();
  stupid.Begin()--;
  --stupid.Begin();
  return NS_OK;
}

int main (void){
  _TestDeque test;
  return 0;
}
