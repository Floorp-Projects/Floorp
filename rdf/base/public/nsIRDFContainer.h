/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFContainer.idl
 */

#ifndef __gen_nsIRDFContainer_h__
#define __gen_nsIRDFContainer_h__

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

/* starting interface:    nsIRDFContainer */

/* {D4214E90-FB94-11D2-BDD8-00104BDE6048} */
#define NS_IRDFCONTAINER_IID_STR "D4214E90-FB94-11D2-BDD8-00104BDE6048"
#define NS_IRDFCONTAINER_IID \
  {0xD4214E90, 0xFB94, 0x11D2, \
    { 0xBD, 0xD8, 0x00, 0x10, 0x4B, 0xDE, 0x60, 0x48 }}

class nsIRDFContainer : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFCONTAINER_IID)

  /* void Init (in nsIRDFDataSource aDataSource, in nsIRDFResource aContainer); */
  NS_IMETHOD Init(nsIRDFDataSource *aDataSource, nsIRDFResource *aContainer) = 0;

  /* long GetCount (); */
  NS_IMETHOD GetCount(PRInt32 *_retval) = 0;

  /* nsISimpleEnumerator GetElements (); */
  NS_IMETHOD GetElements(nsISimpleEnumerator **_retval) = 0;

  /* void AppendElement (in nsIRDFNode aElement); */
  NS_IMETHOD AppendElement(nsIRDFNode *aElement) = 0;

  /* void RemoveElement (in nsIRDFNode aElement, in boolean aRenumber); */
  NS_IMETHOD RemoveElement(nsIRDFNode *aElement, PRBool aRenumber) = 0;

  /* void InsertElementAt (in nsIRDFNode aElement, in long aIndex, in boolean aRenumber); */
  NS_IMETHOD InsertElementAt(nsIRDFNode *aElement, PRInt32 aIndex, PRBool aRenumber) = 0;

  /* long IndexOf (in nsIRDFNode aElement); */
  NS_IMETHOD IndexOf(nsIRDFNode *aElement, PRInt32 *_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFContainer *priv);
#endif
};
PR_EXTERN(nsresult)
NS_NewRDFContainer(nsIRDFContainer** aResult);
PR_EXTERN(nsresult)
NS_NewRDFContainer(nsIRDFDataSource* aDataSource,
                   nsIRDFResource* aResource,
                   nsIRDFContainer** aResult);
/**
 * Create a cursor on a container that enumerates its contents in
 * order
 */
PR_EXTERN(nsresult)
NS_NewContainerEnumerator(nsIRDFDataSource* aDataSource,
                          nsIRDFResource* aContainer,
                          nsISimpleEnumerator** aResult);


#endif /* __gen_nsIRDFContainer_h__ */
