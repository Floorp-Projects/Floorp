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
#include "nsIDOMProfileCore.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsIJSNativeInitializer.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIProfileCoreIID, NS_IDOMPROFILECORE_IID);

NS_DEF_PTR(nsIDOMProfileCore);


/***********************************************************************/
//
// ProfileCore Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetProfileCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMProfileCore *a = (nsIDOMProfileCore*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case 0:
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
// ProfileCore Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetProfileCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMProfileCore *a = (nsIDOMProfileCore*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case 0:
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
// ProfileCore finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeProfileCore(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// ProfileCore enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateProfileCore(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// ProfileCore resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveProfileCore(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method CreateNewProfile
//
PR_STATIC_CALLBACK(JSBool)
ProfileCoreCreateNewProfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMProfileCore *nativeThis = (nsIDOMProfileCore*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "profilecore.createnewprofile", &ok);
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

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->CreateNewProfile(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function CreateNewProfile requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method RenameProfile
//
PR_STATIC_CALLBACK(JSBool)
ProfileCoreRenameProfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMProfileCore *nativeThis = (nsIDOMProfileCore*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "profilecore.renameprofile", &ok);
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

  if (argc >= 2) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    if (NS_OK != nativeThis->RenameProfile(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function RenameProfile requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DeleteProfile
//
PR_STATIC_CALLBACK(JSBool)
ProfileCoreDeleteProfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMProfileCore *nativeThis = (nsIDOMProfileCore*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "profilecore.deleteprofile", &ok);
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

  if (argc >= 2) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    if (NS_OK != nativeThis->DeleteProfile(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function DeleteProfile requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetProfileList
//
PR_STATIC_CALLBACK(JSBool)
ProfileCoreGetProfileList(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMProfileCore *nativeThis = (nsIDOMProfileCore*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "profilecore.getprofilelist", &ok);
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

    if (NS_OK != nativeThis->GetProfileList(nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function GetProfileList requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method StartCommunicator
//
PR_STATIC_CALLBACK(JSBool)
ProfileCoreStartCommunicator(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMProfileCore *nativeThis = (nsIDOMProfileCore*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "profilecore.startcommunicator", &ok);
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

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->StartCommunicator(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function StartCommunicator requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetCurrentProfile
//
PR_STATIC_CALLBACK(JSBool)
ProfileCoreGetCurrentProfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMProfileCore *nativeThis = (nsIDOMProfileCore*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "profilecore.getcurrentprofile", &ok);
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

    if (NS_OK != nativeThis->GetCurrentProfile(nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function GetCurrentProfile requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method MigrateProfile
//
PR_STATIC_CALLBACK(JSBool)
ProfileCoreMigrateProfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMProfileCore *nativeThis = (nsIDOMProfileCore*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "profilecore.migrateprofile", &ok);
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

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->MigrateProfile(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function MigrateProfile requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for ProfileCore
//
JSClass ProfileCoreClass = {
  "ProfileCore", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetProfileCoreProperty,
  SetProfileCoreProperty,
  EnumerateProfileCore,
  ResolveProfileCore,
  JS_ConvertStub,
  FinalizeProfileCore
};


//
// ProfileCore class properties
//
static JSPropertySpec ProfileCoreProperties[] =
{
  {0}
};


//
// ProfileCore class methods
//
static JSFunctionSpec ProfileCoreMethods[] = 
{
  {"CreateNewProfile",          ProfileCoreCreateNewProfile,     1},
  {"RenameProfile",          ProfileCoreRenameProfile,     2},
  {"DeleteProfile",          ProfileCoreDeleteProfile,     2},
  {"GetProfileList",          ProfileCoreGetProfileList,     0},
  {"StartCommunicator",          ProfileCoreStartCommunicator,     1},
  {"GetCurrentProfile",          ProfileCoreGetCurrentProfile,     0},
  {"MigrateProfile",          ProfileCoreMigrateProfile,     1},
  {0}
};


//
// ProfileCore constructor
//
PR_STATIC_CALLBACK(JSBool)
ProfileCore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMProfileCore *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;
  nsIJSNativeInitializer* initializer = nsnull;

  static NS_DEFINE_IID(kIDOMProfileCoreIID, NS_IDOMPROFILECORE_IID);
  static NS_DEFINE_IID(kIJSNativeInitializerIID, NS_IJSNATIVEINITIALIZER_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("ProfileCore", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsComponentManager::CreateInstance(classID,
                                        nsnull,
                                        kIDOMProfileCoreIID,
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
// ProfileCore class initialization
//
extern "C" NS_DOM nsresult NS_InitProfileCoreClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "ProfileCore", &vp)) ||
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
                         &ProfileCoreClass,      // JSClass
                         ProfileCore,            // JSNative ctor
                         0,             // ctor args
                         ProfileCoreProperties,  // proto props
                         ProfileCoreMethods,     // proto funcs
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
// Method for creating a new ProfileCore JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptProfileCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptProfileCore");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMProfileCore *aProfileCore;

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

  if (NS_OK != NS_InitProfileCoreClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIProfileCoreIID, (void **)&aProfileCore);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &ProfileCoreClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aProfileCore);
  }
  else {
    NS_RELEASE(aProfileCore);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
