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
#include "nsIDOMInstallTriggerGlobal.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIInstallTriggerGlobalIID, NS_IDOMINSTALLTRIGGERGLOBAL_IID);

NS_DEF_PTR(nsIDOMInstallTriggerGlobal);


/***********************************************************************/
//
// InstallTriggerGlobal Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetInstallTriggerGlobalProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMInstallTriggerGlobal *a = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case 0:
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
// InstallTriggerGlobal Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetInstallTriggerGlobalProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMInstallTriggerGlobal *a = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case 0:
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
// InstallTriggerGlobal finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeInstallTriggerGlobal(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// InstallTriggerGlobal enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateInstallTriggerGlobal(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// InstallTriggerGlobal resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveInstallTriggerGlobal(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method UpdateEnabled
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobalUpdateEnabled(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRBool nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->UpdateEnabled(&nativeRet)) {
      return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function UpdateEnabled requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method StartSoftwareUpdate
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobalStartSoftwareUpdate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->StartSoftwareUpdate(b0, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function StartSoftwareUpdate requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ConditionalSoftwareUpdate
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobalConditionalSoftwareUpdate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  PRInt32 b2;
  nsAutoString b3;
  PRInt32 b4;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 5) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    if (!JS_ValueToInt32(cx, argv[2], (int32 *)&b2)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b3, cx, argv[3]);

    if (!JS_ValueToInt32(cx, argv[4], (int32 *)&b4)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ConditionalSoftwareUpdate(b0, b1, b2, b3, b4, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function ConditionalSoftwareUpdate requires 5 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method CompareVersion
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobalCompareVersion(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    if (NS_OK != nativeThis->CompareVersion(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function CompareVersion requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for InstallTriggerGlobal
//
JSClass InstallTriggerGlobalClass = {
  "InstallTriggerGlobal", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetInstallTriggerGlobalProperty,
  SetInstallTriggerGlobalProperty,
  EnumerateInstallTriggerGlobal,
  ResolveInstallTriggerGlobal,
  JS_ConvertStub,
  FinalizeInstallTriggerGlobal
};


//
// InstallTriggerGlobal class properties
//
static JSPropertySpec InstallTriggerGlobalProperties[] =
{
  {0}
};


//
// InstallTriggerGlobal class methods
//
static JSFunctionSpec InstallTriggerGlobalMethods[] = 
{
  {"UpdateEnabled",          InstallTriggerGlobalUpdateEnabled,     0},
  {"StartSoftwareUpdate",          InstallTriggerGlobalStartSoftwareUpdate,     1},
  {"ConditionalSoftwareUpdate",          InstallTriggerGlobalConditionalSoftwareUpdate,     5},
  {"CompareVersion",          InstallTriggerGlobalCompareVersion,     2},
  {0}
};


//
// InstallTriggerGlobal constructor
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobal(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// InstallTriggerGlobal class initialization
//
nsresult NS_InitInstallTriggerGlobalClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "InstallTriggerGlobal", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &InstallTriggerGlobalClass,      // JSClass
                         InstallTriggerGlobal,            // JSNative ctor
                         0,             // ctor args
                         InstallTriggerGlobalProperties,  // proto props
                         InstallTriggerGlobalMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "InstallTriggerGlobal", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMInstallTriggerGlobal::MAJOR_DIFF);
      JS_SetProperty(jscontext, constructor, "MAJOR_DIFF", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallTriggerGlobal::MINOR_DIFF);
      JS_SetProperty(jscontext, constructor, "MINOR_DIFF", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallTriggerGlobal::REL_DIFF);
      JS_SetProperty(jscontext, constructor, "REL_DIFF", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallTriggerGlobal::BLD_DIFF);
      JS_SetProperty(jscontext, constructor, "BLD_DIFF", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallTriggerGlobal::EQUAL);
      JS_SetProperty(jscontext, constructor, "EQUAL", &vp);

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
// Method for creating a new InstallTriggerGlobal JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptInstallTriggerGlobal(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptInstallTriggerGlobal");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMInstallTriggerGlobal *aInstallTriggerGlobal;

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

  if (NS_OK != NS_InitInstallTriggerGlobalClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIInstallTriggerGlobalIID, (void **)&aInstallTriggerGlobal);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &InstallTriggerGlobalClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aInstallTriggerGlobal);
  }
  else {
    NS_RELEASE(aInstallTriggerGlobal);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
