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
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIController.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULFocusTracker.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIControllerIID, NS_ICONTROLLER_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIXULFocusTrackerIID, NS_IDOMXULFOCUSTRACKER_IID);

NS_DEF_PTR(nsIController);
NS_DEF_PTR(nsIDOMElement);
NS_DEF_PTR(nsIDOMXULFocusTracker);

//
// XULFocusTracker property ids
//
enum XULFocusTracker_slots {
  XULFOCUSTRACKER_CURRENT = -1
};

/***********************************************************************/
//
// XULFocusTracker Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetXULFocusTrackerProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULFocusTracker *a = (nsIDOMXULFocusTracker*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsIScriptSecurityManager *secMan;
    PRBool ok = PR_FALSE;
    if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case XULFOCUSTRACKER_CURRENT:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulfocustracker.current", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMElement* prop;
        if (NS_OK == a->GetCurrent(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
    NS_RELEASE(secMan);
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// XULFocusTracker Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetXULFocusTrackerProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULFocusTracker *a = (nsIDOMXULFocusTracker*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsIScriptSecurityManager *secMan;
    PRBool ok = PR_FALSE;
    if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case XULFOCUSTRACKER_CURRENT:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulfocustracker.current", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMElement* prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                kIElementIID, "Element",
                                                cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetCurrent(prop);
        NS_IF_RELEASE(prop);
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
    NS_RELEASE(secMan);
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// XULFocusTracker finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeXULFocusTracker(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// XULFocusTracker enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateXULFocusTracker(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// XULFocusTracker resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULFocusTracker(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method AddFocusListener
//
PR_STATIC_CALLBACK(JSBool)
XULFocusTrackerAddFocusListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULFocusTracker *nativeThis = (nsIDOMXULFocusTracker*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMElementPtr b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulfocustracker.addfocuslistener", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }
  else {
    return JS_FALSE;
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIElementIID,
                                           "Element",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->AddFocusListener(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function addFocusListener requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method RemoveFocusListener
//
PR_STATIC_CALLBACK(JSBool)
XULFocusTrackerRemoveFocusListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULFocusTracker *nativeThis = (nsIDOMXULFocusTracker*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMElementPtr b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulfocustracker.removefocuslistener", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }
  else {
    return JS_FALSE;
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIElementIID,
                                           "Element",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->RemoveFocusListener(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function removeFocusListener requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method FocusChanged
//
PR_STATIC_CALLBACK(JSBool)
XULFocusTrackerFocusChanged(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULFocusTracker *nativeThis = (nsIDOMXULFocusTracker*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulfocustracker.focuschanged", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }
  else {
    return JS_FALSE;
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->FocusChanged()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function focusChanged requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetController
//
PR_STATIC_CALLBACK(JSBool)
XULFocusTrackerGetController(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULFocusTracker *nativeThis = (nsIDOMXULFocusTracker*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIController* nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulfocustracker.getcontroller", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }
  else {
    return JS_FALSE;
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->GetController(&nativeRet)) {
      return JS_FALSE;
    }

    // n.b., this will release nativeRet
    nsJSUtils::nsConvertXPCObjectToJSVal(nativeRet, nsIController::GetIID(), cx, rval);
  }
  else {
    JS_ReportError(cx, "Function getController requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetController
//
PR_STATIC_CALLBACK(JSBool)
XULFocusTrackerSetController(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULFocusTracker *nativeThis = (nsIDOMXULFocusTracker*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIControllerPtr b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulfocustracker.setcontroller", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }
  else {
    return JS_FALSE;
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToXPCObject((nsISupports**) &b0,
                                           kIControllerIID, cx, argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SetController(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setController requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for XULFocusTracker
//
JSClass XULFocusTrackerClass = {
  "XULFocusTracker", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetXULFocusTrackerProperty,
  SetXULFocusTrackerProperty,
  EnumerateXULFocusTracker,
  ResolveXULFocusTracker,
  JS_ConvertStub,
  FinalizeXULFocusTracker
};


//
// XULFocusTracker class properties
//
static JSPropertySpec XULFocusTrackerProperties[] =
{
  {"current",    XULFOCUSTRACKER_CURRENT,    JSPROP_ENUMERATE},
  {0}
};


//
// XULFocusTracker class methods
//
static JSFunctionSpec XULFocusTrackerMethods[] = 
{
  {"addFocusListener",          XULFocusTrackerAddFocusListener,     1},
  {"removeFocusListener",          XULFocusTrackerRemoveFocusListener,     1},
  {"focusChanged",          XULFocusTrackerFocusChanged,     0},
  {"getController",          XULFocusTrackerGetController,     0},
  {"setController",          XULFocusTrackerSetController,     1},
  {0}
};


//
// XULFocusTracker constructor
//
PR_STATIC_CALLBACK(JSBool)
XULFocusTracker(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// XULFocusTracker class initialization
//
extern "C" NS_DOM nsresult NS_InitXULFocusTrackerClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XULFocusTracker", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &XULFocusTrackerClass,      // JSClass
                         XULFocusTracker,            // JSNative ctor
                         0,             // ctor args
                         XULFocusTrackerProperties,  // proto props
                         XULFocusTrackerMethods,     // proto funcs
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
// Method for creating a new XULFocusTracker JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptXULFocusTracker(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptXULFocusTracker");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMXULFocusTracker *aXULFocusTracker;

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

  if (NS_OK != NS_InitXULFocusTrackerClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIXULFocusTrackerIID, (void **)&aXULFocusTracker);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &XULFocusTrackerClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aXULFocusTracker);
  }
  else {
    NS_RELEASE(aXULFocusTracker);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
