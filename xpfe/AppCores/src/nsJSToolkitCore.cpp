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
#include "nsIDOMToolkitCore.h"
#include "nsIDOMWindow.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIToolkitCoreIID, NS_IDOMTOOLKITCORE_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

NS_DEF_PTR(nsIDOMToolkitCore);
NS_DEF_PTR(nsIDOMWindow);


/***********************************************************************/
//
// ToolkitCore Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetToolkitCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMToolkitCore *a = (nsIDOMToolkitCore*)JS_GetPrivate(cx, obj);

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
// ToolkitCore Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetToolkitCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMToolkitCore *a = (nsIDOMToolkitCore*)JS_GetPrivate(cx, obj);

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
// ToolkitCore finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeToolkitCore(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// ToolkitCore enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateToolkitCore(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// ToolkitCore resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveToolkitCore(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method ShowDialog
//
PR_STATIC_CALLBACK(JSBool)
ToolkitCoreShowDialog(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMToolkitCore *nativeThis = (nsIDOMToolkitCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsIDOMWindowPtr b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kIWindowIID,
                                           "Window",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ShowDialog(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function ShowDialog requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ShowWindow
//
PR_STATIC_CALLBACK(JSBool)
ToolkitCoreShowWindow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMToolkitCore *nativeThis = (nsIDOMToolkitCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsIDOMWindowPtr b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kIWindowIID,
                                           "Window",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ShowWindow(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function ShowWindow requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ShowWindowWithArgs
//
PR_STATIC_CALLBACK(JSBool)
ToolkitCoreShowWindowWithArgs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMToolkitCore *nativeThis = (nsIDOMToolkitCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsIDOMWindowPtr b1;
  nsAutoString b2;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 3) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kIWindowIID,
                                           "Window",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);

    if (NS_OK != nativeThis->ShowWindowWithArgs(b0, b1, b2)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function ShowWindowWithArgs requires 3 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ShowModalDialog
//
PR_STATIC_CALLBACK(JSBool)
ToolkitCoreShowModalDialog(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMToolkitCore *nativeThis = (nsIDOMToolkitCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsIDOMWindowPtr b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kIWindowIID,
                                           "Window",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ShowModalDialog(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function ShowModalDialog requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method CloseWindow
//
PR_STATIC_CALLBACK(JSBool)
ToolkitCoreCloseWindow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMToolkitCore *nativeThis = (nsIDOMToolkitCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMWindowPtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIWindowIID,
                                           "Window",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->CloseWindow(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function CloseWindow requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for ToolkitCore
//
JSClass ToolkitCoreClass = {
  "ToolkitCore", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetToolkitCoreProperty,
  SetToolkitCoreProperty,
  EnumerateToolkitCore,
  ResolveToolkitCore,
  JS_ConvertStub,
  FinalizeToolkitCore
};


//
// ToolkitCore class properties
//
static JSPropertySpec ToolkitCoreProperties[] =
{
  {0}
};


//
// ToolkitCore class methods
//
static JSFunctionSpec ToolkitCoreMethods[] = 
{
  {"ShowDialog",          ToolkitCoreShowDialog,     2},
  {"ShowWindow",          ToolkitCoreShowWindow,     2},
  {"ShowWindowWithArgs",          ToolkitCoreShowWindowWithArgs,     3},
  {"ShowModalDialog",          ToolkitCoreShowModalDialog,     2},
  {"CloseWindow",          ToolkitCoreCloseWindow,     1},
  {0}
};


//
// ToolkitCore constructor
//
PR_STATIC_CALLBACK(JSBool)
ToolkitCore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMToolkitCore *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;

  static NS_DEFINE_IID(kIDOMToolkitCoreIID, NS_IDOMTOOLKITCORE_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("ToolkitCore", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsComponentManager::CreateInstance(classID,
                                        nsnull,
                                        kIDOMToolkitCoreIID,
                                        (void **)&nativeThis);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  // XXX We should be calling Init() on the instance

  result = nativeThis->QueryInterface(kIScriptObjectOwnerIID, (void **)&owner);
  if (NS_OK != result) {
    NS_RELEASE(nativeThis);
    return JS_FALSE;
  }

  owner->SetScriptObject((void *)obj);
  JS_SetPrivate(cx, obj, nativeThis);

  NS_RELEASE(owner);
  return JS_TRUE;
}

//
// ToolkitCore class initialization
//
extern "C" NS_DOM nsresult NS_InitToolkitCoreClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "ToolkitCore", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitBaseAppCoreClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &ToolkitCoreClass,      // JSClass
                         ToolkitCore,            // JSNative ctor
                         0,             // ctor args
                         ToolkitCoreProperties,  // proto props
                         ToolkitCoreMethods,     // proto funcs
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
// Method for creating a new ToolkitCore JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptToolkitCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptToolkitCore");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMToolkitCore *aToolkitCore;

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

  if (NS_OK != NS_InitToolkitCoreClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIToolkitCoreIID, (void **)&aToolkitCore);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &ToolkitCoreClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aToolkitCore);
  }
  else {
    NS_RELEASE(aToolkitCore);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
