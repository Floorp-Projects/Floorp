/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIFactory.idl
 */

#ifndef __gen_nsIFactory_h__
#define __gen_nsIFactory_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIFactory */

/* {00000001-0000-0000-c000-000000000046} */
#define NS_IFACTORY_IID_STR "00000001-0000-0000-c000-000000000046"
#define NS_IFACTORY_IID \
  {0x00000001, 0x0000, 0x0000, \
    { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }}

class nsIFactory : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFACTORY_IID)

  /* voidStar CreateInstance (in nsISupports aOuter, in nsIIDRef iid); */
  NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID & iid, void * *_retval) = 0;

  /* void LockFactory (in PRBool lock); */
  NS_IMETHOD LockFactory(PRBool lock) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIFactory *priv);
#endif
};

#endif /* __gen_nsIFactory_h__ */
