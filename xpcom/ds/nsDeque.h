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

/**
 * MODULE NOTES:
 * @update  gess 4/15/98 (tax day)
 * 
 * The Deque is a very small, very efficient container object
 * than can hold elements of type void*, offering the following features:
 *    It's interface supports pushing and poping of children.
 *    It can iterate (via an interator class) it's children.
 *    When full, it can efficently resize dynamically.
 *
 *
 * NOTE: The only bit of trickery here is that this deque is
 * built upon a ring-buffer. Like all ring buffers, the first 
 * element may not be at index[0]. The mOrigin member determines 
 * where the first child is. This point is quietly hidden from 
 * customers of this class.
 *    
 */


#ifndef _NSDEQUE
#define _NSDEQUE

#include "nscore.h"

/**
 * The nsDequefunctor class is used when you want to create
 * callbacks between the deque and your generic code.
 * Use these objects in a call to ForEach();
 *
 * @update	gess4/20/98
 */
class NS_BASE nsDequeFunctor{
public:
  virtual void* operator()(void* anObject)=0;
};


/******************************************************
 * Here comes the nsDeque class itself...
 ******************************************************/

/**
 * The deque (double-ended queue) class is a common container type, 
 * whose behavior mimics a line in your favorite checkout stand.
 * Classic CS describes the common behavior of a queue as FIFO. 
 * A Deque allows items to be added and removed from either end of
 * the queue.
 *
 * @update	gess4/20/98
 */

class NS_BASE nsDeque {
friend class nsDequeIterator;
  public:
                       nsDeque(nsDequeFunctor* aDeallocator);
                      ~nsDeque();
            
  /**
   * Returns the number of elements currently stored in
   * this deque.
   *
   * @update	gess4/18/98
   * @param 
   * @return  int contains element count
   */
  PRInt32 GetSize() const;
  

  /**
   * Pushes new member onto the end of the deque
   *
   * @update	gess4/18/98
   * @param   ptr to object to store
   * @return  *this
   */
  nsDeque& Push(void* anItem);

  /**
   * Pushes new member onto the front of the deque
   *
   * @update	gess4/18/98
   * @param   ptr to object to store
   * @return  *this
   */
  nsDeque& PushFront(void* anItem);
  
  /**
   * Remove and return the first item in the container.
   * 
   * @update	gess4/18/98
   * @param   none
   * @return  ptr to first item in container
   */
  void* Pop(void);

 /**
   * Remove and return the first item in the container.
   * 
   * @update	gess4/18/98
   * @param   none
   * @return  ptr to first item in container
   */
  void* PopFront(void);

 
  /**
   * Return topmost item without removing it.
   * 
   * @update	gess4/18/98
   * @param   none
   * @return  ptr to first item in container
   */
  void* Peek(void);
  
  /**
   * method used to retrieve ptr to
   * ith member in container. DOesn't remove
   * that item.
   *
   * @update	gess4/18/98
   * @param   index of desired item
   * @return  ptr to ith element in list
   */
  void* ObjectAt(int anIndex) const;
  
  /**
   * Remove all items from container without destroying them
   * 
   * @update	gess4/18/98
   * @param 
   * @return
   */
  nsDeque& Empty();
  
  /**
   * Remove and delete all items from container
   * 
   * @update	gess4/18/98
   * @param 
   * @return
   */
  nsDeque& Erase();


  /**
   * Creates a new iterator, init'ed to start of container
   *
   * @update	gess4/18/98
   * @return  new dequeIterator
   */
  nsDequeIterator Begin() const;
  
  /**
   * Creates a new iterator, init'ed to end of container
   *
   * @update	gess4/18/98
   * @return  new dequeIterator
   */
  nsDequeIterator End() const;


  /**
   * Call this method when you wanto to iterate all the
   * members of the container, passing a functor along
   * to call your code.
   *
   * @update	gess4/20/98
   * @param   aFunctor object to call for each member
   * @return  *this
   */
  void ForEach(nsDequeFunctor& aFunctor) const;

  /**
   * Call this method when you wanto to iterate all the
   * members of the container, passing a functor along
   * to call your code. This process will interupt if
   * your function returns a null to this iterator.
   *
   * @update	gess4/20/98
   * @param   aFunctor object to call for each member
   * @return  *this
   */
  const void* FirstThat(nsDequeFunctor& aFunctor) const;

  void SetDeallocator(nsDequeFunctor* aDeallocator);

  /**
   * Perform automated selftest on the deque
   *
   * @update	gess4/18/98
   * @param 
   * @return
   */
  static void SelfTest();

protected:

