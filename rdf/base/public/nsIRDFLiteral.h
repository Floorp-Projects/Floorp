/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIRDFLiteral.idl
 */

#ifndef __gen_nsIRDFLiteral_h__
#define __gen_nsIRDFLiteral_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIRDFNode.h" /* interface nsIRDFNode */
#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
#include "nscore.h" // for PRUnichar


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

#endif /* __gen_nsIRDFLiteral_h__ */
