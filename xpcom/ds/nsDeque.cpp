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

 
#include "nsDeque.h"
#include "nsCRT.h"

//#define _SELFTEST_DEQUE 1
#undef _SELFTEST_DEQUE 

/**
 * Standard constructor
 * @update	gess4/18/98
 * @return  new deque
 */
nsDeque::nsDeque(nsDequeFunctor* aDeallocator) {
  mDeallocator=aDeallocator;
  mCapacity=eGrowthDelta;
  mOrigin=mSize=0;
  mData=new void*[mCapacity];
}


/**
 * Destructor
 * @update	gess4/18/98
 */
nsDeque::~nsDeque() {
  Erase();
  delete [] mData;
  mData=0;
  if(mDeallocator) {
    delete mDeallocator;
  }
  mDeallocator=0;
}


/**
 * Returns the number of elements currently stored in
 * this deque.
 *
 * @update	gess4/18/98
 * @param 
 * @return  int contains element count
 */ 
PRInt32 nsDeque::GetSize(void) const {
  return mSize;
}

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
  if(mDeallocator) {
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
  void** temp=new void*[mCapacity+eGrowthDelta];

  //Here's the interesting part: You can't just move the elements
  //directy (in situ) from the old buffer to the new one.
  //Since capacity has changed, the old origin doesn't make
  //sense anymore. It's better to resequence the elements now.
  
  int tempi=0;
  int i=0;
  int j=0;
  for(i=mOrigin;i<mCapacity;i++) temp[tempi++]=mData[i]; //copy the leading elements...
  for(j=0;j<mOrigin;j++) temp[tempi++]=mData[j];         //copy the trailing elements...
  mCapacity+=eGrowthDelta;
  mOrigin=0;       //now realign the origin...
  delete[]mData;
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
    mOrigin=mCapacity-1-mSize++;
    mData[mOrigin]=anItem;
  }
  else {// if(mCapacity==(mOrigin+mSize-1)){ //case2: [..xxx] and case3: [.xxx.]
    mData[--mOrigin]=anItem;
    mSize++;
  }
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
    result=mData[mOrigin];
    mData[mOrigin++]=0;     //zero it out for debugging purposes.
    mSize--;
    if(mCapacity==mOrigin)  //you popped off the end, so cycle back around...
      mOrigin=0;
    if(0==mSize)
      mOrigin=0;
  }
  NS_ASSERTION(mOrigin<mCapacity,"Error: Bad origin");
  return result;
}

/**
 * This method gets called you want to peek at the topmost
 * member without removing it.
 *
 * @update	gess4/18/98
 * @param   nada
 * @return  last item in container
 */
void* nsDeque::Peek() {
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

