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
#include "nsIDOMInstallFolder.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsRepository.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIInstallFolderIID, NS_IDOMINSTALLFOLDER_IID);

NS_DEF_PTR(nsIDOMInstallFolder);


/***********************************************************************/
//
// InstallFolder Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetInstallFolderProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMInstallFolder *a = (nsIDOMInstallFolder*)JS_GetPrivate(cx, obj);

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
// InstallFolder Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetInstallFolderProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMInstallFolder *a = (nsIDOMInstallFolder*)JS_GetPrivate(cx, obj);

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
// InstallFolder finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeInstallFolder(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// InstallFolder enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateInstallFolder(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// InstallFolder resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveInstallFolder(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Init
//
PR_STATIC_CALLBACK(JSBool)
InstallFolderInit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallFolder *nativeThis = (nsIDOMInstallFolder*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 3) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);

    if (NS_OK != nativeThis->Init(b0, b1, b2)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function init requires 3 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetDirectoryPath
//
PR_STATIC_CALLBACK(JSBool)
InstallFolderGetDirectoryPath(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallFolder *nativeThis = (nsIDOMInstallFolder*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->GetDirectoryPath(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function GetDirectoryPath requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method MakeFullPath
//
PR_STATIC_CALLBACK(JSBool)
InstallFolderMakeFullPath(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallFolder *nativeThis = (nsIDOMInstallFolder*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
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

    if (NS_OK != nativeThis->MakeFullPath(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function MakeFullPath requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method IsJavaCapable
//
PR_STATIC_CALLBACK(JSBool)
InstallFolderIsJavaCapable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallFolder *nativeThis = (nsIDOMInstallFolder*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRBool nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->IsJavaCapable(&nativeRet)) {
      return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function IsJavaCapable requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ToString
//
PR_STATIC_CALLBACK(JSBool)
InstallFolderToString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallFolder *nativeThis = (nsIDOMInstallFolder*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->ToString(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function ToString requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for InstallFolder
//
JSClass InstallFolderClass = {
  "InstallFolder", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetInstallFolderProperty,
  SetInstallFolderProperty,
  EnumerateInstallFolder,
  ResolveInstallFolder,
  JS_ConvertStub,
  FinalizeInstallFolder
};


//
// InstallFolder class properties
//
static JSPropertySpec InstallFolderProperties[] =
{
  {0}
};


//
// InstallFolder class methods
//
static JSFunctionSpec InstallFolderMethods[] = 
{
  {"init",          InstallFolderInit,     3},
  {"GetDirectoryPath",          InstallFolderGetDirectoryPath,     1},
  {"MakeFullPath",          InstallFolderMakeFullPath,     2},
  {"IsJavaCapable",          InstallFolderIsJavaCapable,     0},
  {"ToString",          InstallFolderToString,     1},
  {0}
};


//
// InstallFolder constructor
//
PR_STATIC_CALLBACK(JSBool)
InstallFolder(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMInstallFolder *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;

  static NS_DEFINE_IID(kIDOMInstallFolderIID, NS_IDOMINSTALLFOLDER_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("InstallFolder", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsRepository::CreateInstance(classID,
                                        nsnull,
                                        kIDOMInstallFolderIID,
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
// InstallFolder class initialization
//
nsresult NS_InitInstallFolderClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "InstallFolder", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &InstallFolderClass,      // JSClass
                         InstallFolder,            // JSNative ctor
                         0,             // ctor args
                         InstallFolderProperties,  // proto props
                         InstallFolderMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "InstallFolder", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMInstallFolder::BadFolder);
      JS_SetProperty(jscontext, constructor, "BadFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::PluginFolder);
      JS_SetProperty(jscontext, constructor, "PluginFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::ProgramFolder);
      JS_SetProperty(jscontext, constructor, "ProgramFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::PackageFolder);
      JS_SetProperty(jscontext, constructor, "PackageFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::TemporaryFolder);
      JS_SetProperty(jscontext, constructor, "TemporaryFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::CommunicatorFolder);
      JS_SetProperty(jscontext, constructor, "CommunicatorFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::InstalledFolder);
      JS_SetProperty(jscontext, constructor, "InstalledFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::CurrentUserFolder);
      JS_SetProperty(jscontext, constructor, "CurrentUserFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::NetHelpFolder);
      JS_SetProperty(jscontext, constructor, "NetHelpFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::OSDriveFolder);
      JS_SetProperty(jscontext, constructor, "OSDriveFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::FileURLFolder);
      JS_SetProperty(jscontext, constructor, "FileURLFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::JavaBinFolder);
      JS_SetProperty(jscontext, constructor, "JavaBinFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::JavaClassesFolder);
      JS_SetProperty(jscontext, constructor, "JavaClassesFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::JavaDownloadFolder);
      JS_SetProperty(jscontext, constructor, "JavaDownloadFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Win_WindowsFolder);
      JS_SetProperty(jscontext, constructor, "Win_WindowsFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Win_SystemFolder);
      JS_SetProperty(jscontext, constructor, "Win_SystemFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Win_System16Folder);
      JS_SetProperty(jscontext, constructor, "Win_System16Folder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Mac_SystemFolder);
      JS_SetProperty(jscontext, constructor, "Mac_SystemFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Mac_DesktopFolder);
      JS_SetProperty(jscontext, constructor, "Mac_DesktopFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Mac_TrashFolder);
      JS_SetProperty(jscontext, constructor, "Mac_TrashFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Mac_StartupFolder);
      JS_SetProperty(jscontext, constructor, "Mac_StartupFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Mac_ShutdownFolder);
      JS_SetProperty(jscontext, constructor, "Mac_ShutdownFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Mac_AppleMenuFolder);
      JS_SetProperty(jscontext, constructor, "Mac_AppleMenuFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Mac_ControlPanelFolder);
      JS_SetProperty(jscontext, constructor, "Mac_ControlPanelFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Mac_ExtensionFolder);
      JS_SetProperty(jscontext, constructor, "Mac_ExtensionFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Mac_FontsFolder);
      JS_SetProperty(jscontext, constructor, "Mac_FontsFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Mac_PreferencesFolder);
      JS_SetProperty(jscontext, constructor, "Mac_PreferencesFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Unix_LocalFolder);
      JS_SetProperty(jscontext, constructor, "Unix_LocalFolder", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstallFolder::Unix_LibFolder);
      JS_SetProperty(jscontext, constructor, "Unix_LibFolder", &vp);

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
// Method for creating a new InstallFolder JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptInstallFolder(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptInstallFolder");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMInstallFolder *aInstallFolder;

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

  if (NS_OK != NS_InitInstallFolderClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIInstallFolderIID, (void **)&aInstallFolder);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &InstallFolderClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aInstallFolder);
  }
  else {
    NS_RELEASE(aInstallFolder);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
