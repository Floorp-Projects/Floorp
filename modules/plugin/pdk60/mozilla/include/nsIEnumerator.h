/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIEnumerator.idl
 */

#ifndef __gen_nsIEnumerator_h__
#define __gen_nsIEnumerator_h__

#include "nsISupports.h"

#define NS_ENUMERATOR_FALSE 1

/* starting interface:    nsISimpleEnumerator */
#define NS_ISIMPLEENUMERATOR_IID_STR "d1899240-f9d2-11d2-bdd6-000064657374"

#define NS_ISIMPLEENUMERATOR_IID \
  {0xd1899240, 0xf9d2, 0x11d2, \
    { 0xbd, 0xd6, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74 }}

class nsISimpleEnumerator : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISIMPLEENUMERATOR_IID)

  /* boolean HasMoreElements (); */
  NS_IMETHOD HasMoreElements(PRBool *_retval) = 0;

  /* nsISupports GetNext (); */
  NS_IMETHOD GetNext(nsISupports **_retval) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSISIMPLEENUMERATOR \
  NS_IMETHOD HasMoreElements(PRBool *_retval); \
  NS_IMETHOD GetNext(nsISupports **_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSISIMPLEENUMERATOR(_to) \
  NS_IMETHOD HasMoreElements(PRBool *_retval) { return _to ## HasMoreElements(_retval); } \
  NS_IMETHOD GetNext(nsISupports **_retval) { return _to ## GetNext(_retval); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsSimpleEnumerator : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

  nsSimpleEnumerator();
  virtual ~nsSimpleEnumerator();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsSimpleEnumerator, nsISimpleEnumerator)

nsSimpleEnumerator::nsSimpleEnumerator()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsSimpleEnumerator::~nsSimpleEnumerator()
{
  /* destructor code */
}

/* boolean HasMoreElements (); */
NS_IMETHODIMP nsSimpleEnumerator::HasMoreElements(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISupports GetNext (); */
NS_IMETHODIMP nsSimpleEnumerator::GetNext(nsISupports **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


/* starting interface:    nsIEnumerator */
#define NS_IENUMERATOR_IID_STR "ad385286-cbc4-11d2-8cca-0060b0fc14a3"

#define NS_IENUMERATOR_IID \
  {0xad385286, 0xcbc4, 0x11d2, \
    { 0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3 }}

class nsIEnumerator : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IENUMERATOR_IID)

  /** First will reset the list. will return NS_FAILED if no items
   */
  /* void first (); */
  NS_IMETHOD First(void) = 0;

  /** Next will advance the list. will return failed if already at end
   */
  /* void next (); */
  NS_IMETHOD Next(void) = 0;

  /** CurrentItem will return the CurrentItem item it will fail if the 
   *  list is empty
   */
  /* nsISupports currentItem (); */
  NS_IMETHOD CurrentItem(nsISupports **_retval) = 0;

  /** return if the collection is at the end.  that is the beginning following 
   *  a call to Prev and it is the end of the list following a call to next
   */
  /* void isDone (); */
  NS_IMETHOD IsDone(void) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIENUMERATOR \
  NS_IMETHOD First(void); \
  NS_IMETHOD Next(void); \
  NS_IMETHOD CurrentItem(nsISupports **_retval); \
  NS_IMETHOD IsDone(void); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIENUMERATOR(_to) \
  NS_IMETHOD First(void) { return _to ## First(); } \
  NS_IMETHOD Next(void) { return _to ## Next(); } \
  NS_IMETHOD CurrentItem(nsISupports **_retval) { return _to ## CurrentItem(_retval); } \
  NS_IMETHOD IsDone(void) { return _to ## IsDone(); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsEnumerator : public nsIEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIENUMERATOR

  nsEnumerator();
  virtual ~nsEnumerator();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsEnumerator, nsIEnumerator)

nsEnumerator::nsEnumerator()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsEnumerator::~nsEnumerator()
{
  /* destructor code */
}

/* void first (); */
NS_IMETHODIMP nsEnumerator::First()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void next (); */
NS_IMETHODIMP nsEnumerator::Next()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISupports currentItem (); */
NS_IMETHODIMP nsEnumerator::CurrentItem(nsISupports **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void isDone (); */
NS_IMETHODIMP nsEnumerator::IsDone()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


/* starting interface:    nsIBidirectionalEnumerator */
#define NS_IBIDIRECTIONALENUMERATOR_IID_STR "75f158a0-cadd-11d2-8cca-0060b0fc14a3"

#define NS_IBIDIRECTIONALENUMERATOR_IID \
  {0x75f158a0, 0xcadd, 0x11d2, \
    { 0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3 }}

class nsIBidirectionalEnumerator : public nsIEnumerator {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IBIDIRECTIONALENUMERATOR_IID)

  /** Last will reset the list to the end. will return NS_FAILED if no items
   */
  /* void Last (); */
  NS_IMETHOD Last(void) = 0;

  /** Prev will decrement the list. will return failed if already at beginning
   */
  /* void Prev (); */
  NS_IMETHOD Prev(void) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIBIDIRECTIONALENUMERATOR \
  NS_IMETHOD Last(void); \
  NS_IMETHOD Prev(void); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIBIDIRECTIONALENUMERATOR(_to) \
  NS_IMETHOD Last(void) { return _to ## Last(); } \
  NS_IMETHOD Prev(void) { return _to ## Prev(); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsBidirectionalEnumerator : public nsIBidirectionalEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBIDIRECTIONALENUMERATOR

  nsBidirectionalEnumerator();
  virtual ~nsBidirectionalEnumerator();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsBidirectionalEnumerator, nsIBidirectionalEnumerator)

nsBidirectionalEnumerator::nsBidirectionalEnumerator()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsBidirectionalEnumerator::~nsBidirectionalEnumerator()
{
  /* destructor code */
}

/* void Last (); */
NS_IMETHODIMP nsBidirectionalEnumerator::Last()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Prev (); */
NS_IMETHODIMP nsBidirectionalEnumerator::Prev()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif

extern "C" NS_COM nsresult
NS_NewEmptyEnumerator(nsISimpleEnumerator** aResult);
// Construct and return an implementation of a "conjoining enumerator." This
// enumerator lets you string together two other enumerators into one sequence.
// The result is an nsIBidirectionalEnumerator, but if either input is not
// also bidirectional, the Last and Prev operations will fail.
extern "C" NS_COM nsresult
NS_NewConjoiningEnumerator(nsIEnumerator* first, nsIEnumerator* second,
                           nsIBidirectionalEnumerator* *aInstancePtrResult);
// Construct and return an implementation of a "union enumerator." This
// enumerator will only return elements that are found in both constituent
// enumerators.
extern "C" NS_COM nsresult
NS_NewUnionEnumerator(nsIEnumerator* first, nsIEnumerator* second,
                      nsIEnumerator* *aInstancePtrResult);
// Construct and return an implementation of an "intersection enumerator." This
// enumerator will return elements that are found in either constituent
// enumerators, eliminating duplicates.
extern "C" NS_COM nsresult
NS_NewIntersectionEnumerator(nsIEnumerator* first, nsIEnumerator* second,
                             nsIEnumerator* *aInstancePtrResult);

#endif /* __gen_nsIEnumerator_h__ */
