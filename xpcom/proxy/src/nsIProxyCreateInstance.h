/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIProxyCreateInstance.idl
 */

#ifndef __gen_nsIProxyCreateInstance_h__
#define __gen_nsIProxyCreateInstance_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIProxyCreateInstance */

/* {948c2080-0398-11d3-915e-0000863011c4} */
#define NS_IPROXYCREATEINSTANCE_IID_STR "948c2080-0398-11d3-915e-0000863011c4"
#define NS_IPROXYCREATEINSTANCE_IID \
  {0x948c2080, 0x0398, 0x11d3, \
    { 0x91, 0x5e, 0x00, 0x00, 0x86, 0x30, 0x11, 0xc4 }}

class nsIProxyCreateInstance : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPROXYCREATEINSTANCE_IID)

  /* void CreateInstanceByIID (in nsIIDRef cid, in nsISupports aOuter, in nsIIDRef iid, out voidStar result); */
  NS_IMETHOD CreateInstanceByIID(const nsIID & cid, nsISupports *aOuter, const nsIID & iid, void * *result) = 0;

  /* void CreateInstanceByContractID (in string aContractID, in nsISupports aOuter, in nsIIDRef iid, out voidStar result); */
  NS_IMETHOD CreateInstanceByContractID(const char *aContractID, nsISupports *aOuter, const nsIID & iid, void * *result) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIProxyCreateInstance *priv);
#endif
};

#endif /* __gen_nsIProxyCreateInstance_h__ */
