/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsRDFInterfaces.idl
 */
#include "jsapi.h"
#include "nsRDFInterfaces.h"

static char XXXnsresult2string_fmt[] = "XPCOM error %#x";
#define XXXnsresult2string(res) XXXnsresult2string_fmt, res

/* boolean EqualsNode (in nsIRDFNode aNode); */
static JSBool
nsIRDFNode_EqualsNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFNode *priv = (nsIRDFNode *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFNode *aNode;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aNode))
    return JS_FALSE;
  PRBool retval;
  nsresult rv = priv->EqualsNode(aNode, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = BOOLEAN_TO_JSVAL(retval);
  return JS_TRUE;
}

static JSFunctionSpec nsIRDFNode_funcs[] = {
  {"EqualsNode", nsIRDFNode_EqualsNode, 1},
  {0}
};

static void
nsIRDFNode_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFNode *priv = (nsIRDFNode *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFNode_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFNode_class = {
  "nsIRDFNode",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFNode_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFNode::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFNode_class, nsIRDFNode_ctor, 0,
                                 0, nsIRDFNode_funcs, 0, 0);
  return proto;
}

JSObject *
nsIRDFNode::GetJSObject(JSContext *cx, nsIRDFNode *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFNode_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

/* void Init (in string uri); */
static JSBool
nsIRDFResource_Init(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFResource *priv = (nsIRDFResource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  const char *uri;
  if (!JS_ConvertArguments(cx, argc, argv, "s", &uri))
    return JS_FALSE;
  nsresult rv = priv->Init(uri);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* boolean EqualsResource (in nsIRDFResource aResource); */
static JSBool
nsIRDFResource_EqualsResource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFResource *priv = (nsIRDFResource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aResource;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aResource))
    return JS_FALSE;
  PRBool retval;
  nsresult rv = priv->EqualsResource(aResource, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = BOOLEAN_TO_JSVAL(retval);
  return JS_TRUE;
}

/* boolean EqualsString (in string aURI); */
static JSBool
nsIRDFResource_EqualsString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFResource *priv = (nsIRDFResource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  const char *aURI;
  if (!JS_ConvertArguments(cx, argc, argv, "s", &aURI))
    return JS_FALSE;
  PRBool retval;
  nsresult rv = priv->EqualsString(aURI, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = BOOLEAN_TO_JSVAL(retval);
  return JS_TRUE;
}

static JSFunctionSpec nsIRDFResource_funcs[] = {
  {"Init", nsIRDFResource_Init, 1},
  {"EqualsResource", nsIRDFResource_EqualsResource, 1},
  {"EqualsString", nsIRDFResource_EqualsString, 1},
  {0}
};

static JSPropertySpec nsIRDFResource_props[] = {
  {"Value", -1, JSPROP_READONLY},
  {0}
};

static JSBool
nsIRDFResource_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (!JSVAL_IS_INT(id))
    return JS_TRUE;
  nsresult result;
  nsIRDFResource *priv = (nsIRDFResource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  switch (JSVAL_TO_INT(id)) {
   case -1:
    char * Value;
    result = priv->GetValue(&Value);
    if (NS_FAILED(result))
      goto bad;
    JSString *str = JS_NewStringCopyZ(cx, Value);
    if (!str)
      return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    break;
  }
  return JS_TRUE;
bad:
  JS_ReportError(cx, XXXnsresult2string(result));
  return JS_FALSE;
}

static void
nsIRDFResource_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFResource *priv = (nsIRDFResource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFResource_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFResource_class = {
  "nsIRDFResource",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, nsIRDFResource_GetProperty, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFResource_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFResource::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFResource_class, nsIRDFResource_ctor, 0,
                                 nsIRDFResource_props, nsIRDFResource_funcs, 0, 0);
  return proto;
}

JSObject *
nsIRDFResource::GetJSObject(JSContext *cx, nsIRDFResource *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFResource_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

/* boolean EqualsLiteral (in nsIRDFLiteral aLiteral); */
static JSBool
nsIRDFLiteral_EqualsLiteral(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFLiteral *priv = (nsIRDFLiteral *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFLiteral *aLiteral;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aLiteral))
    return JS_FALSE;
  PRBool retval;
  nsresult rv = priv->EqualsLiteral(aLiteral, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = BOOLEAN_TO_JSVAL(retval);
  return JS_TRUE;
}

static JSFunctionSpec nsIRDFLiteral_funcs[] = {
  {"EqualsLiteral", nsIRDFLiteral_EqualsLiteral, 1},
  {0}
};

static JSPropertySpec nsIRDFLiteral_props[] = {
  {"Value", -1, JSPROP_READONLY},
  {0}
};

static JSBool
nsIRDFLiteral_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (!JSVAL_IS_INT(id))
    return JS_TRUE;
  nsresult result;
  nsIRDFLiteral *priv = (nsIRDFLiteral *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  switch (JSVAL_TO_INT(id)) {
   case -1:
    PRUnichar * Value;
    result = priv->GetValue(&Value);
    if (NS_FAILED(result))
      goto bad;
    JSString *str = JS_NewUCStringCopyZ(cx, Value);
    if (!str)
      return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    break;
  }
  return JS_TRUE;
bad:
  JS_ReportError(cx, XXXnsresult2string(result));
  return JS_FALSE;
}

static void
nsIRDFLiteral_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFLiteral *priv = (nsIRDFLiteral *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFLiteral_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFLiteral_class = {
  "nsIRDFLiteral",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, nsIRDFLiteral_GetProperty, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFLiteral_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFLiteral::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFLiteral_class, nsIRDFLiteral_ctor, 0,
                                 nsIRDFLiteral_props, nsIRDFLiteral_funcs, 0, 0);
  return proto;
}

JSObject *
nsIRDFLiteral::GetJSObject(JSContext *cx, nsIRDFLiteral *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFLiteral_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

/* boolean EqualsDate (in nsIRDFDate aDate); */
static JSBool
nsIRDFDate_EqualsDate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDate *priv = (nsIRDFDate *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFDate *aDate;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aDate))
    return JS_FALSE;
  PRBool retval;
  nsresult rv = priv->EqualsDate(aDate, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = BOOLEAN_TO_JSVAL(retval);
  return JS_TRUE;
}

static JSFunctionSpec nsIRDFDate_funcs[] = {
  {"EqualsDate", nsIRDFDate_EqualsDate, 1},
  {0}
};

static JSPropertySpec nsIRDFDate_props[] = {
  {"Value", -1, JSPROP_READONLY},
  {0}
};

static JSBool
nsIRDFDate_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (!JSVAL_IS_INT(id))
    return JS_TRUE;
  nsresult result;
  nsIRDFDate *priv = (nsIRDFDate *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  switch (JSVAL_TO_INT(id)) {
   case -1:
    PRTime Value;
    result = priv->GetValue(&Value);
    if (NS_FAILED(result))
      goto bad;
    *vp = JSVAL_NULL;
    break;
  }
  return JS_TRUE;
bad:
  JS_ReportError(cx, XXXnsresult2string(result));
  return JS_FALSE;
}

static void
nsIRDFDate_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFDate *priv = (nsIRDFDate *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFDate_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFDate_class = {
  "nsIRDFDate",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, nsIRDFDate_GetProperty, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFDate_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFDate::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFDate_class, nsIRDFDate_ctor, 0,
                                 nsIRDFDate_props, nsIRDFDate_funcs, 0, 0);
  return proto;
}

JSObject *
nsIRDFDate::GetJSObject(JSContext *cx, nsIRDFDate *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFDate_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

/* boolean EqualsInt (in nsIRDFInt aInt); */
static JSBool
nsIRDFInt_EqualsInt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFInt *priv = (nsIRDFInt *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFInt *aInt;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aInt))
    return JS_FALSE;
  PRBool retval;
  nsresult rv = priv->EqualsInt(aInt, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = BOOLEAN_TO_JSVAL(retval);
  return JS_TRUE;
}

static JSFunctionSpec nsIRDFInt_funcs[] = {
  {"EqualsInt", nsIRDFInt_EqualsInt, 1},
  {0}
};

static JSPropertySpec nsIRDFInt_props[] = {
  {"Value", -1, JSPROP_READONLY},
  {0}
};

static JSBool
nsIRDFInt_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (!JSVAL_IS_INT(id))
    return JS_TRUE;
  nsresult result;
  nsIRDFInt *priv = (nsIRDFInt *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  switch (JSVAL_TO_INT(id)) {
   case -1:
    PRInt32 Value;
    result = priv->GetValue(&Value);
    if (NS_FAILED(result))
      goto bad;
    if (!JS_NewNumberValue(cx, (jsdouble) Value, vp))
      return JS_FALSE;
    break;
  }
  return JS_TRUE;
bad:
  JS_ReportError(cx, XXXnsresult2string(result));
  return JS_FALSE;
}

static void
nsIRDFInt_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFInt *priv = (nsIRDFInt *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFInt_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFInt_class = {
  "nsIRDFInt",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, nsIRDFInt_GetProperty, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFInt_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFInt::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFInt_class, nsIRDFInt_ctor, 0,
                                 nsIRDFInt_props, nsIRDFInt_funcs, 0, 0);
  return proto;
}

JSObject *
nsIRDFInt::GetJSObject(JSContext *cx, nsIRDFInt *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFInt_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

/* void Advance (); */
static JSBool
nsIRDFCursor_Advance(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFCursor *priv = (nsIRDFCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  if (!JS_ConvertArguments(cx, argc, argv, ""))
    return JS_FALSE;
  nsresult rv = priv->Advance();
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

static JSFunctionSpec nsIRDFCursor_funcs[] = {
  {"Advance", nsIRDFCursor_Advance, 0},
  {0}
};

static JSPropertySpec nsIRDFCursor_props[] = {
  {"DataSource", -1, JSPROP_READONLY},
  {"Value", -2, JSPROP_READONLY},
  {0}
};

static JSBool
nsIRDFCursor_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (!JSVAL_IS_INT(id))
    return JS_TRUE;
  nsresult result;
  nsIRDFCursor *priv = (nsIRDFCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  switch (JSVAL_TO_INT(id)) {
   case -1:
    nsIRDFDataSource * DataSource;
    result = priv->GetDataSource(&DataSource);
    if (NS_FAILED(result))
      goto bad;
    *vp = OBJECT_TO_JSVAL(nsIRDFDataSource::GetJSObject(cx, DataSource));
    NS_RELEASE(DataSource);
    break;
   case -2:
    nsIRDFNode * Value;
    result = priv->GetValue(&Value);
    if (NS_FAILED(result))
      goto bad;
    *vp = OBJECT_TO_JSVAL(nsIRDFNode::GetJSObject(cx, Value));
    NS_RELEASE(Value);
    break;
  }
  return JS_TRUE;
bad:
  JS_ReportError(cx, XXXnsresult2string(result));
  return JS_FALSE;
}

static void
nsIRDFCursor_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFCursor *priv = (nsIRDFCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFCursor_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFCursor_class = {
  "nsIRDFCursor",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, nsIRDFCursor_GetProperty, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFCursor_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFCursor::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFCursor_class, nsIRDFCursor_ctor, 0,
                                 nsIRDFCursor_props, nsIRDFCursor_funcs, 0, 0);
  return proto;
}

JSObject *
nsIRDFCursor::GetJSObject(JSContext *cx, nsIRDFCursor *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFCursor_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

static JSPropertySpec nsIRDFAssertionCursor_props[] = {
  {"Source", -1, JSPROP_READONLY},
  {"Label", -2, JSPROP_READONLY},
  {"Target", -3, JSPROP_READONLY},
  {"TruthValue", -4, JSPROP_READONLY},
  {0}
};

static JSBool
nsIRDFAssertionCursor_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (!JSVAL_IS_INT(id))
    return JS_TRUE;
  nsresult result;
  nsIRDFAssertionCursor *priv = (nsIRDFAssertionCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  switch (JSVAL_TO_INT(id)) {
   case -1:
    nsIRDFResource * Source;
    result = priv->GetSource(&Source);
    if (NS_FAILED(result))
      goto bad;
    *vp = OBJECT_TO_JSVAL(nsIRDFResource::GetJSObject(cx, Source));
    NS_RELEASE(Source);
    break;
   case -2:
    nsIRDFResource * Label;
    result = priv->GetLabel(&Label);
    if (NS_FAILED(result))
      goto bad;
    *vp = OBJECT_TO_JSVAL(nsIRDFResource::GetJSObject(cx, Label));
    NS_RELEASE(Label);
    break;
   case -3:
    nsIRDFNode * Target;
    result = priv->GetTarget(&Target);
    if (NS_FAILED(result))
      goto bad;
    *vp = OBJECT_TO_JSVAL(nsIRDFNode::GetJSObject(cx, Target));
    NS_RELEASE(Target);
    break;
   case -4:
    PRBool TruthValue;
    result = priv->GetTruthValue(&TruthValue);
    if (NS_FAILED(result))
      goto bad;
    *vp = BOOLEAN_TO_JSVAL(TruthValue);
    break;
  }
  return JS_TRUE;
bad:
  JS_ReportError(cx, XXXnsresult2string(result));
  return JS_FALSE;
}

static void
nsIRDFAssertionCursor_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFAssertionCursor *priv = (nsIRDFAssertionCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFAssertionCursor_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFAssertionCursor_class = {
  "nsIRDFAssertionCursor",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, nsIRDFAssertionCursor_GetProperty, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFAssertionCursor_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFAssertionCursor::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFAssertionCursor_class, nsIRDFAssertionCursor_ctor, 0,
                                 nsIRDFAssertionCursor_props, 0, 0, 0);
  return proto;
}

JSObject *
nsIRDFAssertionCursor::GetJSObject(JSContext *cx, nsIRDFAssertionCursor *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFAssertionCursor_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

static JSPropertySpec nsIRDFArcsInCursor_props[] = {
  {"Label", -1, JSPROP_READONLY},
  {"Target", -2, JSPROP_READONLY},
  {0}
};

static JSBool
nsIRDFArcsInCursor_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (!JSVAL_IS_INT(id))
    return JS_TRUE;
  nsresult result;
  nsIRDFArcsInCursor *priv = (nsIRDFArcsInCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  switch (JSVAL_TO_INT(id)) {
   case -1:
    nsIRDFResource * Label;
    result = priv->GetLabel(&Label);
    if (NS_FAILED(result))
      goto bad;
    *vp = OBJECT_TO_JSVAL(nsIRDFResource::GetJSObject(cx, Label));
    NS_RELEASE(Label);
    break;
   case -2:
    nsIRDFNode * Target;
    result = priv->GetTarget(&Target);
    if (NS_FAILED(result))
      goto bad;
    *vp = OBJECT_TO_JSVAL(nsIRDFNode::GetJSObject(cx, Target));
    NS_RELEASE(Target);
    break;
  }
  return JS_TRUE;
bad:
  JS_ReportError(cx, XXXnsresult2string(result));
  return JS_FALSE;
}

static void
nsIRDFArcsInCursor_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFArcsInCursor *priv = (nsIRDFArcsInCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFArcsInCursor_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFArcsInCursor_class = {
  "nsIRDFArcsInCursor",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, nsIRDFArcsInCursor_GetProperty, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFArcsInCursor_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFArcsInCursor::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFArcsInCursor_class, nsIRDFArcsInCursor_ctor, 0,
                                 nsIRDFArcsInCursor_props, 0, 0, 0);
  return proto;
}

JSObject *
nsIRDFArcsInCursor::GetJSObject(JSContext *cx, nsIRDFArcsInCursor *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFArcsInCursor_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

static JSPropertySpec nsIRDFArcsOutCursor_props[] = {
  {"Source", -1, JSPROP_READONLY},
  {"Label", -2, JSPROP_READONLY},
  {0}
};

static JSBool
nsIRDFArcsOutCursor_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (!JSVAL_IS_INT(id))
    return JS_TRUE;
  nsresult result;
  nsIRDFArcsOutCursor *priv = (nsIRDFArcsOutCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  switch (JSVAL_TO_INT(id)) {
   case -1:
    nsIRDFResource * Source;
    result = priv->GetSource(&Source);
    if (NS_FAILED(result))
      goto bad;
    *vp = OBJECT_TO_JSVAL(nsIRDFResource::GetJSObject(cx, Source));
    NS_RELEASE(Source);
    break;
   case -2:
    nsIRDFResource * Label;
    result = priv->GetLabel(&Label);
    if (NS_FAILED(result))
      goto bad;
    *vp = OBJECT_TO_JSVAL(nsIRDFResource::GetJSObject(cx, Label));
    NS_RELEASE(Label);
    break;
  }
  return JS_TRUE;
bad:
  JS_ReportError(cx, XXXnsresult2string(result));
  return JS_FALSE;
}

static void
nsIRDFArcsOutCursor_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFArcsOutCursor *priv = (nsIRDFArcsOutCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFArcsOutCursor_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFArcsOutCursor_class = {
  "nsIRDFArcsOutCursor",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, nsIRDFArcsOutCursor_GetProperty, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFArcsOutCursor_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFArcsOutCursor::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFArcsOutCursor_class, nsIRDFArcsOutCursor_ctor, 0,
                                 nsIRDFArcsOutCursor_props, 0, 0, 0);
  return proto;
}

JSObject *
nsIRDFArcsOutCursor::GetJSObject(JSContext *cx, nsIRDFArcsOutCursor *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFArcsOutCursor_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

static JSPropertySpec nsIRDFResourceCursor_props[] = {
  {"Resource", -1, JSPROP_READONLY},
  {0}
};

static JSBool
nsIRDFResourceCursor_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (!JSVAL_IS_INT(id))
    return JS_TRUE;
  nsresult result;
  nsIRDFResourceCursor *priv = (nsIRDFResourceCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  switch (JSVAL_TO_INT(id)) {
   case -1:
    nsIRDFResource * Resource;
    result = priv->GetResource(&Resource);
    if (NS_FAILED(result))
      goto bad;
    *vp = OBJECT_TO_JSVAL(nsIRDFResource::GetJSObject(cx, Resource));
    NS_RELEASE(Resource);
    break;
  }
  return JS_TRUE;
bad:
  JS_ReportError(cx, XXXnsresult2string(result));
  return JS_FALSE;
}

static void
nsIRDFResourceCursor_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFResourceCursor *priv = (nsIRDFResourceCursor *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFResourceCursor_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFResourceCursor_class = {
  "nsIRDFResourceCursor",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, nsIRDFResourceCursor_GetProperty, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFResourceCursor_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFResourceCursor::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFResourceCursor_class, nsIRDFResourceCursor_ctor, 0,
                                 nsIRDFResourceCursor_props, 0, 0, 0);
  return proto;
}

JSObject *
nsIRDFResourceCursor::GetJSObject(JSContext *cx, nsIRDFResourceCursor *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFResourceCursor_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

/* void OnAssert (in nsIRDFResource aSource, in nsIRDFResource aLabel, in nsIRDFNode aTarget); */
static JSBool
nsIRDFObserver_OnAssert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFObserver *priv = (nsIRDFObserver *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aSource;
  nsIRDFResource *aLabel;
  nsIRDFNode *aTarget;
  if (!JS_ConvertArguments(cx, argc, argv, "ooo", &aSource, &aLabel, &aTarget))
    return JS_FALSE;
  nsresult rv = priv->OnAssert(aSource, aLabel, aTarget);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* void OnUnassert (in nsIRDFResource aSource, in nsIRDFResource aLabel, in nsIRDFNode aTarget); */
static JSBool
nsIRDFObserver_OnUnassert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFObserver *priv = (nsIRDFObserver *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aSource;
  nsIRDFResource *aLabel;
  nsIRDFNode *aTarget;
  if (!JS_ConvertArguments(cx, argc, argv, "ooo", &aSource, &aLabel, &aTarget))
    return JS_FALSE;
  nsresult rv = priv->OnUnassert(aSource, aLabel, aTarget);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

static JSFunctionSpec nsIRDFObserver_funcs[] = {
  {"OnAssert", nsIRDFObserver_OnAssert, 3},
  {"OnUnassert", nsIRDFObserver_OnUnassert, 3},
  {0}
};

static void
nsIRDFObserver_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFObserver *priv = (nsIRDFObserver *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFObserver_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFObserver_class = {
  "nsIRDFObserver",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFObserver_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFObserver::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFObserver_class, nsIRDFObserver_ctor, 0,
                                 0, nsIRDFObserver_funcs, 0, 0);
  return proto;
}

JSObject *
nsIRDFObserver::GetJSObject(JSContext *cx, nsIRDFObserver *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFObserver_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

/* void Init (in string uri); */
static JSBool
nsIRDFDataSource_Init(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  const char *uri;
  if (!JS_ConvertArguments(cx, argc, argv, "s", &uri))
    return JS_FALSE;
  nsresult rv = priv->Init(uri);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* nsIRDFResource GetSource (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
static JSBool
nsIRDFDataSource_GetSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aProperty;
  nsIRDFNode *aTarget;
  PRBool aTruthValue;
  if (!JS_ConvertArguments(cx, argc, argv, "oob", &aProperty, &aTarget, &aTruthValue))
    return JS_FALSE;
  nsIRDFResource * retval;
  nsresult rv = priv->GetSource(aProperty, aTarget, aTruthValue, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFResource::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* nsIRDFAssertionCursor GetSources (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
static JSBool
nsIRDFDataSource_GetSources(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aProperty;
  nsIRDFNode *aTarget;
  PRBool aTruthValue;
  if (!JS_ConvertArguments(cx, argc, argv, "oob", &aProperty, &aTarget, &aTruthValue))
    return JS_FALSE;
  nsIRDFAssertionCursor * retval;
  nsresult rv = priv->GetSources(aProperty, aTarget, aTruthValue, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFAssertionCursor::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
static JSBool
nsIRDFDataSource_GetTarget(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aSource;
  nsIRDFResource *aProperty;
  PRBool aTruthValue;
  if (!JS_ConvertArguments(cx, argc, argv, "oob", &aSource, &aProperty, &aTruthValue))
    return JS_FALSE;
  nsIRDFNode * retval;
  nsresult rv = priv->GetTarget(aSource, aProperty, aTruthValue, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFNode::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* nsIRDFAssertionCursor GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
static JSBool
nsIRDFDataSource_GetTargets(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aSource;
  nsIRDFResource *aProperty;
  PRBool aTruthValue;
  if (!JS_ConvertArguments(cx, argc, argv, "oob", &aSource, &aProperty, &aTruthValue))
    return JS_FALSE;
  nsIRDFAssertionCursor * retval;
  nsresult rv = priv->GetTargets(aSource, aProperty, aTruthValue, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFAssertionCursor::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* void Assert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
static JSBool
nsIRDFDataSource_Assert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aSource;
  nsIRDFResource *aProperty;
  nsIRDFNode *aTarget;
  PRBool aTruthValue;
  if (!JS_ConvertArguments(cx, argc, argv, "ooob", &aSource, &aProperty, &aTarget, &aTruthValue))
    return JS_FALSE;
  nsresult rv = priv->Assert(aSource, aProperty, aTarget, aTruthValue);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* void Unassert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
static JSBool
nsIRDFDataSource_Unassert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aSource;
  nsIRDFResource *aProperty;
  nsIRDFNode *aTarget;
  if (!JS_ConvertArguments(cx, argc, argv, "ooo", &aSource, &aProperty, &aTarget))
    return JS_FALSE;
  nsresult rv = priv->Unassert(aSource, aProperty, aTarget);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
static JSBool
nsIRDFDataSource_HasAssertion(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aSource;
  nsIRDFResource *aProperty;
  nsIRDFNode *aTarget;
  PRBool aTruthValue;
  if (!JS_ConvertArguments(cx, argc, argv, "ooob", &aSource, &aProperty, &aTarget, &aTruthValue))
    return JS_FALSE;
  PRBool retval;
  nsresult rv = priv->HasAssertion(aSource, aProperty, aTarget, aTruthValue, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = BOOLEAN_TO_JSVAL(retval);
  return JS_TRUE;
}

/* void AddObserver (in nsIRDFObserver aObserver); */
static JSBool
nsIRDFDataSource_AddObserver(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFObserver *aObserver;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aObserver))
    return JS_FALSE;
  nsresult rv = priv->AddObserver(aObserver);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* void RemoveObserver (in nsIRDFObserver aObserver); */
static JSBool
nsIRDFDataSource_RemoveObserver(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFObserver *aObserver;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aObserver))
    return JS_FALSE;
  nsresult rv = priv->RemoveObserver(aObserver);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* nsIRDFArcsInCursor ArcLabelsIn (in nsIRDFNode aNode); */
static JSBool
nsIRDFDataSource_ArcLabelsIn(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFNode *aNode;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aNode))
    return JS_FALSE;
  nsIRDFArcsInCursor * retval;
  nsresult rv = priv->ArcLabelsIn(aNode, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFArcsInCursor::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* nsIRDFArcsOutCursor ArcLabelsOut (in nsIRDFResource aSource); */
static JSBool
nsIRDFDataSource_ArcLabelsOut(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aSource;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aSource))
    return JS_FALSE;
  nsIRDFArcsOutCursor * retval;
  nsresult rv = priv->ArcLabelsOut(aSource, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFArcsOutCursor::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* nsIRDFResourceCursor GetAllResources (); */
static JSBool
nsIRDFDataSource_GetAllResources(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  if (!JS_ConvertArguments(cx, argc, argv, ""))
    return JS_FALSE;
  nsIRDFResourceCursor * retval;
  nsresult rv = priv->GetAllResources(&retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFResourceCursor::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* void Flush (); */
static JSBool
nsIRDFDataSource_Flush(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  if (!JS_ConvertArguments(cx, argc, argv, ""))
    return JS_FALSE;
  nsresult rv = priv->Flush();
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
static JSBool
nsIRDFDataSource_GetAllCommands(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aSource;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aSource))
    return JS_FALSE;
  nsIEnumerator * retval;
  nsresult rv = priv->GetAllCommands(aSource, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIEnumerator::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
static JSBool
nsIRDFDataSource_IsCommandEnabled(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsISupportsArray *aSources;
  nsIRDFResource *aCommand;
  nsISupportsArray *aArguments;
  if (!JS_ConvertArguments(cx, argc, argv, "ooo", &aSources, &aCommand, &aArguments))
    return JS_FALSE;
  PRBool retval;
  nsresult rv = priv->IsCommandEnabled(aSources, aCommand, aArguments, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = BOOLEAN_TO_JSVAL(retval);
  return JS_TRUE;
}

/* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
static JSBool
nsIRDFDataSource_DoCommand(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsISupportsArray *aSources;
  nsIRDFResource *aCommand;
  nsISupportsArray *aArguments;
  if (!JS_ConvertArguments(cx, argc, argv, "ooo", &aSources, &aCommand, &aArguments))
    return JS_FALSE;
  nsresult rv = priv->DoCommand(aSources, aCommand, aArguments);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

static JSFunctionSpec nsIRDFDataSource_funcs[] = {
  {"Init", nsIRDFDataSource_Init, 1},
  {"GetSource", nsIRDFDataSource_GetSource, 3},
  {"GetSources", nsIRDFDataSource_GetSources, 3},
  {"GetTarget", nsIRDFDataSource_GetTarget, 3},
  {"GetTargets", nsIRDFDataSource_GetTargets, 3},
  {"Assert", nsIRDFDataSource_Assert, 4},
  {"Unassert", nsIRDFDataSource_Unassert, 3},
  {"HasAssertion", nsIRDFDataSource_HasAssertion, 4},
  {"AddObserver", nsIRDFDataSource_AddObserver, 1},
  {"RemoveObserver", nsIRDFDataSource_RemoveObserver, 1},
  {"ArcLabelsIn", nsIRDFDataSource_ArcLabelsIn, 1},
  {"ArcLabelsOut", nsIRDFDataSource_ArcLabelsOut, 1},
  {"GetAllResources", nsIRDFDataSource_GetAllResources, 0},
  {"Flush", nsIRDFDataSource_Flush, 0},
  {"GetAllCommands", nsIRDFDataSource_GetAllCommands, 1},
  {"IsCommandEnabled", nsIRDFDataSource_IsCommandEnabled, 3},
  {"DoCommand", nsIRDFDataSource_DoCommand, 3},
  {0}
};

static JSPropertySpec nsIRDFDataSource_props[] = {
  {"URI", -1, JSPROP_READONLY},
  {0}
};

static JSBool
nsIRDFDataSource_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (!JSVAL_IS_INT(id))
    return JS_TRUE;
  nsresult result;
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  switch (JSVAL_TO_INT(id)) {
   case -1:
    char * URI;
    result = priv->GetURI(&URI);
    if (NS_FAILED(result))
      goto bad;
    JSString *str = JS_NewStringCopyZ(cx, URI);
    if (!str)
      return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    break;
  }
  return JS_TRUE;
bad:
  JS_ReportError(cx, XXXnsresult2string(result));
  return JS_FALSE;
}

static void
nsIRDFDataSource_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFDataSource *priv = (nsIRDFDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFDataSource_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFDataSource_class = {
  "nsIRDFDataSource",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, nsIRDFDataSource_GetProperty, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFDataSource_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFDataSource::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFDataSource_class, nsIRDFDataSource_ctor, 0,
                                 nsIRDFDataSource_props, nsIRDFDataSource_funcs, 0, 0);
  return proto;
}

JSObject *
nsIRDFDataSource::GetJSObject(JSContext *cx, nsIRDFDataSource *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFDataSource_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

/* void AddDataSource (in nsIRDFDataSource aDataSource); */
static JSBool
nsIRDFCompositeDataSource_AddDataSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFCompositeDataSource *priv = (nsIRDFCompositeDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFDataSource *aDataSource;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aDataSource))
    return JS_FALSE;
  nsresult rv = priv->AddDataSource(aDataSource);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* void RemoveDataSource (in nsIRDFDataSource aDataSource); */
static JSBool
nsIRDFCompositeDataSource_RemoveDataSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFCompositeDataSource *priv = (nsIRDFCompositeDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFDataSource *aDataSource;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aDataSource))
    return JS_FALSE;
  nsresult rv = priv->RemoveDataSource(aDataSource);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

static JSFunctionSpec nsIRDFCompositeDataSource_funcs[] = {
  {"AddDataSource", nsIRDFCompositeDataSource_AddDataSource, 1},
  {"RemoveDataSource", nsIRDFCompositeDataSource_RemoveDataSource, 1},
  {0}
};

static void
nsIRDFCompositeDataSource_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFCompositeDataSource *priv = (nsIRDFCompositeDataSource *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFCompositeDataSource_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFCompositeDataSource_class = {
  "nsIRDFCompositeDataSource",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFCompositeDataSource_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFCompositeDataSource::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *parentProto = nsIRDFDataSource::InitJSClass(cx);
  JSObject *proto = JS_InitClass(cx, globj, parentProto, &nsIRDFCompositeDataSource_class, nsIRDFCompositeDataSource_ctor, 0,
                                 0, nsIRDFCompositeDataSource_funcs, 0, 0);
  return proto;
}

JSObject *
nsIRDFCompositeDataSource::GetJSObject(JSContext *cx, nsIRDFCompositeDataSource *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFCompositeDataSource_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */

/* nsIRDFResource GetResource (in string aURI); */
static JSBool
nsIRDFService_GetResource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  const char *aURI;
  if (!JS_ConvertArguments(cx, argc, argv, "s", &aURI))
    return JS_FALSE;
  nsIRDFResource * retval;
  nsresult rv = priv->GetResource(aURI, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFResource::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* nsIRDFResource GetUnicodeResource (in wstring aURI); */
static JSBool
nsIRDFService_GetUnicodeResource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  const PRUnichar *aURI;
  if (!JS_ConvertArguments(cx, argc, argv, "W", &aURI))
    return JS_FALSE;
  nsIRDFResource * retval;
  nsresult rv = priv->GetUnicodeResource(aURI, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFResource::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* nsIRDFLiteral GetLiteral (in wstring aValue); */
static JSBool
nsIRDFService_GetLiteral(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  const PRUnichar *aValue;
  if (!JS_ConvertArguments(cx, argc, argv, "W", &aValue))
    return JS_FALSE;
  nsIRDFLiteral * retval;
  nsresult rv = priv->GetLiteral(aValue, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFLiteral::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* nsIRDFDate GetDateLiteral (in time aValue); */
static JSBool
nsIRDFService_GetDateLiteral(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  PRTime aValue;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aValue))
    return JS_FALSE;
  nsIRDFDate * retval;
  nsresult rv = priv->GetDateLiteral(aValue, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFDate::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* nsIRDFInt GetIntLiteral (in long aValue); */
static JSBool
nsIRDFService_GetIntLiteral(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  PRInt32 aValue;
  if (!JS_ConvertArguments(cx, argc, argv, "i", &aValue))
    return JS_FALSE;
  nsIRDFInt * retval;
  nsresult rv = priv->GetIntLiteral(aValue, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFInt::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

/* void RegisterResource (in nsIRDFResource aResource, in boolean aReplace); */
static JSBool
nsIRDFService_RegisterResource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aResource;
  PRBool aReplace;
  if (!JS_ConvertArguments(cx, argc, argv, "ob", &aResource, &aReplace))
    return JS_FALSE;
  nsresult rv = priv->RegisterResource(aResource, aReplace);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* void UnregisterResource (in nsIRDFResource aResource); */
static JSBool
nsIRDFService_UnregisterResource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFResource *aResource;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aResource))
    return JS_FALSE;
  nsresult rv = priv->UnregisterResource(aResource);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* void RegisterDataSource (in nsIRDFDataSource aDataSource, in boolean aReplace); */
static JSBool
nsIRDFService_RegisterDataSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFDataSource *aDataSource;
  PRBool aReplace;
  if (!JS_ConvertArguments(cx, argc, argv, "ob", &aDataSource, &aReplace))
    return JS_FALSE;
  nsresult rv = priv->RegisterDataSource(aDataSource, aReplace);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* void UnregisterDataSource (in nsIRDFDataSource aDataSource); */
static JSBool
nsIRDFService_UnregisterDataSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  nsIRDFDataSource *aDataSource;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &aDataSource))
    return JS_FALSE;
  nsresult rv = priv->UnregisterDataSource(aDataSource);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  return JS_TRUE;
}

/* nsIRDFDataSource GetDataSource (in string aURI); */
static JSBool
nsIRDFService_GetDataSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return JS_TRUE;
  const char *aURI;
  if (!JS_ConvertArguments(cx, argc, argv, "s", &aURI))
    return JS_FALSE;
  nsIRDFDataSource * retval;
  nsresult rv = priv->GetDataSource(aURI, &retval);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, XXXnsresult2string(rv));
    return JS_FALSE;
  }
  *rval = OBJECT_TO_JSVAL(nsIRDFDataSource::GetJSObject(cx, retval));
  NS_RELEASE(retval);
  return JS_TRUE;
}

static JSFunctionSpec nsIRDFService_funcs[] = {
  {"GetResource", nsIRDFService_GetResource, 1},
  {"GetUnicodeResource", nsIRDFService_GetUnicodeResource, 1},
  {"GetLiteral", nsIRDFService_GetLiteral, 1},
  {"GetDateLiteral", nsIRDFService_GetDateLiteral, 1},
  {"GetIntLiteral", nsIRDFService_GetIntLiteral, 1},
  {"RegisterResource", nsIRDFService_RegisterResource, 2},
  {"UnregisterResource", nsIRDFService_UnregisterResource, 1},
  {"RegisterDataSource", nsIRDFService_RegisterDataSource, 2},
  {"UnregisterDataSource", nsIRDFService_UnregisterDataSource, 1},
  {"GetDataSource", nsIRDFService_GetDataSource, 1},
  {0}
};

static void
nsIRDFService_Finalize(JSContext *cx, JSObject *obj)
{
  nsIRDFService *priv = (nsIRDFService *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIRDFService_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIRDFService_class = {
  "nsIRDFService",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIRDFService_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIRDFService::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIRDFService_class, nsIRDFService_ctor, 0,
                                 0, nsIRDFService_funcs, 0, 0);
  return proto;
}

JSObject *
nsIRDFService::GetJSObject(JSContext *cx, nsIRDFService *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIRDFService_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */
