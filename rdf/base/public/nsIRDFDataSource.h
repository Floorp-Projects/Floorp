/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFDataSource.idl
 */

#ifndef __gen_nsIRDFDataSource_h__
#define __gen_nsIRDFDataSource_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsISupportsArray.h" /* interface nsISupportsArray */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIRDFNode.h" /* interface nsIRDFNode */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIRDFObserver.h" /* interface nsIRDFObserver */
#include "nsIRDFResource.h" /* interface nsIRDFResource */
#include "nsISimpleEnumerator.h" /* interface nsISimpleEnumerator */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIRDFDataSource */

/* {0F78DA58-8321-11d2-8EAC-00805F29F370} */
#define NS_IRDFDATASOURCE_IID_STR "0F78DA58-8321-11d2-8EAC-00805F29F370"
#define NS_IRDFDATASOURCE_IID \
  {0x0F78DA58, 0x8321, 0x11d2, \
    { 0x8E, 0xAC, 0x00, 0x80, 0x5F, 0x29, 0xF3, 0x70 }}

class nsIRDFDataSource : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFDATASOURCE_IID)

  /* void Init (in string uri); */
  NS_IMETHOD Init(const char *uri) = 0;

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI) = 0;

  /* nsIRDFResource GetSource (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **_retval) = 0;

  /* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval) = 0;

  /* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
  NS_IMETHOD GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **_retval) = 0;

  /* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval) = 0;

  /* void Assert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue) = 0;

  /* void Unassert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
  NS_IMETHOD Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget) = 0;

  /* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval) = 0;

  /* void AddObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD AddObserver(nsIRDFObserver *aObserver) = 0;

  /* void RemoveObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD RemoveObserver(nsIRDFObserver *aObserver) = 0;

  /* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
  NS_IMETHOD ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval) = 0;

  /* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval) = 0;

  /* nsISimpleEnumerator GetAllResources (); */
  NS_IMETHOD GetAllResources(nsISimpleEnumerator **_retval) = 0;

  /* void Flush (); */
  NS_IMETHOD Flush() = 0;

  /* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
  NS_IMETHOD GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval) = 0;

  /* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
  NS_IMETHOD IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments, PRBool *_retval) = 0;

  /* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
  NS_IMETHOD DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFDataSource *priv);
#endif
};

#endif /* __gen_nsIRDFDataSource_h__ */
