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

//#define _SELFTEST_DEQUE 1
#undef _SELFTEST_DEQUE 

MOZ_DECL_CTOR_COUNTER(nsDeque)

/**
 * Standard constructor
 * @update	gess4/18/98
 * @return  new deque
 */
nsDeque::nsDeque(nsDequeFunctor* aDeallocator) {
  MOZ_COUNT_CTOR(nsDeque);
  mDeallocator=aDeallocator;
  mOrigin=mSize=0;
  mData=mBuffer; // don't allocate space until you must
  mCapacity=sizeof(mBuffer)/sizeof(mBuffer[0]);
  nsCRT::zero(mData,mCapacity*sizeof(mBuffer[0]));
}


/**
 * Destructor
 * @update	gess4/18/98
 */
nsDeque::~nsDeque() {
  MOZ_COUNT_DTOR(nsDeque);

#if 0
  char buffer[30];
  printf("Capacity: %i\n",mCapacity);

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
  if(mData && (mData!=mBuffer))
    delete [] mData;
  mData=0;
  if(mDeallocator) {
    delete mDeallocator;
  }
  mDeallocator=0;
}


/**
 *
 * @update	gess4/18/98
 * @param 
 * @return  
 */ 
void nsDeque::SetDeallocator(nsDequeFunctor* aDeallocator){
  if(mDeallocator) {
    delete mDeallocator;
  }
  mDeallocator=aDeallocator;
}

/**
 * Remove all items from container without destroying them.
 *
 * @update	gess4/18/98
 * @param 
 * @return
 */
nsDeque& nsDeque::Empty() {
  if((0<mCapacity) && (mData)) {
    nsCRT::zero(mData,mCapacity*sizeof(mData));
  }
  mSize=0;
  mOrigin=0;
  return *this;
}

/**
 * Remove and delete all items from container
 * 
 * @update	gess4/18/98
 * @return  this
 */
nsDeque& nsDeque::Erase() { 
  if(mDeallocator && mSize) {
    ForEach(*mDeallocator);
  }
  return Empty();
}


/**
 * This method adds an item to the end of the deque. 
 * This operation has the potential to cause the 
 * underlying buffer to resize.
 *
 * @update	gess4/18/98
 * @param   anItem: new item to be added to deque
 * @return  nada
 */
nsDeque& nsDeque::GrowCapacity(void) {

  PRInt32 theNewSize = mCapacity<<2;
  void** temp=new void*[theNewSize];

  //Here's the interesting part: You can't just move the elements
  //directy (in situ) from the old buffer to the new one.
  //Since capacity has changed, the old origin doesn't make
  //sense anymore. It's better to resequence the elements now.

  if(mData) {
    int tempi=0;
    int i=0;
    int j=0;
    for(i=mOrigin;i<mCapacity;i++) temp[tempi++]=mData[i]; //copy the leading elements...
    for(j=0;j<mOrigin;j++) temp[tempi++]=mData[j];         //copy the trailing elements...
    if(mData!=mBuffer)
      delete [] mData;
  }
  mCapacity=theNewSize;
  mOrigin=0;       //now realign the origin...
  mData=temp;
  return *this;
}

/**
 * This method adds an item to the end of the deque. 
 * This operation has the potential to cause the 
 * underlying buffer to resize.
 *
 * @update	gess4/18/98
 * @param   anItem: new item to be added to deque
 * @return  nada
 */
nsDeque& nsDeque::Push(void* anItem) {
  if(mSize==mCapacity) {
    GrowCapacity();
  }
  int offset=mOrigin+mSize;
  if(offset<mCapacity)
    mData[offset]=anItem;
  else mData[offset-mCapacity]=anItem;
  mSize++;
  return *this;
}

/**
 * This method adds an item to the front of the deque. 
 * This operation has the potential to cause the 
 * underlying buffer to resize.
 *
 * @update	gess4/18/98
 * @param   anItem: new item to be added to deque
 * @return  nada
 */
nsDeque& nsDeque::PushFront(void* anItem) {
  if(mSize==mCapacity) {
    GrowCapacity();
  }
  if(0==mOrigin){  //case1: [xxx..]
    //mOrigin=mCapacity-1-mSize++;
    mOrigin=mCapacity-1;
    mData[mOrigin]=anItem;
  }
  else {// if(mCapacity==(mOrigin+mSize-1)){ //case2: [..xxx] and case3: [.xxx.]
    mData[--mOrigin]=anItem;
  }
  mSize++;
  return *this;
}

/**
 * Remove and return the last item in the container.
 * 
 * @update	gess4/18/98
 * @param   none
 * @return  ptr to last item in container
 */
void* nsDeque::Pop(void) {
  void* result=0;
  if(mSize>0) {
    int offset=mOrigin+mSize-1;
    if(offset>=mCapacity) 
      offset-=mCapacity;
    result=mData[offset];
    mData[offset]=0;
    mSize--;
    if(0==mSize)
      mOrigin=0;
  }
  return result;
}

