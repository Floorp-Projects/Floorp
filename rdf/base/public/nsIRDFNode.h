/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFNode.idl
 */

#ifndef __gen_nsIRDFNode_h__
#define __gen_nsIRDFNode_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

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

#endif /* __gen_nsIRDFNode_h__ */
