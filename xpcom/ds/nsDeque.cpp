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
#ifdef DEBUG_rickg
#include <stdio.h>
#endif

/**
 * 07/02/2001  09:17p 509,104 clangref.pdf from openwatcom's site
 * Watcom C Language Reference Edition 11.0c
 * page 118 of 297
 *
 * The % symbol yields the remainder from the division of the first operand
 * by the second operand. The operands of % must have integral type.
 *
 * When both operands of % are positive, the result is a positive value
 * smaller than the second operand. When one or both operands is negative,
 * whether the result is positive or negative is implementation-defined.
 *
 */
/* Ok, so first of all, C is underspecified. joy.
 * The following functions do not provide a correct implementation of modulus
 * They provide functionality for x>-y.
 * There are risks of 2*y being greater than max int, which is part of the
 * reason no multiplication is used and other operations are avoided.
 *
 * modasgn
 * @param x variable
 * @param y expression
 * approximately equivalent to x %= y
 *
 * modulus
 * @param x expression
 * @param y expression
 * approximately equivalent to x % y
 */
#define modasgn(x,y) if (x<0) x+=y; x%=y
#define modulus(x,y) ((x<0)?(x+y)%(y):(x)%(y))

MOZ_DECL_CTOR_COUNTER(nsDeque)

/**
 * Standard constructor
 * @param deallocator, called by Erase and ~nsDeque
 */
nsDeque::nsDeque(nsDequeFunctor* aDeallocator) {
  MOZ_COUNT_CTOR(nsDeque);
  mDeallocator=aDeallocator;
  mOrigin=mSize=0;
  mData=mBuffer; // don't allocate space until you must
  mCapacity=sizeof(mBuffer)/sizeof(mBuffer[0]);
  memset(mData, 0, mCapacity*sizeof(mBuffer[0]));
}

/**
 * Destructor
 */