/**
 * This method gets called you want to remove and return
 * the first member in the container.
 *
 * @update	gess4/18/98
 * @param   nada
 * @return  last item in container
 */
void* nsDeque::PopFront() {
  void* result=0;
  if(mSize>0) {
    NS_ASSERTION(mOrigin<mCapacity,"Error: Bad origin");
    result=mData[mOrigin];
    mData[mOrigin++]=0;     //zero it out for debugging purposes.
    mSize--;
    if(mCapacity==mOrigin)  //you popped off the end, so cycle back around...
      mOrigin=0;
    if(0==mSize)
      mOrigin=0;
  }
  return result;
}
/**
 * This method gets called you want to peek at the bottom
 * member without removing it.
 *
 * @update	sford 11/25/99
 * @param   nada
 * @return  last item in container
 */
void* nsDeque::Peek() {
	void* result=0;
	if(mSize>0) {
		int offset=mOrigin+mSize-1;	
		result=mData[offset];
	}
	return result;
}	

/**
 * This method gets called you want to peek at the topmost
 * member without removing it.
 *
 * @update  sford 11/25/99	
 * @param   nada
 * @return  last item in container
 */
void* nsDeque::PeekFront() {
  void* result=0;
  if(mSize>0) {
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
 * @update	gess4/18/98
 * @param   anIndex : 0 relative offset of item you want
 * @return  void* or null
 */
void* nsDeque::ObjectAt(PRInt32 anIndex) const {
  void* result=0;

  if((anIndex>=0) && (anIndex<mSize))
  {
    if(anIndex<(mCapacity-mOrigin)) {
      result=mData[mOrigin+anIndex];
    }
    else {
      result=mData[anIndex-(mCapacity-mOrigin)];
    }
  }
  return result;
}

/**
 * Create and return an iterator pointing to
 * the beginning of the queue. Note that this
 * takes the circular buffer semantics into account.
 * 
 * @update	gess4/18/98
 * @return  new deque iterator, init'ed to 1st item
 */
nsDequeIterator nsDeque::Begin(void) const{
  return nsDequeIterator(*this,0);
}

/**
 * Create and return an iterator pointing to
 * the last of the queue. Note that this
 * takes the circular buffer semantics into account.
 * 
 * @update	gess4/18/98
 * @return  new deque iterator, init'ed to last item
 */
nsDequeIterator nsDeque::End(void) const{
  return nsDequeIterator(*this,mSize);
}

/**
 * Call this method when you wanto to iterate all the
 * members of the container, passing a functor along
 * to call your code.
 *
 * @update	gess4/20/98
 * @param   aFunctor object to call for each member
 * @return  *this
 */
void nsDeque::ForEach(nsDequeFunctor& aFunctor) const{
  int i=0;
  for(i=0;i<mSize;i++){
    void* obj=ObjectAt(i);
    obj=aFunctor(obj);
  }
}

/**
 * Call this method when you wanto to iterate all the
 * members of the container, passing a functor along
 * to call your code. Iteration continues until your
 * functor returns a non-null.
 *
 * @update	gess4/20/98
 * @param   aFunctor object to call for each member
 * @return  *this
 */
const void* nsDeque::FirstThat(nsDequeFunctor& aFunctor) const{
  int i=0;
  for(i=0;i<mSize;i++){
    void* obj=ObjectAt(i);
    obj=aFunctor(obj);
    if(obj)
      return obj;
  }
  return 0;
}


/******************************************************
 * Here comes the nsDequeIterator class...
 ******************************************************/

/**
 * DequeIterator is an object that knows how to iterate (forward and backward)
 * a Deque. Normally, you don't need to do this, but there are some special
 * cases where it is pretty handy, so here you go.
 *
 * This is a standard dequeiterator constructor
 *
 * @update	gess4/18/98
 * @param   aQueue is the deque object to be iterated
 * @param   anIndex is the starting position for your iteration
 */
nsDequeIterator::nsDequeIterator(const nsDeque& aQueue,int anIndex):  mIndex(anIndex), mDeque(aQueue) {
}

/**
 * Copy construct a new iterator beginning with given 
 *
 * @update	gess4/20/98
 * @param   aCopy is another iterator to copy from
 * @return
 */
nsDequeIterator::nsDequeIterator(const nsDequeIterator& aCopy) : mIndex(aCopy.mIndex), mDeque(aCopy.mDeque) {
}

/**
 * Moves iterator to first element in deque
 * @update	gess4/18/98
 * @return  this
 */
nsDequeIterator& nsDequeIterator::First(void){
  mIndex=0;
  return *this;
}

/**
 * Standard assignment operator for dequeiterator
 *
 * @update	gess4/18/98
 * @param   aCopy is an iterator to be copied from
 * @return  *this
 */
nsDequeIterator& nsDequeIterator::operator=(const nsDequeIterator& aCopy) {
  //queue's are already equal.
  mIndex=aCopy.mIndex;
  return *this;
}

/**
 * preform ! operation against to iterators to test for equivalence
 * (or lack thereof)!
 *
 * @update	gess4/18/98
 * @param   anIter is the object to be compared to
 * @return  TRUE if NOT equal.
 */
PRBool nsDequeIterator::operator!=(nsDequeIterator& anIter) {
  return PRBool(!this->operator==(anIter));
}


/**
 * Compare 2 iterators for equivalence.
 *
 * @update	gess4/18/98
 * @param   anIter is the other iterator to be compared to
 * @return  TRUE if EQUAL
 */
PRBool nsDequeIterator::operator<(nsDequeIterator& anIter) {
  return PRBool(((mIndex<anIter.mIndex) && (&mDeque==&anIter.mDeque)));
}

/**
 * Compare 2 iterators for equivalence.
 *
 * @update	gess4/18/98
 * @param   anIter is the other iterator to be compared to
 * @return  TRUE if EQUAL
 */
PRBool nsDequeIterator::operator==(nsDequeIterator& anIter) {
  return PRBool(((mIndex==anIter.mIndex) && (&mDeque==&anIter.mDeque)));
}

/**
 * Compare 2 iterators for equivalence.
 *
 * @update	gess4/18/98
 * @param   anIter is the other iterator to be compared to
 * @return  TRUE if EQUAL
 */
PRBool nsDequeIterator::operator>=(nsDequeIterator& anIter) {
  return PRBool(((mIndex>=anIter.mIndex) && (&mDeque==&anIter.mDeque)));
}

/**
 * Pre-increment operator 
 *
 * @update	gess4/18/98
 * @return  object at preincremented index
 */
void* nsDequeIterator::operator++() {
  return mDeque.ObjectAt(++mIndex);
}

/**
 * Post-increment operator 
 *
 * @update	gess4/18/98
 * @param   param is ignored
 * @return  object at post-incremented index
 */
void* nsDequeIterator::operator++(int) {
  return mDeque.ObjectAt(mIndex++);
}

/**
 * Pre-decrement operator
 *
 * @update	gess4/18/98
 * @return  object at pre-decremented index
 */
void* nsDequeIterator::operator--() {
  return mDeque.ObjectAt(--mIndex);
}

/**
 * Post-decrement operator
 *
 * @update	gess4/18/98
 * @param   param is ignored
 * @return  object at post-decremented index
 */
void* nsDequeIterator::operator--(int) {
  return mDeque.ObjectAt(mIndex--);
}

/**
 * Dereference operator
 *
 * @update	gess4/18/98
 * @return  object at ith index
 */
void* nsDequeIterator::GetCurrent(void) {
  return mDeque.ObjectAt(mIndex);
}

/**
 * Call this method when you wanto to iterate all the
 * members of the container, passing a functor along
 * to call your code.
 *
 * @update	gess4/20/98
 * @param   aFunctor object to call for each member
 * @return  *this
 */
void nsDequeIterator::ForEach(nsDequeFunctor& aFunctor) const{
  mDeque.ForEach(aFunctor);
}

/**
 * Call this method when you wanto to iterate all the
 * members of the container, passing a functor along
 * to call your code.
 *
 * @update	gess4/20/98
 * @param   aFunctor object to call for each member
 * @return  *this
 */
const void* nsDequeIterator::FirstThat(nsDequeFunctor& aFunctor) const{
  return mDeque.FirstThat(aFunctor);
}

#ifdef  _SELFTEST_DEQUE
/**************************************************************
  Now define the token deallocator class...
 **************************************************************/
class _SelfTestDeallocator: public nsDequeFunctor{
public:
  _SelfTestDeallocator::_SelfTestDeallocator() {
    nsDeque::SelfTest();
  }
  virtual void* operator()(void* anObject) {
    return 0;
  }
};
static _SelfTestDeallocator gDeallocator;
#endif

/**
 * conduct automated self test for this class
 *
 * @update	gess4/18/98
 * @param 
 * @return
 */
void nsDeque::SelfTest(void) {
#ifdef  _SELFTEST_DEQUE

  {
    nsDeque theDeque(gDeallocator); //construct a simple one...
    
    int ints[200];
    int count=sizeof(ints)/sizeof(int);
    int i=0;

    for(i=0;i<count;i++){ //initialize'em
      ints[i]=10*(1+i);
    }

    for(i=0;i<70;i++){
      theDeque.Push(&ints[i]);
    }

    for(i=0;i<56;i++){
      int* temp=(int*)theDeque.Pop();
    }

    for(i=0;i<55;i++){
      theDeque.Push(&ints[i]);
    }

    for(i=0;i<35;i++){
      int* temp=(int*)theDeque.Pop();
    }

    for(i=0;i<35;i++){
      theDeque.Push(&ints[i]);
    }

    for(i=0;i<38;i++){
      int* temp=(int*)theDeque.Pop();
    }

  }

  int x;
  x=10;
#endif
}

