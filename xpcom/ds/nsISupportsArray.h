/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsISupportsArray.idl
 */

#ifndef __gen_nsISupportsArray_h__
#define __gen_nsISupportsArray_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsICollection.h" /* interface nsICollection */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
class nsIBidirectionalEnumerator;
 
#define NS_SUPPORTSARRAY_CID                         \
{ /* bda17d50-0d6b-11d3-9331-00104ba0fd40 */         \
    0xbda17d50,                                      \
    0x0d6b,                                          \
    0x11d3,                                          \
    {0x93, 0x31, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}
#define NS_SUPPORTSARRAY_PROGID "component://netscape/supports-array"
#define NS_SUPPORTSARRAY_CLASSNAME "Supports Array"
 
// Enumerator callback function. Return PR_FALSE to stop
typedef PRBool (*nsISupportsArrayEnumFunc)(nsISupports* aElement, void *aData);
 


/* starting interface:    nsISupportsArray */

/* {791eafa0-b9e6-11d1-8031-006008159b5a} */
#define NS_ISUPPORTSARRAY_IID_STR "791eafa0-b9e6-11d1-8031-006008159b5a"
#define NS_ISUPPORTSARRAY_IID \
  {0x791eafa0, 0xb9e6, 0x11d1, \
    { 0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a }}

class nsISupportsArray : public nsICollection {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISUPPORTSARRAY_IID)

  /* [notxpcom] boolean Equals ([const] in nsISupportsArray other); */
  NS_IMETHOD_(PRBool) Equals(const nsISupportsArray *other) = 0;

  /* [notxpcom] nsISupports ElementAt (in unsigned long aIndex); */
  NS_IMETHOD_(nsISupports *) ElementAt(PRUint32 aIndex) = 0;

  /* [notxpcom] long IndexOf ([const] in nsISupports aPossibleElement); */
  NS_IMETHOD_(PRInt32) IndexOf(const nsISupports *aPossibleElement) = 0;

  /* [notxpcom] long IndexOfStartingAt ([const] in nsISupports aPossibleElement, in unsigned long aStartIndex); */
  NS_IMETHOD_(PRInt32) IndexOfStartingAt(const nsISupports *aPossibleElement, PRUint32 aStartIndex) = 0;

  /* [notxpcom] long LastIndexOf ([const] in nsISupports aPossibleElement); */
  NS_IMETHOD_(PRInt32) LastIndexOf(const nsISupports *aPossibleElement) = 0;

  /* long GetIndexOf (in nsISupports aPossibleElement); */
  NS_IMETHOD GetIndexOf(nsISupports *aPossibleElement, PRInt32 *_retval) = 0;

  /* long GetIndexOfStartingAt (in nsISupports aPossibleElement, in unsigned long aStartIndex); */
  NS_IMETHOD GetIndexOfStartingAt(nsISupports *aPossibleElement, PRUint32 aStartIndex, PRInt32 *_retval) = 0;

  /* long GetLastIndexOf (in nsISupports aPossibleElement); */
  NS_IMETHOD GetLastIndexOf(nsISupports *aPossibleElement, PRInt32 *_retval) = 0;

  /* [notxpcom] boolean InsertElementAt (in nsISupports aElement, in unsigned long aIndex); */
  NS_IMETHOD_(PRBool) InsertElementAt(nsISupports *aElement, PRUint32 aIndex) = 0;

  /* [notxpcom] boolean ReplaceElementAt (in nsISupports aElement, in unsigned long aIndex); */
  NS_IMETHOD_(PRBool) ReplaceElementAt(nsISupports *aElement, PRUint32 aIndex) = 0;

  /* [notxpcom] boolean RemoveElementAt (in unsigned long aIndex); */
  NS_IMETHOD_(PRBool) RemoveElementAt(PRUint32 aIndex) = 0;

  /* [notxpcom] boolean RemoveLastElement ([const] in nsISupports aElement); */
  NS_IMETHOD_(PRBool) RemoveLastElement(const nsISupports *aElement) = 0;

  /* void DeleteLastElement (in nsISupports aElement); */
  NS_IMETHOD DeleteLastElement(nsISupports *aElement) = 0;

  /* void DeleteElementAt (in unsigned long aIndex); */
  NS_IMETHOD DeleteElementAt(PRUint32 aIndex) = 0;

  /* [notxpcom] boolean AppendElements (in nsISupportsArray aElements); */
  NS_IMETHOD_(PRBool) AppendElements(nsISupportsArray *aElements) = 0;

  /* void Compact (); */
  NS_IMETHOD Compact() = 0;

  /* [noscript, notxpcom] boolean EnumerateForwards (in nsISupportsArrayEnumFunc aFunc, in voidStar aData); */
  NS_IMETHOD_(PRBool) EnumerateForwards(nsISupportsArrayEnumFunc aFunc, void * aData) = 0;

  /* [noscript, notxpcom] boolean EnumerateBackwards (in nsISupportsArrayEnumFunc aFunc, in voidStar aData); */
  NS_IMETHOD_(PRBool) EnumerateBackwards(nsISupportsArrayEnumFunc aFunc, void * aData) = 0;
private:
  NS_IMETHOD_(nsISupportsArray&) operator=(const nsISupportsArray& other) = 0;
  NS_IMETHOD_(PRBool) operator==(const nsISupportsArray& other) = 0;
  NS_IMETHOD_(nsISupports*)  operator[](PRUint32 aIndex) = 0;

};
// Construct and return a default implementation of nsISupportsArray:
extern NS_COM nsresult
NS_NewISupportsArray(nsISupportsArray** aInstancePtrResult);
// Construct and return a default implementation of an enumerator for nsISupportsArrays:
extern NS_COM nsresult
NS_NewISupportsArrayEnumerator(nsISupportsArray* array,
                               nsIBidirectionalEnumerator* *aInstancePtrResult);


#endif /* __gen_nsISupportsArray_h__ */
