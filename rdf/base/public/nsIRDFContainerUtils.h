/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFContainerUtils.idl
 */

#ifndef __gen_nsIRDFContainerUtils_h__
#define __gen_nsIRDFContainerUtils_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsISupportsArray.h" /* interface nsISupportsArray */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIRDFNode.h" /* interface nsIRDFNode */
#include "nsIRDFDataSource.h" /* interface nsIRDFDataSource */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIRDFObserver.h" /* interface nsIRDFObserver */
#include "nsIRDFContainer.h" /* interface nsIRDFContainer */
#include "nsIRDFResource.h" /* interface nsIRDFResource */
#include "nsISimpleEnumerator.h" /* interface nsISimpleEnumerator */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIRDFContainerUtils */

/* {D4214E91-FB94-11D2-BDD8-00104BDE6048} */
#define NS_IRDFCONTAINERUTILS_IID_STR "D4214E91-FB94-11D2-BDD8-00104BDE6048"
#define NS_IRDFCONTAINERUTILS_IID \
  {0xD4214E91, 0xFB94, 0x11D2, \
    { 0xBD, 0xD8, 0x00, 0x10, 0x4B, 0xDE, 0x60, 0x48 }}

class nsIRDFContainerUtils : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFCONTAINERUTILS_IID)

  /* boolean IsOrdinalProperty (in nsIRDFResource aProperty); */
  NS_IMETHOD IsOrdinalProperty(nsIRDFResource *aProperty, PRBool *_retval) = 0;

  /* nsIRDFResource IndexToOrdinalResource (in long aIndex); */
  NS_IMETHOD IndexToOrdinalResource(PRInt32 aIndex, nsIRDFResource **_retval) = 0;

  /* long OrdinalResourceToIndex (in nsIRDFResource aOrdinal); */
  NS_IMETHOD OrdinalResourceToIndex(nsIRDFResource *aOrdinal, PRInt32 *_retval) = 0;

  /* boolean IsContainer (in nsIRDFDataSource aDataSource, in nsIRDFResource aResource); */
  NS_IMETHOD IsContainer(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval) = 0;

  /* boolean IsBag (in nsIRDFDataSource aDataSource, in nsIRDFResource aResource); */
  NS_IMETHOD IsBag(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval) = 0;

  /* boolean IsSeq (in nsIRDFDataSource aDataSource, in nsIRDFResource aResource); */
  NS_IMETHOD IsSeq(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval) = 0;

  /* boolean IsAlt (in nsIRDFDataSource aDataSource, in nsIRDFResource aResource); */
  NS_IMETHOD IsAlt(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, PRBool *_retval) = 0;

  /* nsIRDFContainer MakeBag (in nsIRDFDataSource aDataSource, in nsIRDFResource aResource); */
  NS_IMETHOD MakeBag(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, nsIRDFContainer **_retval) = 0;

  /* nsIRDFContainer MakeSeq (in nsIRDFDataSource aDataSource, in nsIRDFResource aResource); */
  NS_IMETHOD MakeSeq(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, nsIRDFContainer **_retval) = 0;

  /* nsIRDFContainer MakeAlt (in nsIRDFDataSource aDataSource, in nsIRDFResource aResource); */
  NS_IMETHOD MakeAlt(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, nsIRDFContainer **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFContainerUtils *priv);
#endif
};
extern nsresult
NS_NewRDFContainerUtils(nsIRDFContainerUtils** aResult);


#endif /* __gen_nsIRDFContainerUtils_h__ */
