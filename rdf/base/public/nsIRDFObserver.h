/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFObserver.idl
 */

#ifndef __gen_nsIRDFObserver_h__
#define __gen_nsIRDFObserver_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIRDFNode.h" /* interface nsIRDFNode */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIRDFResource.h" /* interface nsIRDFResource */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIRDFObserver */

/* {3CC75360-484A-11D2-BC16-00805F912FE7} */
#define NS_IRDFOBSERVER_IID_STR "3CC75360-484A-11D2-BC16-00805F912FE7"
#define NS_IRDFOBSERVER_IID \
  {0x3CC75360, 0x484A, 0x11D2, \
    { 0xBC, 0x16, 0x00, 0x80, 0x5F, 0x91, 0x2F, 0xE7 }}

class nsIRDFObserver : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFOBSERVER_IID)

  /* void OnAssert (in nsIRDFResource aSource, in nsIRDFResource aLabel, in nsIRDFNode aTarget); */
  NS_IMETHOD OnAssert(nsIRDFResource *aSource, nsIRDFResource *aLabel, nsIRDFNode *aTarget) = 0;

  /* void OnUnassert (in nsIRDFResource aSource, in nsIRDFResource aLabel, in nsIRDFNode aTarget); */
  NS_IMETHOD OnUnassert(nsIRDFResource *aSource, nsIRDFResource *aLabel, nsIRDFNode *aTarget) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFObserver *priv);
#endif
};

#endif /* __gen_nsIRDFObserver_h__ */
