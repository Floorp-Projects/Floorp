/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFService.idl
 */

#ifndef __gen_nsIRDFService_h__
#define __gen_nsIRDFService_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsISupportsArray.h" /* interface nsISupportsArray */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIRDFNode.h" /* interface nsIRDFNode */
#include "nsIRDFDataSource.h" /* interface nsIRDFDataSource */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIRDFObserver.h" /* interface nsIRDFObserver */
#include "nsIRDFResource.h" /* interface nsIRDFResource */
#include "nsIRDFLiteral.h" /* interface nsIRDFLiteral */
#include "nsISimpleEnumerator.h" /* interface nsISimpleEnumerator */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIRDFService */

/* {BFD05261-834C-11d2-8EAC-00805F29F370} */
#define NS_IRDFSERVICE_IID_STR "BFD05261-834C-11d2-8EAC-00805F29F370"
#define NS_IRDFSERVICE_IID \
  {0xBFD05261, 0x834C, 0x11d2, \
    { 0x8E, 0xAC, 0x00, 0x80, 0x5F, 0x29, 0xF3, 0x70 }}

class nsIRDFService : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFSERVICE_IID)

  /* nsIRDFResource GetResource (in string aURI); */
  NS_IMETHOD GetResource(const char *aURI, nsIRDFResource **_retval) = 0;

  /* nsIRDFResource GetUnicodeResource (in wstring aURI); */
  NS_IMETHOD GetUnicodeResource(const PRUnichar *aURI, nsIRDFResource **_retval) = 0;

  /* nsIRDFLiteral GetLiteral (in wstring aValue); */
  NS_IMETHOD GetLiteral(const PRUnichar *aValue, nsIRDFLiteral **_retval) = 0;

  /* nsIRDFDate GetDateLiteral (in long long aValue); */
  NS_IMETHOD GetDateLiteral(PRInt64 aValue, nsIRDFDate **_retval) = 0;

  /* nsIRDFInt GetIntLiteral (in long aValue); */
  NS_IMETHOD GetIntLiteral(PRInt32 aValue, nsIRDFInt **_retval) = 0;

  /* void RegisterResource (in nsIRDFResource aResource, in boolean aReplace); */
  NS_IMETHOD RegisterResource(nsIRDFResource *aResource, PRBool aReplace) = 0;

  /* void UnregisterResource (in nsIRDFResource aResource); */
  NS_IMETHOD UnregisterResource(nsIRDFResource *aResource) = 0;

  /* void RegisterDataSource (in nsIRDFDataSource aDataSource, in boolean aReplace); */
  NS_IMETHOD RegisterDataSource(nsIRDFDataSource *aDataSource, PRBool aReplace) = 0;

  /* void UnregisterDataSource (in nsIRDFDataSource aDataSource); */
  NS_IMETHOD UnregisterDataSource(nsIRDFDataSource *aDataSource) = 0;

  /* nsIRDFDataSource GetDataSource (in string aURI); */
  NS_IMETHOD GetDataSource(const char *aURI, nsIRDFDataSource **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFService *priv);
#endif
};
extern nsresult
NS_NewRDFService(nsIRDFService** result);


#endif /* __gen_nsIRDFService_h__ */
