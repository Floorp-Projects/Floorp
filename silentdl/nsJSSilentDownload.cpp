/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMSilentDownload.h"
#include "nsIDOMSilentDownloadTask.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kISilentDownloadIID, NS_IDOMSILENTDOWNLOAD_IID);
static NS_DEFINE_IID(kISilentDownloadTaskIID, NS_IDOMSILENTDOWNLOADTASK_IID);

NS_DEF_PTR(nsIDOMSilentDownload);
NS_DEF_PTR(nsIDOMSilentDownloadTask);

//
// SilentDownload property ids
//
enum SilentDownload_slots {
  SILENTDOWNLOAD_BYTERANGE = -1,
  SILENTDOWNLOAD_INTERVAL = -2
};

/***********************************************************************/
//
// SilentDownload Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetSilentDownloadProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMSilentDownload *a = (nsIDOMSilentDownload*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case SILENTDOWNLOAD_BYTERANGE:
      {
        PRInt32 prop;
        if (NS_OK == a->GetByteRange(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SILENTDOWNLOAD_INTERVAL:
      {
        PRInt32 prop;
        if (NS_OK == a->GetInterval(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// SilentDownload Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetSilentDownloadProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMSilentDownload *a = (nsIDOMSilentDownload*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case SILENTDOWNLOAD_BYTERANGE:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetByteRange(prop);
        
        break;
      }
      case SILENTDOWNLOAD_INTERVAL:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetInterval(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// SilentDownload finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeSilentDownload(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// SilentDownload enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateSilentDownload(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// SilentDownload resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveSilentDownload(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Startup
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadStartup(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownload *nativeThis = (nsIDOMSilentDownload*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Startup()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function Startup requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Shutdown
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadShutdown(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownload *nativeThis = (nsIDOMSilentDownload*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Shutdown()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function Shutdown requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Add
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadAdd(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownload *nativeThis = (nsIDOMSilentDownload*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMSilentDownloadTaskPtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kISilentDownloadTaskIID,
                                           "SilentDownloadTask",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Add(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function Add requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Remove
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadRemove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownload *nativeThis = (nsIDOMSilentDownload*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMSilentDownloadTaskPtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kISilentDownloadTaskIID,
                                           "SilentDownloadTask",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Remove(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function Remove requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Find
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadFind(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownload *nativeThis = (nsIDOMSilentDownload*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMSilentDownloadTask* nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->Find(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function Find requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for SilentDownload
//
JSClass SilentDownloadClass = {
  "SilentDownload", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetSilentDownloadProperty,
  SetSilentDownloadProperty,
  EnumerateSilentDownload,
  ResolveSilentDownload,
  JS_ConvertStub,
  FinalizeSilentDownload
};


//
// SilentDownload class properties
//
static JSPropertySpec SilentDownloadProperties[] =
{
  {"ByteRange",    SILENTDOWNLOAD_BYTERANGE,    JSPROP_ENUMERATE},
  {"Interval",    SILENTDOWNLOAD_INTERVAL,    JSPROP_ENUMERATE},
  {0}
};


//
// SilentDownload class methods
//
static JSFunctionSpec SilentDownloadMethods[] = 
{
  {"Startup",          SilentDownloadStartup,     0},
  {"Shutdown",          SilentDownloadShutdown,     0},
  {"Add",          SilentDownloadAdd,     1},
  {"Remove",          SilentDownloadRemove,     1},
  {"Find",          SilentDownloadFind,     1},
  {0}
};


//
// SilentDownload constructor
//
PR_STATIC_CALLBACK(JSBool)
SilentDownload(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// SilentDownload class initialization
//
nsresult NS_InitSilentDownloadClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "SilentDownload", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &SilentDownloadClass,      // JSClass
                         SilentDownload,            // JSNative ctor
                         0,             // ctor args
                         SilentDownloadProperties,  // proto props
                         SilentDownloadMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

  }
  else if ((nsnull != constructor) && JSVAL_IS_OBJECT(vp)) {
    proto = JSVAL_TO_OBJECT(vp);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (aPrototype) {
    *aPrototype = proto;
  }
  return NS_OK;
}


//
// Method for creating a new SilentDownload JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptSilentDownload(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptSilentDownload");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMSilentDownload *aSilentDownload;

  if (nsnull == aParent) {
    parent = nsnull;
  }
  else if (NS_OK == aParent->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
    if (NS_OK != owner->GetScriptObject(aContext, (void **)&parent)) {
      NS_RELEASE(owner);
      return NS_ERROR_FAILURE;
    }
    NS_RELEASE(owner);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (NS_OK != NS_InitSilentDownloadClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kISilentDownloadIID, (void **)&aSilentDownload);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &SilentDownloadClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aSilentDownload);
  }
  else {
    NS_RELEASE(aSilentDownload);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
