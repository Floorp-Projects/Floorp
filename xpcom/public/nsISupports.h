/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsISupports.idl
 */

#ifndef __gen_nsISupports_h__
#define __gen_nsISupports_h__

#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
/* 
 * Start commenting out the C++ versions of the below in the output header
 */
#if 0


/* starting interface:    nsISupports */

/* {00000000-0000-0000-c000-000000000046} */
#define NS_ISUPPORTS_IID_STR "00000000-0000-0000-c000-000000000046"
#define NS_ISUPPORTS_IID \
  {0x00000000, 0x0000, 0x0000, \
    { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }}

class nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISUPPORTS_IID)

  /* void QueryInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
  NS_IMETHOD QueryInterface(const nsIID & uuid, void * *result) = 0;

  /* [noscript, notxpcom] nsrefcnt AddRef (); */
  NS_IMETHOD_(nsrefcnt) AddRef() = 0;

  /* [noscript, notxpcom] nsrefcnt Release (); */
  NS_IMETHOD_(nsrefcnt) Release() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsISupports *priv);
#endif
};
/* 
 * End commenting out the C++ versions of the above in the output header
 */
#endif

/* 
 * The declaration of nsISupports for C++ is in nsISupportsUtils.h.
 * This is to allow for some macro trickery to support the Mac.
 */
#include "nsISupportsUtils.h"


#endif /* __gen_nsISupports_h__ */
