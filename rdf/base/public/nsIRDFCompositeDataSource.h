/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFCompositeDataSource.idl
 */

#ifndef __gen_nsIRDFCompositeDataSource_h__
#define __gen_nsIRDFCompositeDataSource_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsISupportsArray.h" /* interface nsISupportsArray */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIRDFNode.h" /* interface nsIRDFNode */
#include "nsIRDFDataSource.h" /* interface nsIRDFDataSource */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIRDFObserver.h" /* interface nsIRDFObserver */
#include "nsIRDFResource.h" /* interface nsIRDFResource */
#include "nsISimpleEnumerator.h" /* interface nsISimpleEnumerator */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIRDFCompositeDataSource */

/* {96343820-307C-11D2-BC15-00805F912FE7} */
#define NS_IRDFCOMPOSITEDATASOURCE_IID_STR "96343820-307C-11D2-BC15-00805F912FE7"
#define NS_IRDFCOMPOSITEDATASOURCE_IID \
  {0x96343820, 0x307C, 0x11D2, \
    { 0xBC, 0x15, 0x00, 0x80, 0x5F, 0x91, 0x2F, 0xE7 }}

class nsIRDFCompositeDataSource : public nsIRDFDataSource {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFCOMPOSITEDATASOURCE_IID)

  /* void AddDataSource (in nsIRDFDataSource aDataSource); */
  NS_IMETHOD AddDataSource(nsIRDFDataSource *aDataSource) = 0;

  /* void RemoveDataSource (in nsIRDFDataSource aDataSource); */
  NS_IMETHOD RemoveDataSource(nsIRDFDataSource *aDataSource) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFCompositeDataSource *priv);
#endif
};
extern nsresult
NS_NewRDFCompositeDataSource(nsIRDFCompositeDataSource** result);


#endif /* __gen_nsIRDFCompositeDataSource_h__ */
