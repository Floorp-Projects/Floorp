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
#include "nsDOMError.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIController.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMWindow.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIControllerIID, NS_ICONTROLLER_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIXULCommandDispatcherIID, NS_IDOMXULCOMMANDDISPATCHER_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

NS_DEF_PTR(nsIController);
NS_DEF_PTR(nsIDOMElement);
NS_DEF_PTR(nsIDOMXULCommandDispatcher);
NS_DEF_PTR(nsIDOMWindow);

//
// XULCommandDispatcher property ids
//
enum XULCommandDispatcher_slots {
  XULCOMMANDDISPATCHER_FOCUSEDELEMENT = -1,
  XULCOMMANDDISPATCHER_FOCUSEDWINDOW = -2
};

/***********************************************************************/
//
// XULCommandDispatcher Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetXULCommandDispatcherProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULCommandDispatcher *a = (nsIDOMXULCommandDispatcher*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
    }
    switch(JSVAL_TO_INT(id)) {
      case XULCOMMANDDISPATCHER_FOCUSEDELEMENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "xulcommanddispatcher.focusedelement", PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsIDOMElement* prop;
        nsresult result = NS_OK;
        result = a->GetFocusedElement(&prop);
        if (NS_SUCCEEDED(result)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, result);
        }
        break;
      }
      case XULCOMMANDDISPATCHER_FOCUSEDWINDOW:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "xulcommanddispatcher.focusedwindow", PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsIDOMWindow* prop;
        nsresult result = NS_OK;
        result = a->GetFocusedWindow(&prop);
        if (NS_SUCCEEDED(result)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, result);
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
// XULCommandDispatcher Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetXULCommandDispatcherProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULCommandDispatcher *a = (nsIDOMXULCommandDispatcher*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
    }
    switch(JSVAL_TO_INT(id)) {
      case XULCOMMANDDISPATCHER_FOCUSEDELEMENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "xulcommanddispatcher.focusedelement", PR_TRUE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsIDOMElement* prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                kIElementIID, "Element",
                                                cx, *vp)) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_NOT_OBJECT_ERR);
        }
      
        a->SetFocusedElement(prop);
        NS_IF_RELEASE(prop);
        break;
      }
      case XULCOMMANDDISPATCHER_FOCUSEDWINDOW:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "xulcommanddispatcher.focusedwindow", PR_TRUE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsIDOMWindow* prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                kIWindowIID, "Window",
                                                cx, *vp)) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_NOT_OBJECT_ERR);
        }
      
        a->SetFocusedWindow(prop);
        NS_IF_RELEASE(prop);
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
// XULCommandDispatcher finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeXULCommandDispatcher(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// XULCommandDispatcher enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateXULCommandDispatcher(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// XULCommandDispatcher resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULCommandDispatcher(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method AddCommandUpdater
//
PR_STATIC_CALLBACK(JSBool)
XULCommandDispatcherAddCommandUpdater(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULCommandDispatcher *nativeThis = (nsIDOMXULCommandDispatcher*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMElementPtr b0;
  nsAutoString b1;
  nsAutoString b2;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulcommanddispatcher.addcommandupdater",PR_FALSE , &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 3) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIElementIID,
                                           "Element",
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);
    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);

    result = nativeThis->AddCommandUpdater(b0, b1, b2);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method RemoveCommandUpdater
//
PR_STATIC_CALLBACK(JSBool)
XULCommandDispatcherRemoveCommandUpdater(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULCommandDispatcher *nativeThis = (nsIDOMXULCommandDispatcher*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMElementPtr b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulcommanddispatcher.removecommandupdater",PR_FALSE , &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIElementIID,
                                           "Element",
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->RemoveCommandUpdater(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method UpdateCommands
//
PR_STATIC_CALLBACK(JSBool)
XULCommandDispatcherUpdateCommands(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULCommandDispatcher *nativeThis = (nsIDOMXULCommandDispatcher*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulcommanddispatcher.updatecommands",PR_FALSE , &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->UpdateCommands(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetController
//
PR_STATIC_CALLBACK(JSBool)
XULCommandDispatcherGetController(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULCommandDispatcher *nativeThis = (nsIDOMXULCommandDispatcher*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIController* nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulcommanddispatcher.getcontroller",PR_FALSE , &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    result = nativeThis->GetController(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, result);
    }

    // n.b., this will release nativeRet
    nsJSUtils::nsConvertXPCObjectToJSVal(nativeRet, nsIController::GetIID(), cx, rval);
  }

  return JS_TRUE;
}


//
// Native method SetController
//
PR_STATIC_CALLBACK(JSBool)
XULCommandDispatcherSetController(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULCommandDispatcher *nativeThis = (nsIDOMXULCommandDispatcher*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIControllerPtr b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulcommanddispatcher.setcontroller",PR_FALSE , &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToXPCObject((nsISupports**) &b0,
                                           kIControllerIID, cx, argv[0])) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_NOT_XPC_OBJECT_ERR);
    }

    result = nativeThis->SetController(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for XULCommandDispatcher
//
JSClass XULCommandDispatcherClass = {
  "XULCommandDispatcher", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetXULCommandDispatcherProperty,
  SetXULCommandDispatcherProperty,
  EnumerateXULCommandDispatcher,
  ResolveXULCommandDispatcher,
  JS_ConvertStub,
  FinalizeXULCommandDispatcher
};


//
// XULCommandDispatcher class properties
//
static JSPropertySpec XULCommandDispatcherProperties[] =
{
  {"focusedElement",    XULCOMMANDDISPATCHER_FOCUSEDELEMENT,    JSPROP_ENUMERATE},
  {"focusedWindow",    XULCOMMANDDISPATCHER_FOCUSEDWINDOW,    JSPROP_ENUMERATE},
  {0}
};


//
// XULCommandDispatcher class methods
//
static JSFunctionSpec XULCommandDispatcherMethods[] = 
{
  {"addCommandUpdater",          XULCommandDispatcherAddCommandUpdater,     3},
  {"removeCommandUpdater",          XULCommandDispatcherRemoveCommandUpdater,     1},
  {"updateCommands",          XULCommandDispatcherUpdateCommands,     1},
  {"getController",          XULCommandDispatcherGetController,     0},
  {"setController",          XULCommandDispatcherSetController,     1},
  {0}
};


//
// XULCommandDispatcher constructor
//
PR_STATIC_CALLBACK(JSBool)
XULCommandDispatcher(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// XULCommandDispatcher class initialization
//
extern "C" NS_DOM nsresult NS_InitXULCommandDispatcherClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XULCommandDispatcher", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &XULCommandDispatcherClass,      // JSClass
                         XULCommandDispatcher,            // JSNative ctor
                         0,             // ctor args
                         XULCommandDispatcherProperties,  // proto props
                         XULCommandDispatcherMethods,     // proto funcs
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
// Method for creating a new XULCommandDispatcher JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptXULCommandDispatcher(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptXULCommandDispatcher");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMXULCommandDispatcher *aXULCommandDispatcher;

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

  if (NS_OK != NS_InitXULCommandDispatcherClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIXULCommandDispatcherIID, (void **)&aXULCommandDispatcher);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &XULCommandDispatcherClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aXULCommandDispatcher);
  }
  else {
    NS_RELEASE(aXULCommandDispatcher);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
