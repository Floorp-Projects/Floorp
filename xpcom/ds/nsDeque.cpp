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

/**
 * Standard constructor
 * @update	gess4/18/98
 * @return  new deque
 */
nsDeque::nsDeque(PRBool aOwnsMemory) {
  mOwnsMemory=aOwnsMemory;
  mCapacity=eGrowthDelta;
  mOrigin=mSize=0;
  mData=new void*[mCapacity];
}


/**
 * Destructor
 * @update	gess4/18/98
 */
nsDeque::~nsDeque() {
  if(mOwnsMemory)
    Erase();
  delete [] mData;
  mData=0;
  mOwnsMemory=PR_FALSE;
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

/**
 * Remove all items from container without destroying them.
 *
 * @update	gess4/18/98
 * @param 
 * @return
 */
nsDeque& nsDeque::Empty() {
  for(int i=0;i<mCapacity;i++)
    mData[i]=0;
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
  nsDequeIterator iter1=Begin();
  nsDequeIterator iter2=End();
  void* temp;
  while(iter1!=iter2) {
    temp=(iter1++);
    delete temp;
  }
  return Empty();
}


/**
 * This method adds an item to the end of the queue. 
 * This operation has the potential to cause the 
 * underlying buffer to resize.
 *
 * @update	gess4/18/98
 * @param   anItem: new item to be added to queue
 * @return  nada
 */
nsDeque& nsDeque::Push(void* anItem) {
  if(mSize==mCapacity) {
    void** temp=new void*[mCapacity+eGrowthDelta];

    //so here's the problem. You can't just move the elements
    //directy (in situ) from the old buffer to the new one.
    //Since capacity has changed, the old origin doesn't make
    //sense anymore. It's better to resequence the elements now.
    
    int tempi=0;
    for(int i=mOrigin;i<mCapacity;i++) temp[tempi++]=mData[i]; //copy the leading elements...
    for(int j=0;j<mOrigin;j++) temp[tempi++]=mData[j];         //copy the trailing elements...
    mCapacity+=eGrowthDelta;
    mOrigin=0;       //now realign the origin...
    delete[]mData;
    mData=temp;
  }
  int offset=mOrigin+mSize;
  if(offset<mCapacity)
    mData[offset]=anItem;
  else mData[offset-mCapacity]=anItem;
  mSize++;
  return *this;
}


/**
 * This method gets called you want to remove and return
 * the first member in the container.
 *
 * @update	gess4/18/98
 * @param   nada
 * @return  last item in container
 */
void* nsDeque::Pop() {
  void* result=0;
  if(mSize>0) {
    result=mData[mOrigin];
    mData[mOrigin++]=0;     //zero it out for debugging purposes.
    mSize--;
    if(0==mSize)
      mOrigin=0;
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
    if(anIndex<(mSize-mOrigin)) {
      result=mData[mOrigin+anIndex];
    }
    else {
      result=mData[anIndex-(mSize-mOrigin)];
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
const nsDeque& nsDeque::ForEach(nsDequeFunctor& aFunctor) const{
  for(int i=0;i<mSize;i++){
    void* obj=ObjectAt(i);
    aFunctor(obj);
  }
  return *this;
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
nsDequeIterator::nsDequeIterator(const nsDeque& aQueue,int anIndex): mDeque(aQueue) {
  mIndex=anIndex; 
}

/**
 * Copy construct a new iterator beginning with given 
 *
 * @update	gess4/20/98
 * @param   aCopy is another iterator to copy from
 * @return
 */
nsDequeIterator::nsDequeIterator(const nsDequeIterator& aCopy) : 
  mDeque(aCopy.mDeque),
  mIndex(aCopy.mIndex) {
}

/**
 * Standard assignment operator for dequeiterator
 *
 * @update	gess4/18/98
 * @param   aCopy is an iterator to be copied from
 * @return  *this
 */
nsDequeIterator& nsDequeIterator::operator=(nsDequeIterator& aCopy) {
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
PRBool nsDequeIterator::operator==(nsDequeIterator& anIter) {
  return PRBool(((mIndex==anIter.mIndex) && (&mDeque==&anIter.mDeque)));
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
const nsDequeIterator& nsDequeIterator::ForEach(nsDequeFunctor& aFunctor) const{
  mDeque.ForEach(aFunctor);
  return *this;
}


/**
 * conduct automated self test for this class
 *
 * @update	gess4/18/98
 * @param 
 * @return
 */
void nsDeque::SelfTest(void) {
#undef  _SELFTEST_DEQUE
#ifdef  _SELFTEST_DEQUE
#include <iostream.h>

  {
    nsDeque theDeque(PR_FALSE); //construct a simple one...
    
    int ints[10]={100,200,300,400,500,600,700,800,900,1000};
    int count=sizeof(ints)/sizeof(int);

    for(int i=0;i<count;i++){
      theDeque.Push(&ints[i]);
    }

    int* temp1=(int*)theDeque.Pop(); //should have popped 100
    int* temp2=(int*)theDeque.Pop(); //should have popped 200

    theDeque.Push(temp1); //these should now become
    theDeque.Push(temp2); //the last 2 items in deque.

    nsDequeIterator iter1=theDeque.Begin();
    nsDequeIterator iter2=theDeque.End();
    while(iter1!=iter2) {
      temp1=(int*)(iter1++);
      cout << *temp1 << endl;
    }

    //now, afll thru and watch the deque dtor run...
    cout << "done" << endl;
  }

  int x;
  x=10;
#endif
}

