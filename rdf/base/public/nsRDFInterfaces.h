/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsRDFInterfaces.idl
 */

#ifndef __gen_nsRDFInterfaces_h__
#define __gen_nsRDFInterfaces_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsISupportsArray.h" /* interface nsISupportsArray */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsISimpleEnumerator.h" /* interface nsISimpleEnumerator */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
#include "nscore.h" // for PRUnichar


/* starting interface:    nsIRDFNode */

/* {0F78DA50-8321-11d2-8EAC-00805F29F370} */
#define NS_IRDFNODE_IID_STR "0F78DA50-8321-11d2-8EAC-00805F29F370"
#define NS_IRDFNODE_IID \
  {0x0F78DA50, 0x8321, 0x11d2, \
    { 0x8E, 0xAC, 0x00, 0x80, 0x5F, 0x29, 0xF3, 0x70 }}

class nsIRDFNode : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFNODE_IID)

  /* boolean EqualsNode (in nsIRDFNode aNode); */
  NS_IMETHOD EqualsNode(nsIRDFNode *aNode, PRBool *_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFNode *priv);
#endif
};

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

/* starting interface:    nsIRDFLiteral */

/* {E0C493D2-9542-11d2-8EB8-00805F29F370} */
#define NS_IRDFLITERAL_IID_STR "E0C493D2-9542-11d2-8EB8-00805F29F370"
#define NS_IRDFLITERAL_IID \
  {0xE0C493D2, 0x9542, 0x11d2, \
    { 0x8E, 0xB8, 0x00, 0x80, 0x5F, 0x29, 0xF3, 0x70 }}

class nsIRDFLiteral : public nsIRDFNode {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFLITERAL_IID)

  /* readonly attribute wstring Value; */
  NS_IMETHOD GetValue(PRUnichar * *aValue) = 0;

  /* boolean EqualsLiteral (in nsIRDFLiteral aLiteral); */
  NS_IMETHOD EqualsLiteral(nsIRDFLiteral *aLiteral, PRBool *_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFLiteral *priv);
#endif
};

/* starting interface:    nsIRDFDate */

/* {E13A24E1-C77A-11d2-80BE-006097B76B8E} */
#define NS_IRDFDATE_IID_STR "E13A24E1-C77A-11d2-80BE-006097B76B8E"
#define NS_IRDFDATE_IID \
  {0xE13A24E1, 0xC77A, 0x11d2, \
    { 0x80, 0xBE, 0x00, 0x60, 0x97, 0xB7, 0x6B, 0x8E }}

class nsIRDFDate : public nsIRDFNode {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFDATE_IID)

  /* readonly attribute long long Value; */
  NS_IMETHOD GetValue(PRInt64 *aValue) = 0;

  /* boolean EqualsDate (in nsIRDFDate aDate); */
  NS_IMETHOD EqualsDate(nsIRDFDate *aDate, PRBool *_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFDate *priv);
#endif
};

/* starting interface:    nsIRDFInt */

/* {E13A24E3-C77A-11d2-80BE-006097B76B8E} */
#define NS_IRDFINT_IID_STR "E13A24E3-C77A-11d2-80BE-006097B76B8E"
#define NS_IRDFINT_IID \
  {0xE13A24E3, 0xC77A, 0x11d2, \
    { 0x80, 0xBE, 0x00, 0x60, 0x97, 0xB7, 0x6B, 0x8E }}

class nsIRDFInt : public nsIRDFNode {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRDFINT_IID)

  /* readonly attribute long Value; */
  NS_IMETHOD GetValue(PRInt32 *aValue) = 0;

  /* boolean EqualsInt (in nsIRDFInt aInt); */
  NS_IMETHOD EqualsInt(nsIRDFInt *aInt, PRBool *_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIRDFInt *priv);
#endif
};
class nsIRDFDataSource; /* forward decl */

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

#endif /* __gen_nsRDFInterfaces_h__ */
