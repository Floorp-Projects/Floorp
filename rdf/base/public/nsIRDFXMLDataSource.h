/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFXMLDataSource.idl
 */

#ifndef __gen_nsIRDFXMLDataSource_h__
#define __gen_nsIRDFXMLDataSource_h__

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
class nsIAtom;
class nsString;

class nsIRDFXMLDataSource; /* forward decl */

/* starting interface:    nsIRDFXMLDataSourceObserver */

/* {EB1A5D30-AB33-11D2-8EC6-00805F29F370} */
#define NS_IRDFXMLDATASOURCEOBSERVER_IID_STR "EB1A5D30-AB33-11D2-8EC6-00805F29F370"
#define NS_IRDFXMLDATASOURCEOBSERVER_IID \
  {0xEB1A5D30, 0xAB33, 0x11D2, \
    { 0x8E, 0xC6, 0x00, 0x80, 0x5F, 0x29, 0xF3, 0x70 }}

class nsIRDFXMLDataSourceObserver : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFXMLDATASOURCEOBSERVER_IID)

  /* void OnBeginLoad (in nsIRDFXMLDataSource aStream); */
  NS_IMETHOD OnBeginLoad(nsIRDFXMLDataSource *aStream) = 0;

  /* void OnInterrupt (in nsIRDFXMLDataSource aStream); */
  NS_IMETHOD OnInterrupt(nsIRDFXMLDataSource *aStream) = 0;

  /* void OnResume (in nsIRDFXMLDataSource aStream); */
  NS_IMETHOD OnResume(nsIRDFXMLDataSource *aStream) = 0;

  /* void OnEndLoad (in nsIRDFXMLDataSource aStream); */
  NS_IMETHOD OnEndLoad(nsIRDFXMLDataSource *aStream) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFXMLDataSourceObserver *priv);
#endif
};

/* starting interface:    nsIRDFXMLDataSource */

/* {EB1A5D31-AB33-11D2-8EC6-00805F29F370} */
#define NS_IRDFXMLDATASOURCE_IID_STR "EB1A5D31-AB33-11D2-8EC6-00805F29F370"
#define NS_IRDFXMLDATASOURCE_IID \
  {0xEB1A5D31, 0xAB33, 0x11D2, \
    { 0x8E, 0xC6, 0x00, 0x80, 0x5F, 0x29, 0xF3, 0x70 }}

class nsIRDFXMLDataSource : public nsIRDFDataSource {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFXMLDATASOURCE_IID)

  /* attribute boolean ReadOnly; */
  NS_IMETHOD GetReadOnly(PRBool *aReadOnly) = 0;
  NS_IMETHOD SetReadOnly(PRBool aReadOnly) = 0;

  /* void Open (in boolean aBlocking); */
  NS_IMETHOD Open(PRBool aBlocking) = 0;

  /* void BeginLoad (); */
  NS_IMETHOD BeginLoad() = 0;

  /* void Interrupt (); */
  NS_IMETHOD Interrupt() = 0;

  /* void Resume (); */
  NS_IMETHOD Resume() = 0;

  /* void EndLoad (); */
  NS_IMETHOD EndLoad() = 0;

  /* void AddNameSpace (in nsIAtomPtr aPrefix, [const] in nsStringRef aURI); */
  NS_IMETHOD AddNameSpace(nsIAtom * aPrefix, const nsString & aURI) = 0;

  /* void AddXMLStreamObserver (in nsIRDFXMLDataSourceObserver aObserver); */
  NS_IMETHOD AddXMLStreamObserver(nsIRDFXMLDataSourceObserver *aObserver) = 0;

  /* void RemoveXMLStreamObserver (in nsIRDFXMLDataSourceObserver aObserver); */
  NS_IMETHOD RemoveXMLStreamObserver(nsIRDFXMLDataSourceObserver *aObserver) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFXMLDataSource *priv);
#endif
};
// Two different kinds of XML data sources exist for RDF: serialized RDF and XUL.
// These are the methods that make these data sources.
extern nsresult
NS_NewRDFXMLDataSource(nsIRDFXMLDataSource** result);
extern nsresult
NS_NewXULDataSource(nsIRDFXMLDataSource** result);


#endif /* __gen_nsIRDFXMLDataSource_h__ */
