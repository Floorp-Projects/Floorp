/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFPurgeableDataSource.idl
 */

#ifndef __gen_nsIRDFPurgeableDataSource_h__
#define __gen_nsIRDFPurgeableDataSource_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIRDFNode.h" /* interface nsIRDFNode */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIRDFResource.h" /* interface nsIRDFResource */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIRDFPurgeableDataSource */

/* {951700F0-FED0-11D2-BDD9-00104BDE6048} */
#define NS_IRDFPURGEABLEDATASOURCE_IID_STR "951700F0-FED0-11D2-BDD9-00104BDE6048"
#define NS_IRDFPURGEABLEDATASOURCE_IID \
  {0x951700F0, 0xFED0, 0x11D2, \
    { 0xBD, 0xD9, 0x00, 0x10, 0x4B, 0xDE, 0x60, 0x48 }}

class nsIRDFPurgeableDataSource : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFPURGEABLEDATASOURCE_IID)

  /* boolean Mark (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD Mark(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval) = 0;

  /* void Sweep (); */
  NS_IMETHOD Sweep() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFPurgeableDataSource *priv);
#endif
};

#endif /* __gen_nsIRDFPurgeableDataSource_h__ */
