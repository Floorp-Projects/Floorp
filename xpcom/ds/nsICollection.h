/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsICollection.idl
 */

#ifndef __gen_nsICollection_h__
#define __gen_nsICollection_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIEnumerator.h" /* interface nsIEnumerator */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsICollection */

/* {83b6019c-cbc4-11d2-8cca-0060b0fc14a3} */
#define NS_ICOLLECTION_IID_STR "83b6019c-cbc4-11d2-8cca-0060b0fc14a3"
#define NS_ICOLLECTION_IID \
  {0x83b6019c, 0xcbc4, 0x11d2, \
    { 0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3 }}

class nsICollection : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOLLECTION_IID)

  /* unsigned long Count (); */
  NS_IMETHOD Count(PRUint32 *_retval) = 0;

  /* nsISupports GetElementAt (in unsigned long index); */
  NS_IMETHOD GetElementAt(PRUint32 index, nsISupports **_retval) = 0;

  /* void SetElementAt (in unsigned long index, in nsISupports item); */
  NS_IMETHOD SetElementAt(PRUint32 index, nsISupports *item) = 0;

  /* void AppendElement (in nsISupports item); */
  NS_IMETHOD AppendElement(nsISupports *item) = 0;

  /* void RemoveElement (in nsISupports item); */
  NS_IMETHOD RemoveElement(nsISupports *item) = 0;

  /* nsIEnumerator Enumerate (); */
  NS_IMETHOD Enumerate(nsIEnumerator **_retval) = 0;

  /* void Clear (); */
  NS_IMETHOD Clear() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsICollection *priv);
#endif
};

#endif /* __gen_nsICollection_h__ */