nsDeque::~nsDeque() {
  MOZ_COUNT_DTOR(nsDeque);

#ifdef DEBUG_rickg
  char buffer[30];
  printf("Capacity: %i\n", mCapacity);

  static int mCaps[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  switch(mCapacity) {
    case 4:     mCaps[0]++; break;
    case 8:     mCaps[1]++; break;
    case 16:    mCaps[2]++; break;
    case 32:    mCaps[3]++; break;
    case 64:    mCaps[4]++; break;
    case 128:   mCaps[5]++; break;
    case 256:   mCaps[6]++; break;
    case 512:   mCaps[7]++; break;
    case 1024:  mCaps[8]++; break;
    case 2048:  mCaps[9]++; break;
    case 4096:  mCaps[10]++; break;
    default:
      break;
  }
#endif

  Erase();
  if (mData && (mData!=mBuffer)) {
    delete [] mData;
  }
  mData=0;
  SetDeallocator(0);
}

/**
 * Set the functor to be called by Erase()
 * The deque owns the functor.
 *
 * @param   aDeallocator functor object for use by Erase()
 */
void nsDeque::SetDeallocator(nsDequeFunctor* aDeallocator){
  if (mDeallocator) {
    delete mDeallocator;
  }
  mDeallocator=aDeallocator;
}

/**
 * Remove all items from container without destroying them.
 *
 * @return  *this
 */
nsDeque& nsDeque::Empty() {
  if (mSize && mData) {
    memset(mData, 0, mCapacity*sizeof(mData));
  }
  mSize=0;
  mOrigin=0;
  return *this;
}

/**
 * Remove and delete all items from container
 *
 * @return  *this
 */
nsDeque& nsDeque::Erase() {
  if (mDeallocator && mSize) {
    ForEach(*mDeallocator);
  }
  return Empty();
}

/**
 * This method quadruples the size of the deque
 * Elements in the deque are resequenced so that elements
 * in the deque are stored sequentially
 *
 * If the deque actually overflows, there's very little we can do.
 * Perhaps this function should return PRBool/nsresult indicating success/failure.
 *
 * @return  capacity of the deque
 *          If the deque did not grow,
 *          and you knew its capacity beforehand,
 *          then this would be a way to indicate the failure.
 */
PRInt32 nsDeque::GrowCapacity() {
  PRInt32 theNewSize=mCapacity<<2;
  NS_ASSERTION(theNewSize>mCapacity, "Overflow");
  if (theNewSize<=mCapacity)
    return mCapacity;
  void** temp=new void*[theNewSize];

  //Here's the interesting part: You can't just move the elements
  //directly (in situ) from the old buffer to the new one.
  //Since capacity has changed, the old origin doesn't make
  //sense anymore. It's better to resequence the elements now.

  if (temp) {
    PRInt32 tempi=0;
    PRInt32 i=0;
    PRInt32 j=0;
    for (i=mOrigin; i<mCapacity; i++) {
      temp[tempi++]=mData[i]; //copy the leading elements...
    }
    for (j=0;j<mOrigin;j++) {
      temp[tempi++]=mData[j]; //copy the trailing elements...
    }
    if (mData != mBuffer) {
      delete [] mData;
    }
    mCapacity=theNewSize;
    mOrigin=0; //now realign the origin...
    mData=temp;
  }
  return mCapacity;
}

/**
 * This method adds an item to the end of the deque.
 * This operation has the potential to cause the
 * underlying buffer to resize.
 *
 * @param   aItem: new item to be added to deque
 * @return  *this
 */
nsDeque& nsDeque::Push(void* aItem) {
  if (mSize==mCapacity) {
    GrowCapacity();
  }
  mData[modulus(mOrigin + mSize, mCapacity)]=aItem;
  mSize++;
  return *this;
}

/**
 * This method adds an item to the front of the deque.
 * This operation has the potential to cause the
 * underlying buffer to resize.
 *
 * --Commments for GrowCapacity() case
 * We've grown and shifted which means that the old
 * final element in the deque is now the first element
 * in the deque.  This is temporary.
 * We haven't inserted the new element at the front.
 *
 * To continue with the idea of having the front at zero
 * after a grow, we move the old final item (which through
 * the voodoo of mOrigin-- is now the first) to its final
 * position which is conveniently the old length.
 *
 * Note that this case only happens when the deque is full.
 * [And that pieces of this magic only work if the deque is full.]
 * picture:
 *   [ABCDEFGH] @[mOrigin:3]:D.
 * Task: PushFront("Z")
 * shift mOrigin so, @[mOrigin:2]:C
 * stretch and rearrange: (mOrigin:0)
 *   [CDEFGHAB ________ ________ ________]
 * copy: (The second C is currently out of bounds)
 *   [CDEFGHAB C_______ ________ ________]
 * later we will insert Z:
 *   [ZDEFGHAB C_______ ________ ________]
 * and increment size: 9. (C is no longer out of bounds)
 * --
 * @param   aItem: new item to be added to deque
 * @return  *this
 */
nsDeque& nsDeque::PushFront(void* aItem) {
  mOrigin--;
  modasgn(mOrigin,mCapacity);
  if (mSize==mCapacity) {
    GrowCapacity();
    /* Comments explaining this are above*/
    mData[mSize]=mData[mOrigin];
  }
  mData[mOrigin]=aItem;
  mSize++;
  return *this;
}

/**
 * Remove and return the last item in the container.
 *
 * @return  ptr to last item in container
 */
void* nsDeque::Pop() {
  void* result=0;
  if (mSize>0) {
    --mSize;
    PRInt32 offset=modulus(mSize + mOrigin, mCapacity);
    result=mData[offset];
    mData[offset]=0;
    if (!mSize) {
      mOrigin=0;
    }
  }
  return result;
}

/**
 * This method gets called you want to remove and return
 * the first member in the container.
 *
 * @return  last item in container
 */
void* nsDeque::PopFront() {
  void* result=0;
  if (mSize>0) {
    NS_ASSERTION(mOrigin < mCapacity, "Error: Bad origin");
    result=mData[mOrigin];
    mData[mOrigin++]=0;     //zero it out for debugging purposes.
    mSize--;
    // Cycle around if we pop off the end
    // and reset origin if when we pop the last element
    if (mCapacity==mOrigin || !mSize) {
      mOrigin=0;
    }
  }
  return result;
}

/**
 * This method gets called you want to peek at the bottom
 * member without removing it.
 *
 * @return  last item in container
 */
void* nsDeque::Peek() {
  void* result=0;
  if (mSize>0) {
    result = mData[modulus(mSize - 1 + mOrigin, mCapacity)];
  }
  return result;
} 

/**
 * This method gets called you want to peek at the topmost
 * member without removing it.
 *
 * @return  last item in container
 */
void* nsDeque::PeekFront() {
  void* result=0;
  if (mSize>0) {
    result=mData[mOrigin];
  }
  return result;
}

/**
 * Call this to retrieve the ith element from this container.
 * Keep in mind that accessing the underlying elements is
 * done in a relative fashion. Object 0 is not necessarily
 * the first element (the first element is at mOrigin).
 *
 * @param   aIndex : 0 relative offset of item you want
 * @return  void* or null
 */
void* nsDeque::ObjectAt(PRInt32 aIndex) const {
  void* result=0;
  if ((aIndex>=0) && (aIndex<mSize)) {
    result=mData[modulus(mOrigin + aIndex, mCapacity)];
  }
  return result;
}

/**
 * Create and return an iterator pointing to
 * the beginning of the queue. Note that this
 * takes the circular buffer semantics into account.
 *
 * @return  new deque iterator, init'ed to 1st item
 */
nsDequeIterator nsDeque::Begin() const{
  return nsDequeIterator(*this, 0);
}

/**
 * Create and return an iterator pointing to
 * the last item in the deque.
 * Note that this takes the circular buffer semantics
 * into account.
 *
 * @return  new deque iterator, init'ed to the last item
 */
nsDequeIterator nsDeque::End() const{
  return nsDequeIterator(*this, mSize - 1);
}

void* nsDeque::Last() const {
  return End().GetCurrent();
}

/**
 * Call this method when you want to iterate all the
 * members of the container, passing a functor along
 * to call your code.
 *
 * @param   aFunctor object to call for each member
 * @return  *this
 */
void nsDeque::ForEach(nsDequeFunctor& aFunctor) const{
  for (PRInt32 i=0; i<mSize; i++) {
    aFunctor(ObjectAt(i));
  }
}

/**
 * Call this method when you want to iterate all the
 * members of the container, calling the functor you 
 * passed with each member. This process will interrupt
 * if your function returns non 0 to this method.
 *
 * @param   aFunctor object to call for each member
 * @return  first nonzero result of aFunctor or 0.
 */
const void* nsDeque::FirstThat(nsDequeFunctor& aFunctor) const{
  for (PRInt32 i=0; i<mSize; i++) {
    void* obj=aFunctor(ObjectAt(i));
    if (obj) {
      return obj;
    }
  }
  return 0;
}

/******************************************************
 * Here comes the nsDequeIterator class...
 ******************************************************/

/**
 * DequeIterator is an object that knows how to iterate (forward and backward)
 * through a Deque. Normally, you don't need to do this, but there are some special
 * cases where it is pretty handy, so here you go.
 *
 * This is a standard dequeiterator constructor
 *
 * @param   aQueue is the deque object to be iterated
 * @param   aIndex is the starting position for your iteration
 */
nsDequeIterator::nsDequeIterator(const nsDeque& aQueue, int aIndex)
: mIndex(aIndex),
  mDeque(aQueue)
{
}

/**
 * Create a copy of a DequeIterator
 *
 * @param   aCopy is another iterator to copy from
 */
nsDequeIterator::nsDequeIterator(const nsDequeIterator& aCopy)
: mIndex(aCopy.mIndex),
  mDeque(aCopy.mDeque)
{
}

/**
 * Moves iterator to first element in deque
 * @return  *this
 */
nsDequeIterator& nsDequeIterator::First(){
  mIndex=0;
  return *this;
}

/**
 * Standard assignment operator for dequeiterator
 *
 * @param   aCopy is an iterator to be copied from
 * @return  *this
 */
nsDequeIterator& nsDequeIterator::operator=(const nsDequeIterator& aCopy) {
  NS_ASSERTION(&mDeque==&aCopy.mDeque,"you can't change the deque that an interator is iterating over, sorry.");
  mIndex=aCopy.mIndex;
  return *this;
}

/**
 * preform ! operation against to iterators to test for equivalence
 * (or lack thereof)!
 *
 * @param   aIter is the object to be compared to
 * @return  TRUE if NOT equal.
 */
PRBool nsDequeIterator::operator!=(nsDequeIterator& aIter) {
  return PRBool(!this->operator==(aIter));
}

/**
 * Compare two iterators for increasing order.
 *
 * @param   aIter is the other iterator to be compared to
 * @return  TRUE if this object points to an element before
 *          the element pointed to by aIter.
 *          FALSE if this and aIter are not iterating over the same deque.
 */
PRBool nsDequeIterator::operator<(nsDequeIterator& aIter) {
  return PRBool(((mIndex<aIter.mIndex) && (&mDeque==&aIter.mDeque)));
}

/**
 * Compare two iterators for equivalence.
 *
 * @param   aIter is the other iterator to be compared to
 * @return  TRUE if EQUAL
 */
PRBool nsDequeIterator::operator==(nsDequeIterator& aIter) {
  return PRBool(((mIndex==aIter.mIndex) && (&mDeque==&aIter.mDeque)));
}

/**
 * Compare two iterators for non strict decreasing order.
 *
 * @param   aIter is the other iterator to be compared to
 * @return  TRUE if this object points to the same element, or
 *          an element after the element pointed to by aIter.
 *          FALSE if this and aIter are not iterating over the same deque.
 */
PRBool nsDequeIterator::operator>=(nsDequeIterator& aIter) {
  return PRBool(((mIndex>=aIter.mIndex) && (&mDeque==&aIter.mDeque)));
}

/**
 * Pre-increment operator
 *
 * @return  object at post-incremented index
 */
void* nsDequeIterator::operator++() {
  NS_ASSERTION(mIndex<mDeque.mSize,
    "You have reached the end of the Internet."\
    "You have seen everything there is to see. Please go back. Now."
  );
#ifndef TIMELESS_LIGHTWEIGHT
  if (mIndex>=mDeque.mSize) return 0;
#endif
  return mDeque.ObjectAt(++mIndex);
}

/**
 * Post-increment operator
 *
 * @param   param is ignored
 * @return  object at pre-incremented index
 */
void* nsDequeIterator::operator++(int) {
  NS_ASSERTION(mIndex<=mDeque.mSize,
    "You have already reached the end of the Internet."\
    "You have seen everything there is to see. Please go back. Now."
  );
#ifndef TIMELESS_LIGHTWEIGHT
  if (mIndex>mDeque.mSize) return 0;
#endif
  return mDeque.ObjectAt(mIndex++);
}

/**
 * Pre-decrement operator
 *
 * @return  object at pre-decremented index
 */
void* nsDequeIterator::operator--() {
  NS_ASSERTION(mIndex>=0,
    "You have reached the beginning of the Internet."\
    "You have seen everything there is to see. Please go forward. Now."
  );
#ifndef TIMELESS_LIGHTWEIGHT
  if (mIndex<0) return 0;
#endif
  return mDeque.ObjectAt(--mIndex);
}

/**
 * Post-decrement operator
 *
 * @param   param is ignored
 * @return  object at post-decremented index
 */
void* nsDequeIterator::operator--(int) {
  NS_ASSERTION(mIndex>=0,
    "You have already reached the beginning of the Internet."\
    "You have seen everything there is to see. Please go forward. Now."
  );
#ifndef TIMELESS_LIGHTWEIGHT
  if (mIndex<0) return 0;
#endif
  return mDeque.ObjectAt(mIndex--);
}

/**
 * Dereference operator
 * Note that the iterator floats, so you don't need to do:
 * <code>++iter; aDeque.PopFront();</code>
 * Unless you actually want your iterator to jump 2 spaces.
 *
 * Picture: [1 2I 3 4]
 * PopFront()
 * Picture: [2 3I 4]
 * Note that I still happily points to object at the second index
 *
 * @return  object at ith index
 */
void* nsDequeIterator::GetCurrent() {
  NS_ASSERTION(mIndex<mDeque.mSize&&mIndex>=0,"Current is out of bounds");
#ifndef TIMELESS_LIGHTWEIGHT
  if (mIndex>=mDeque.mSize||mIndex<0) return 0;
#endif
  return mDeque.ObjectAt(mIndex);
}

/**
 * Call this method when you want to iterate all the
 * members of the container, passing a functor along
 * to call your code.
 *
 * @param   aFunctor object to call for each member
 * @return  *this
 */
void nsDequeIterator::ForEach(nsDequeFunctor& aFunctor) const{
  mDeque.ForEach(aFunctor);
}

/**
 * Call this method when you want to iterate all the
 * members of the container, calling the functor you 
 * passed with each member. This process will interrupt
 * if your function returns non 0 to this method.
 *
 * @param   aFunctor object to call for each member
 * @return  first nonzero result of aFunctor or 0.
 */
const void* nsDequeIterator::FirstThat(nsDequeFunctor& aFunctor) const{
  return mDeque.FirstThat(aFunctor);
}
