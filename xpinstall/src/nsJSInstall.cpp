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
#include "nsIDOMInstall.h"
#include "nsIDOMInstallFolder.h"
#include "nsIDOMInstallVersion.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsRepository.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIInstallIID, NS_IDOMINSTALL_IID);
static NS_DEFINE_IID(kIInstallFolderIID, NS_IDOMINSTALLFOLDER_IID);
static NS_DEFINE_IID(kIInstallVersionIID, NS_IDOMINSTALLVERSION_IID);

NS_DEF_PTR(nsIDOMInstall);
NS_DEF_PTR(nsIDOMInstallFolder);
NS_DEF_PTR(nsIDOMInstallVersion);

//
// Install property ids
//
enum Install_slots {
  INSTALL_USERPACKAGENAME = -1,
  INSTALL_REGPACKAGENAME = -2
};

/***********************************************************************/
//
// Install Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetInstallProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMInstall *a = (nsIDOMInstall*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case INSTALL_USERPACKAGENAME:
      {
        nsAutoString prop;
        if (NS_OK == a->GetUserPackageName(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case INSTALL_REGPACKAGENAME:
      {
        nsAutoString prop;
        if (NS_OK == a->GetRegPackageName(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
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
// Install Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetInstallProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMInstall *a = (nsIDOMInstall*)JS_GetPrivate(cx, obj);

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
// Install finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeInstall(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Install enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateInstall(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// Install resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveInstall(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method AbortInstall
//
PR_STATIC_CALLBACK(JSBool)
InstallAbortInstall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->AbortInstall()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function AbortInstall requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method AddDirectory
//
PR_STATIC_CALLBACK(JSBool)
InstallAddDirectory(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  nsIDOMInstallFolderPtr b3;
  nsAutoString b4;
  PRBool b5;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 6) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b3,
                                           kIInstallFolderIID,
                                           "InstallFolder",
                                           cx,
                                           argv[3])) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b4, cx, argv[4]);

    if (!nsJSUtils::nsConvertJSValToBool(&b5, cx, argv[5])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->AddDirectory(b0, b1, b2, b3, b4, b5, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function AddDirectory requires 6 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method AddSubcomponent
//
PR_STATIC_CALLBACK(JSBool)
InstallAddSubcomponent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsIDOMInstallVersionPtr b1;
  nsAutoString b2;
  nsIDOMInstallFolderPtr b3;
  nsAutoString b4;
  PRBool b5;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 6) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kIInstallVersionIID,
                                           "InstallVersion",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b3,
                                           kIInstallFolderIID,
                                           "InstallFolder",
                                           cx,
                                           argv[3])) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b4, cx, argv[4]);

    if (!nsJSUtils::nsConvertJSValToBool(&b5, cx, argv[5])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->AddSubcomponent(b0, b1, b2, b3, b4, b5, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function AddSubcomponent requires 6 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DeleteComponent
//
PR_STATIC_CALLBACK(JSBool)
InstallDeleteComponent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->DeleteComponent(b0, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function DeleteComponent requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DeleteFile
//
PR_STATIC_CALLBACK(JSBool)
InstallDeleteFile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsIDOMInstallFolderPtr b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIInstallFolderIID,
                                           "InstallFolder",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    if (NS_OK != nativeThis->DeleteFile(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function DeleteFile requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DiskSpaceAvailable
//
PR_STATIC_CALLBACK(JSBool)
InstallDiskSpaceAvailable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsIDOMInstallFolderPtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIInstallFolderIID,
                                           "InstallFolder",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->DiskSpaceAvailable(b0, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function DiskSpaceAvailable requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Execute
//
PR_STATIC_CALLBACK(JSBool)
InstallExecute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->Execute(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function Execute requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method FinalizeInstall
//
PR_STATIC_CALLBACK(JSBool)
InstallFinalizeInstall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->FinalizeInstall(&nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function FinalizeInstall requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Gestalt
//
PR_STATIC_CALLBACK(JSBool)
InstallGestalt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->Gestalt(b0, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function Gestalt requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetComponentFolder
//
PR_STATIC_CALLBACK(JSBool)
InstallGetComponentFolder(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMInstallFolder* nativeRet;
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

    if (NS_OK != nativeThis->GetComponentFolder(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function GetComponentFolder requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetFolder
//
PR_STATIC_CALLBACK(JSBool)
InstallGetFolder(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMInstallFolder* nativeRet;
  nsIDOMInstallFolderPtr b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIInstallFolderIID,
                                           "InstallFolder",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    if (NS_OK != nativeThis->GetFolder(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function GetFolder requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetLastError
//
PR_STATIC_CALLBACK(JSBool)
InstallGetLastError(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->GetLastError(&nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function GetLastError requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetWinProfile
//
PR_STATIC_CALLBACK(JSBool)
InstallGetWinProfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsIDOMInstallFolderPtr b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIInstallFolderIID,
                                           "InstallFolder",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    if (NS_OK != nativeThis->GetWinProfile(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function GetWinProfile requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetWinRegistry
//
PR_STATIC_CALLBACK(JSBool)
InstallGetWinRegistry(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->GetWinRegistry(&nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function GetWinRegistry requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Patch
//
PR_STATIC_CALLBACK(JSBool)
InstallPatch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  nsIDOMInstallFolderPtr b3;
  nsAutoString b4;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 5) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b3,
                                           kIInstallFolderIID,
                                           "InstallFolder",
                                           cx,
                                           argv[3])) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b4, cx, argv[4]);

    if (NS_OK != nativeThis->Patch(b0, b1, b2, b3, b4, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function Patch requires 5 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ResetError
//
PR_STATIC_CALLBACK(JSBool)
InstallResetError(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ResetError()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function ResetError requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetPackageFolder
//
PR_STATIC_CALLBACK(JSBool)
InstallSetPackageFolder(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMInstallFolderPtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIInstallFolderIID,
                                           "InstallFolder",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SetPackageFolder(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function SetPackageFolder requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method StartInstall
//
PR_STATIC_CALLBACK(JSBool)
InstallStartInstall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  PRInt32 b3;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 4) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);

    if (!JS_ValueToInt32(cx, argv[3], (int32 *)&b3)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->StartInstall(b0, b1, b2, b3, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function StartInstall requires 4 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Uninstall
//
PR_STATIC_CALLBACK(JSBool)
InstallUninstall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->Uninstall(b0, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function Uninstall requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ExtractFileFromJar
//
PR_STATIC_CALLBACK(JSBool)
InstallExtractFileFromJar(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstall *nativeThis = (nsIDOMInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  nsAutoString b3;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 4) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);

    nsJSUtils::nsConvertJSValToString(b3, cx, argv[3]);

    if (NS_OK != nativeThis->ExtractFileFromJar(b0, b1, b2, b3)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function ExtractFileFromJar requires 4 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Install
//
JSClass InstallClass = {
  "Install", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetInstallProperty,
  SetInstallProperty,
  EnumerateInstall,
  ResolveInstall,
  JS_ConvertStub,
  FinalizeInstall
};


//
// Install class properties
//
static JSPropertySpec InstallProperties[] =
{
  {"UserPackageName",    INSTALL_USERPACKAGENAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"RegPackageName",    INSTALL_REGPACKAGENAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Install class methods
//
static JSFunctionSpec InstallMethods[] = 
{
  {"AbortInstall",          InstallAbortInstall,     0},
  {"AddDirectory",          InstallAddDirectory,     6},
  {"AddSubcomponent",          InstallAddSubcomponent,     6},
  {"DeleteComponent",          InstallDeleteComponent,     1},
  {"DeleteFile",          InstallDeleteFile,     2},
  {"DiskSpaceAvailable",          InstallDiskSpaceAvailable,     1},
  {"Execute",          InstallExecute,     2},
  {"FinalizeInstall",          InstallFinalizeInstall,     0},
  {"Gestalt",          InstallGestalt,     1},
  {"GetComponentFolder",          InstallGetComponentFolder,     2},
  {"GetFolder",          InstallGetFolder,     2},
  {"GetLastError",          InstallGetLastError,     0},
  {"GetWinProfile",          InstallGetWinProfile,     2},
  {"GetWinRegistry",          InstallGetWinRegistry,     0},
  {"Patch",          InstallPatch,     5},
  {"ResetError",          InstallResetError,     0},
  {"SetPackageFolder",          InstallSetPackageFolder,     1},
  {"StartInstall",          InstallStartInstall,     4},
  {"Uninstall",          InstallUninstall,     1},
  {"ExtractFileFromJar",          InstallExtractFileFromJar,     4},
  {0}
};


//
// Install constructor
//
PR_STATIC_CALLBACK(JSBool)
Install(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMInstall *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;

  static NS_DEFINE_IID(kIDOMInstallIID, NS_IDOMINSTALL_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("Install", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsRepository::CreateInstance(classID,
                                        nsnull,
                                        kIDOMInstallIID,
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
// Install class initialization
//
nsresult NS_InitInstallClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Install", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &InstallClass,      // JSClass
                         Install,            // JSNative ctor
                         0,             // ctor args
                         InstallProperties,  // proto props
                         InstallMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "Install", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_BAD_PACKAGE_NAME);
      JS_SetProperty(jscontext, constructor, "SUERR_BAD_PACKAGE_NAME", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_UNEXPECTED_ERROR);
      JS_SetProperty(jscontext, constructor, "SUERR_UNEXPECTED_ERROR", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_ACCESS_DENIED);
      JS_SetProperty(jscontext, constructor, "SUERR_ACCESS_DENIED", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_TOO_MANY_CERTIFICATES);
      JS_SetProperty(jscontext, constructor, "SUERR_TOO_MANY_CERTIFICATES", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_NO_INSTALLER_CERTIFICATE);
      JS_SetProperty(jscontext, constructor, "SUERR_NO_INSTALLER_CERTIFICATE", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_NO_CERTIFICATE);
      JS_SetProperty(jscontext, constructor, "SUERR_NO_CERTIFICATE", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_NO_MATCHING_CERTIFICATE);
      JS_SetProperty(jscontext, constructor, "SUERR_NO_MATCHING_CERTIFICATE", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_UNKNOWN_JAR_FILE);
      JS_SetProperty(jscontext, constructor, "SUERR_UNKNOWN_JAR_FILE", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_INVALID_ARGUMENTS);
      JS_SetProperty(jscontext, constructor, "SUERR_INVALID_ARGUMENTS", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_ILLEGAL_RELATIVE_PATH);
      JS_SetProperty(jscontext, constructor, "SUERR_ILLEGAL_RELATIVE_PATH", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_USER_CANCELLED);
      JS_SetProperty(jscontext, constructor, "SUERR_USER_CANCELLED", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_INSTALL_NOT_STARTED);
      JS_SetProperty(jscontext, constructor, "SUERR_INSTALL_NOT_STARTED", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_SILENT_MODE_DENIED);
      JS_SetProperty(jscontext, constructor, "SUERR_SILENT_MODE_DENIED", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_NO_SUCH_COMPONENT);
      JS_SetProperty(jscontext, constructor, "SUERR_NO_SUCH_COMPONENT", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_FILE_DOES_NOT_EXIST);
      JS_SetProperty(jscontext, constructor, "SUERR_FILE_DOES_NOT_EXIST", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_FILE_READ_ONLY);
      JS_SetProperty(jscontext, constructor, "SUERR_FILE_READ_ONLY", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_FILE_IS_DIRECTORY);
      JS_SetProperty(jscontext, constructor, "SUERR_FILE_IS_DIRECTORY", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_NETWORK_FILE_IS_IN_USE);
      JS_SetProperty(jscontext, constructor, "SUERR_NETWORK_FILE_IS_IN_USE", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_APPLE_SINGLE_ERR);
      JS_SetProperty(jscontext, constructor, "SUERR_APPLE_SINGLE_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_INVALID_PATH_ERR);
      JS_SetProperty(jscontext, constructor, "SUERR_INVALID_PATH_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_PATCH_BAD_DIFF);
      JS_SetProperty(jscontext, constructor, "SUERR_PATCH_BAD_DIFF", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_PATCH_BAD_CHECKSUM_TARGET);
      JS_SetProperty(jscontext, constructor, "SUERR_PATCH_BAD_CHECKSUM_TARGET", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_PATCH_BAD_CHECKSUM_RESULT);
      JS_SetProperty(jscontext, constructor, "SUERR_PATCH_BAD_CHECKSUM_RESULT", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_UNINSTALL_FAILED);
      JS_SetProperty(jscontext, constructor, "SUERR_UNINSTALL_FAILED", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_GESTALT_UNKNOWN_ERR);
      JS_SetProperty(jscontext, constructor, "SUERR_GESTALT_UNKNOWN_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SUERR_GESTALT_INVALID_ARGUMENT);
      JS_SetProperty(jscontext, constructor, "SUERR_GESTALT_INVALID_ARGUMENT", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SU_SUCCESS);
      JS_SetProperty(jscontext, constructor, "SU_SUCCESS", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SU_REBOOT_NEEDED);
      JS_SetProperty(jscontext, constructor, "SU_REBOOT_NEEDED", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SU_LIMITED_INSTALL);
      JS_SetProperty(jscontext, constructor, "SU_LIMITED_INSTALL", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SU_FULL_INSTALL);
      JS_SetProperty(jscontext, constructor, "SU_FULL_INSTALL", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SU_NO_STATUS_DLG);
      JS_SetProperty(jscontext, constructor, "SU_NO_STATUS_DLG", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SU_NO_FINALIZE_DLG);
      JS_SetProperty(jscontext, constructor, "SU_NO_FINALIZE_DLG", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SU_INSTALL_FILE_UNEXPECTED_MSG_ID);
      JS_SetProperty(jscontext, constructor, "SU_INSTALL_FILE_UNEXPECTED_MSG_ID", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SU_DETAILS_REPLACE_FILE_MSG_ID);
      JS_SetProperty(jscontext, constructor, "SU_DETAILS_REPLACE_FILE_MSG_ID", &vp);

      vp = INT_TO_JSVAL(nsIDOMInstall::SU_DETAILS_INSTALL_FILE_MSG_ID);
      JS_SetProperty(jscontext, constructor, "SU_DETAILS_INSTALL_FILE_MSG_ID", &vp);

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
// Method for creating a new Install JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptInstall(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptInstall");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMInstall *aInstall;

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

  if (NS_OK != NS_InitInstallClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIInstallIID, (void **)&aInstall);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &InstallClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aInstall);
  }
  else {
    NS_RELEASE(aInstall);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
