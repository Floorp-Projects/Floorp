/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIEnumerator.idl
 */

#ifndef __gen_nsIEnumerator_h__
#define __gen_nsIEnumerator_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsISimpleEnumerator */

/* {D1899240-F9D2-11D2-BDD6-000064657374} */
#define NS_ISIMPLEENUMERATOR_IID_STR "D1899240-F9D2-11D2-BDD6-000064657374"
#define NS_ISIMPLEENUMERATOR_IID \
  {0xD1899240, 0xF9D2, 0x11D2, \
    { 0xBD, 0xD6, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74 }}

class nsISimpleEnumerator : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISIMPLEENUMERATOR_IID)

  /* boolean HasMoreElements (); */
  NS_IMETHOD HasMoreElements(PRBool *_retval) = 0;

  /* nsISupports GetNext (); */
  NS_IMETHOD GetNext(nsISupports **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsISimpleEnumerator *priv);
#endif
};

/* starting interface:    nsIEnumerator */

/* {ad385286-cbc4-11d2-8cca-0060b0fc14a3} */
#define NS_IENUMERATOR_IID_STR "ad385286-cbc4-11d2-8cca-0060b0fc14a3"
#define NS_IENUMERATOR_IID \
  {0xad385286, 0xcbc4, 0x11d2, \
    { 0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3 }}

class nsIEnumerator : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IENUMERATOR_IID)

  /* void First (); */
  NS_IMETHOD First() = 0;

  /* void Next (); */
  NS_IMETHOD Next() = 0;

  /* nsISupports CurrentItem (); */
  NS_IMETHOD CurrentItem(nsISupports **_retval) = 0;

  /* void IsDone (); */
  NS_IMETHOD IsDone() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIEnumerator *priv);
#endif
};

/* starting interface:    nsIBidirectionalEnumerator */

/* {75f158a0-cadd-11d2-8cca-0060b0fc14a3} */
#define NS_IBIDIRECTIONALENUMERATOR_IID_STR "75f158a0-cadd-11d2-8cca-0060b0fc14a3"
#define NS_IBIDIRECTIONALENUMERATOR_IID \
  {0x75f158a0, 0xcadd, 0x11d2, \
    { 0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3 }}

class nsIBidirectionalEnumerator : public nsIEnumerator {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IBIDIRECTIONALENUMERATOR_IID)

  /* void Last (); */
  NS_IMETHOD Last() = 0;

  /* void Prev (); */
  NS_IMETHOD Prev() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIBidirectionalEnumerator *priv);
#endif
};
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
