/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFXMLSource.idl
 */

#ifndef __gen_nsIRDFXMLSource_h__
#define __gen_nsIRDFXMLSource_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
class nsIOutputStream;


/* starting interface:    nsIRDFXMLSource */

/* {4DA56F10-99FE-11d2-8EBB-00805F29F370} */
#define NS_IRDFXMLSOURCE_IID_STR "4DA56F10-99FE-11d2-8EBB-00805F29F370"
#define NS_IRDFXMLSOURCE_IID \
  {0x4DA56F10, 0x99FE, 0x11d2, \
    { 0x8E, 0xBB, 0x00, 0x80, 0x5F, 0x29, 0xF3, 0x70 }}

class nsIRDFXMLSource : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFXMLSOURCE_IID)

  /* void Serialize (in nsIOutputStreamPtr aStream); */
  NS_IMETHOD Serialize(nsIOutputStream * aStream) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFXMLSource *priv);
#endif
};

#endif /* __gen_nsIRDFXMLSource_h__ */
