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
#include "nsIDOMToolbarCore.h"
#include "nsIDOMWindow.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsRepository.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIToolbarCoreIID, NS_IDOMTOOLBARCORE_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

NS_DEF_PTR(nsIDOMToolbarCore);
NS_DEF_PTR(nsIDOMWindow);


/***********************************************************************/
//
// ToolbarCore Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetToolbarCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMToolbarCore *a = (nsIDOMToolbarCore*)JS_GetPrivate(cx, obj);

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
// ToolbarCore Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetToolbarCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMToolbarCore *a = (nsIDOMToolbarCore*)JS_GetPrivate(cx, obj);

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
// ToolbarCore finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeToolbarCore(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// ToolbarCore enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateToolbarCore(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// ToolbarCore resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveToolbarCore(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method SetWindow
//
PR_STATIC_CALLBACK(JSBool)
ToolbarCoreSetWindow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMToolbarCore *nativeThis = (nsIDOMToolbarCore*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->SetWindow(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function SetWindow requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetWebShellWindow
//
PR_STATIC_CALLBACK(JSBool)
ToolbarCoreSetWebShellWindow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMToolbarCore *nativeThis = (nsIDOMToolbarCore*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->SetWebShellWindow(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function SetWebShellWindow requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetStatus
//
PR_STATIC_CALLBACK(JSBool)
ToolbarCoreSetStatus(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMToolbarCore *nativeThis = (nsIDOMToolbarCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->SetStatus(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function SetStatus requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for ToolbarCore
//
JSClass ToolbarCoreClass = {
  "ToolbarCore", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetToolbarCoreProperty,
  SetToolbarCoreProperty,
  EnumerateToolbarCore,
  ResolveToolbarCore,
  JS_ConvertStub,
  FinalizeToolbarCore
};


//
// ToolbarCore class properties
//
static JSPropertySpec ToolbarCoreProperties[] =
{
  {0}
};


//
// ToolbarCore class methods
//
static JSFunctionSpec ToolbarCoreMethods[] = 
{
  {"SetWindow",          ToolbarCoreSetWindow,     1},
  {"SetWebShellWindow",          ToolbarCoreSetWebShellWindow,     1},
  {"SetStatus",          ToolbarCoreSetStatus,     1},
  {0}
};


//
// ToolbarCore constructor
//
PR_STATIC_CALLBACK(JSBool)
ToolbarCore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMToolbarCore *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;

  static NS_DEFINE_IID(kIDOMToolbarCoreIID, NS_IDOMTOOLBARCORE_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("ToolbarCore", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsRepository::CreateInstance(classID,
                                        nsnull,
                                        kIDOMToolbarCoreIID,
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
// ToolbarCore class initialization
//
nsresult NS_InitToolbarCoreClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "ToolbarCore", &vp)) ||
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
                         &ToolbarCoreClass,      // JSClass
                         ToolbarCore,            // JSNative ctor
                         0,             // ctor args
                         ToolbarCoreProperties,  // proto props
                         ToolbarCoreMethods,     // proto funcs
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
// Method for creating a new ToolbarCore JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptToolbarCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptToolbarCore");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMToolbarCore *aToolbarCore;

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

  if (NS_OK != NS_InitToolbarCoreClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIToolbarCoreIID, (void **)&aToolbarCore);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &ToolbarCoreClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aToolbarCore);
  }
  else {
    NS_RELEASE(aToolbarCore);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