  PRInt32         mSize;
  PRInt32         mCapacity;
  PRInt32         mOrigin;
  nsDequeFunctor* mDeallocator;
  void**          mData;


private: 
  
  enum {eGrowthDelta=64}; 

  /**
   * Simple default constructor (PRIVATE)
   * 
   * @update	gess4/18/98
   * @param 
   * @return
   */
  nsDeque();

  /**
   * Copy constructor (PRIVATE)
   * 
   * @update	gess4/18/98
   * @param 
   * @return
   */
  nsDeque(const nsDeque& other);

  /**
   * Deque assignment operator (PRIVATE)
   *
   * @update	gess4/18/98
   * @param   another deque
   * @return  *this
   */
  nsDeque& operator=(const nsDeque& anOther);

  nsDeque&  GrowCapacity(void);

};

/******************************************************
 * Here comes the nsDequeIterator class...
 ******************************************************/

class NS_BASE nsDequeIterator {
public:
  
  /**
   * DequeIterator is an object that knows how to iterate (forward and backward)
   * a Deque. Normally, you don't need to do this, but there are some special
   * cases where it is pretty handy, so here you go.
   *
   * @update	gess4/18/98
   * @param   aQueue is the deque object to be iterated
   * @param   anIndex is the starting position for your iteration
   */
  nsDequeIterator(const nsDeque& aQueue,int anIndex=0);
  
  /**
   * DequeIterator is an object that knows how to iterate (forward and backward)
   * a Deque. Normally, you don't need to do this, but there are some special
   * cases where it is pretty handy, so here you go.
   *
   * @update	gess4/18/98
   * @param   aQueue is the deque object to be iterated
   * @param   anIndex is the starting position for your iteration
   */
  nsDequeIterator(const nsDequeIterator& aCopy);

  /**
   * Moves iterator to first element in deque
   * @update	gess4/18/98
   * @return  this
   */
  nsDequeIterator& First(void);

  /**
   * Standard assignment operator for deque
   * @update	gess4/18/98
   * @param 
   * @return
   */
  nsDequeIterator& operator=(const nsDequeIterator& aCopy);

  /**
   * preform ! operation against to iterators to test for equivalence
   * (or lack thereof)!
   *
   * @update	gess4/18/98
   * @param   anIter is the object to be compared to
   * @return  TRUE if NOT equal.
   */
  PRBool operator!=(nsDequeIterator& anIter);

  /**
   * Compare 2 iterators for equivalence.
   *
   * @update	gess4/18/98
   * @param   anIter is the other iterator to be compared to
   * @return  TRUE if EQUAL
   */
  PRBool operator<(nsDequeIterator& anIter);

  /**
   * Compare 2 iterators for equivalence.
   *
   * @update	gess4/18/98
   * @param   anIter is the other iterator to be compared to
   * @return  TRUE if EQUAL
   */
  PRBool operator==(nsDequeIterator& anIter);
  
  /**
   * Compare 2 iterators for equivalence.
   *
   * @update	gess4/18/98
   * @param   anIter is the other iterator to be compared to
   * @return  TRUE if EQUAL
   */
  PRBool operator>=(nsDequeIterator& anIter);

  /**
   * Pre-increment operator 
   *
   * @update	gess4/18/98
   * @return  object at preincremented index
   */
  void* operator++();

  /**
   * Post-increment operator 
   *
   * @update	gess4/18/98
   * @param   param is ignored
   * @return  object at post-incremented index
   */
  void* operator++(int);

  /**
   * Pre-decrement operator
   *
   * @update	gess4/18/98
   * @return  object at pre-decremented index
   */
  void* operator--();

  /**
   * Post-decrement operator
   *
   * @update	gess4/18/98
   * @param   param is ignored
   * @return  object at post-decremented index
   */
  void* operator--(int);
  
  /**
   * Retrieve the ptr to the iterators notion of current node
   *
   * @update	gess4/18/98
   * @return  object at ith index
   */
  void* GetCurrent(void);

  /**
   * Call this method when you wanto to iterate all the
   * members of the container, passing a functor along
   * to call your code.
   *
   * @update	gess4/20/98
   * @param   aFunctor object to call for each member
   * @return  *this
   */
  void ForEach(nsDequeFunctor& aFunctor) const;
  
  /**
   * Call this method when you wanto to iterate all the
   * members of the container, passing a functor along
   * to call your code.
   *
   * @update	gess4/20/98
   * @param   aFunctor object to call for each member
   * @return  *this
   */
  const void* FirstThat(nsDequeFunctor& aFunctor) const;

  protected:

        PRInt32         mIndex;
        const nsDeque&  mDeque;
};


#endif
