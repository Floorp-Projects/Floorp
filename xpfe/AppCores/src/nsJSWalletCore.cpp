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
#include "nsIDOMWalletCore.h"
#include "nsIDOMWindow.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsIJSNativeInitializer.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIWalletCoreIID, NS_IDOMWALLETCORE_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

NS_DEF_PTR(nsIDOMWalletCore);
NS_DEF_PTR(nsIDOMWindow);


/***********************************************************************/
//
// WalletCore Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetWalletCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWalletCore *a = (nsIDOMWalletCore*)JS_GetPrivate(cx, obj);

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
// WalletCore Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetWalletCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWalletCore *a = (nsIDOMWalletCore*)JS_GetPrivate(cx, obj);

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
// WalletCore finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeWalletCore(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// WalletCore enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateWalletCore(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// WalletCore resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveWalletCore(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method ShowWindow
//
PR_STATIC_CALLBACK(JSBool)
WalletCoreShowWindow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWalletCore *nativeThis = (nsIDOMWalletCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMWindowPtr b0;
  nsIDOMWindowPtr b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIWindowIID,
                                           "Window",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

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
// Native method ChangePanel
//
PR_STATIC_CALLBACK(JSBool)
WalletCoreChangePanel(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWalletCore *nativeThis = (nsIDOMWalletCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->ChangePanel(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function ChangePanel requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method PanelLoaded
//
PR_STATIC_CALLBACK(JSBool)
WalletCorePanelLoaded(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWalletCore *nativeThis = (nsIDOMWalletCore*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->PanelLoaded(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function PanelLoaded requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SaveWallet
//
PR_STATIC_CALLBACK(JSBool)
WalletCoreSaveWallet(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWalletCore *nativeThis = (nsIDOMWalletCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->SaveWallet(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function SaveWallet requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method CancelWallet
//
PR_STATIC_CALLBACK(JSBool)
WalletCoreCancelWallet(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWalletCore *nativeThis = (nsIDOMWalletCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->CancelWallet()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function CancelWallet requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetPrefillList
//
PR_STATIC_CALLBACK(JSBool)
WalletCoreGetPrefillList(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWalletCore *nativeThis = (nsIDOMWalletCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->GetPrefillList(nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function GetPrefillList requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for WalletCore
//
JSClass WalletCoreClass = {
  "WalletCore", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetWalletCoreProperty,
  SetWalletCoreProperty,
  EnumerateWalletCore,
  ResolveWalletCore,
  JS_ConvertStub,
  FinalizeWalletCore
};


//
// WalletCore class properties
//
static JSPropertySpec WalletCoreProperties[] =
{
  {0}
};


//
// WalletCore class methods
//
static JSFunctionSpec WalletCoreMethods[] = 
{
  {"ShowWindow",          WalletCoreShowWindow,     2},
  {"ChangePanel",          WalletCoreChangePanel,     1},
  {"PanelLoaded",          WalletCorePanelLoaded,     1},
  {"SaveWallet",          WalletCoreSaveWallet,     1},
  {"CancelWallet",          WalletCoreCancelWallet,     0},
  {"GetPrefillList",          WalletCoreGetPrefillList,     0},
  {0}
};


//
// WalletCore constructor
//
PR_STATIC_CALLBACK(JSBool)
WalletCore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMWalletCore *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;
  nsIJSNativeInitializer* initializer = nsnull;

  static NS_DEFINE_IID(kIDOMWalletCoreIID, NS_IDOMWALLETCORE_IID);
  static NS_DEFINE_IID(kIJSNativeInitializerIID, NS_IJSNATIVEINITIALIZER_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("WalletCore", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsComponentManager::CreateInstance(classID,
                                        nsnull,
                                        kIDOMWalletCoreIID,
                                        (void **)&nativeThis);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nativeThis->QueryInterface(kIJSNativeInitializerIID, (void **)&initializer);
  if (NS_OK == result) {
    result = initializer->Initialize(cx, argc, argv);
    NS_RELEASE(initializer);

    if (NS_OK != result) {
      NS_RELEASE(nativeThis);
      return JS_FALSE;
    }
  }

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
// WalletCore class initialization
//
extern "C" NS_DOM nsresult NS_InitWalletCoreClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "WalletCore", &vp)) ||
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
                         &WalletCoreClass,      // JSClass
                         WalletCore,            // JSNative ctor
                         0,             // ctor args
                         WalletCoreProperties,  // proto props
                         WalletCoreMethods,     // proto funcs
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
// Method for creating a new WalletCore JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptWalletCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptWalletCore");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMWalletCore *aWalletCore;

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

  if (NS_OK != NS_InitWalletCoreClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIWalletCoreIID, (void **)&aWalletCore);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &WalletCoreClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aWalletCore);
  }
  else {
    NS_RELEASE(aWalletCore);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
