/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFResource.idl
 */

#ifndef __gen_nsIRDFResource_h__
#define __gen_nsIRDFResource_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIRDFNode.h" /* interface nsIRDFNode */
#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIRDFResource */

/* {E0C493D1-9542-11d2-8EB8-00805F29F370} */
#define NS_IRDFRESOURCE_IID_STR "E0C493D1-9542-11d2-8EB8-00805F29F370"
#define NS_IRDFRESOURCE_IID \
  {0xE0C493D1, 0x9542, 0x11d2, \
    { 0x8E, 0xB8, 0x00, 0x80, 0x5F, 0x29, 0xF3, 0x70 }}

class nsIRDFResource : public nsIRDFNode {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFRESOURCE_IID)

  /* readonly attribute string Value; */
  NS_IMETHOD GetValue(char * *aValue) = 0;

  /* void Init (in string uri); */
  NS_IMETHOD Init(const char *uri) = 0;

  /* boolean EqualsResource (in nsIRDFResource aResource); */
  NS_IMETHOD EqualsResource(nsIRDFResource *aResource, PRBool *_retval) = 0;

  /* boolean EqualsString (in string aURI); */
  NS_IMETHOD EqualsString(const char *aURI, PRBool *_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFResource *priv);
#endif
};

#endif /* __gen_nsIRDFResource_h__ */
