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
#include "nsIDOMDocument.h"
#include "nsIDOMSelection.h"
#include "nsIDOMEditorAppCore.h"
#include "nsIDOMWindow.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kISelectionIID, NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kIEditorAppCoreIID, NS_IDOMEDITORAPPCORE_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

NS_DEF_PTR(nsIDOMDocument);
NS_DEF_PTR(nsIDOMSelection);
NS_DEF_PTR(nsIDOMEditorAppCore);
NS_DEF_PTR(nsIDOMWindow);

//
// EditorAppCore property ids
//
enum EditorAppCore_slots {
  EDITORAPPCORE_CONTENTSASTEXT = -1,
  EDITORAPPCORE_CONTENTSASHTML = -2,
  EDITORAPPCORE_EDITORDOCUMENT = -3,
  EDITORAPPCORE_EDITORSELECTION = -4
};

/***********************************************************************/
//
// EditorAppCore Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetEditorAppCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMEditorAppCore *a = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case EDITORAPPCORE_CONTENTSASTEXT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetContentsAsText(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EDITORAPPCORE_CONTENTSASHTML:
      {
        nsAutoString prop;
        if (NS_OK == a->GetContentsAsHTML(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EDITORAPPCORE_EDITORDOCUMENT:
      {
        nsIDOMDocument* prop;
        if (NS_OK == a->GetEditorDocument(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EDITORAPPCORE_EDITORSELECTION:
      {
        nsIDOMSelection* prop;
        if (NS_OK == a->GetEditorSelection(&prop)) {
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
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// EditorAppCore Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetEditorAppCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMEditorAppCore *a = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);

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
// EditorAppCore finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeEditorAppCore(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// EditorAppCore enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateEditorAppCore(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// EditorAppCore resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveEditorAppCore(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method SetEditorType
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreSetEditorType(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->SetEditorType(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setEditorType requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetTextProperty
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreSetTextProperty(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->SetTextProperty(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setTextProperty requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method RemoveTextProperty
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreRemoveTextProperty(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->RemoveTextProperty(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function removeTextProperty requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetTextProperty
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreGetTextProperty(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  PRBool b1;
  PRBool b2;
  PRBool b3;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 4) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (!nsJSUtils::nsConvertJSValToBool(&b1, cx, argv[1])) {
      return JS_FALSE;
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return JS_FALSE;
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b3, cx, argv[3])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->GetTextProperty(b0, &b1, &b2, &b3)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function getTextProperty requires 4 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Undo
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreUndo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Undo()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function undo requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Redo
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreRedo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Redo()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function redo requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Cut
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreCut(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Cut()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function cut requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Copy
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreCopy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Copy()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function copy requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Paste
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCorePaste(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Paste()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function paste requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SelectAll
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreSelectAll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->SelectAll()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function selectAll requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method BeginBatchChanges
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreBeginBatchChanges(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->BeginBatchChanges()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function beginBatchChanges requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method EndBatchChanges
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreEndBatchChanges(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->EndBatchChanges()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function endBatchChanges requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ShowClipboard
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreShowClipboard(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ShowClipboard()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function showClipboard requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method InsertText
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreInsertText(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->InsertText(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function insertText requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method InsertLink
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreInsertLink(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->InsertLink()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function insertLink requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method InsertImage
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreInsertImage(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->InsertImage()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function insertImage requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Exit
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreExit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Exit()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function exit requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetToolbarWindow
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreSetToolbarWindow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->SetToolbarWindow(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setToolbarWindow requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetContentWindow
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreSetContentWindow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->SetContentWindow(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setContentWindow requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetWebShellWindow
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCoreSetWebShellWindow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEditorAppCore *nativeThis = (nsIDOMEditorAppCore*)JS_GetPrivate(cx, obj);
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
    JS_ReportError(cx, "Function setWebShellWindow requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for EditorAppCore
//
JSClass EditorAppCoreClass = {
  "EditorAppCore", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetEditorAppCoreProperty,
  SetEditorAppCoreProperty,
  EnumerateEditorAppCore,
  ResolveEditorAppCore,
  JS_ConvertStub,
  FinalizeEditorAppCore
};


//
// EditorAppCore class properties
//
static JSPropertySpec EditorAppCoreProperties[] =
{
  {"contentsAsText",    EDITORAPPCORE_CONTENTSASTEXT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"contentsAsHTML",    EDITORAPPCORE_CONTENTSASHTML,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"editorDocument",    EDITORAPPCORE_EDITORDOCUMENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"editorSelection",    EDITORAPPCORE_EDITORSELECTION,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// EditorAppCore class methods
//
static JSFunctionSpec EditorAppCoreMethods[] = 
{
  {"setEditorType",          EditorAppCoreSetEditorType,     1},
  {"setTextProperty",          EditorAppCoreSetTextProperty,     1},
  {"removeTextProperty",          EditorAppCoreRemoveTextProperty,     1},
  {"getTextProperty",          EditorAppCoreGetTextProperty,     4},
  {"undo",          EditorAppCoreUndo,     0},
  {"redo",          EditorAppCoreRedo,     0},
  {"cut",          EditorAppCoreCut,     0},
  {"copy",          EditorAppCoreCopy,     0},
  {"paste",          EditorAppCorePaste,     0},
  {"selectAll",          EditorAppCoreSelectAll,     0},
  {"beginBatchChanges",          EditorAppCoreBeginBatchChanges,     0},
  {"endBatchChanges",          EditorAppCoreEndBatchChanges,     0},
  {"showClipboard",          EditorAppCoreShowClipboard,     0},
  {"insertText",          EditorAppCoreInsertText,     1},
  {"insertLink",          EditorAppCoreInsertLink,     0},
  {"insertImage",          EditorAppCoreInsertImage,     0},
  {"exit",          EditorAppCoreExit,     0},
  {"setToolbarWindow",          EditorAppCoreSetToolbarWindow,     1},
  {"setContentWindow",          EditorAppCoreSetContentWindow,     1},
  {"setWebShellWindow",          EditorAppCoreSetWebShellWindow,     1},
  {0}
};


//
// EditorAppCore constructor
//
PR_STATIC_CALLBACK(JSBool)
EditorAppCore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMEditorAppCore *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;

  static NS_DEFINE_IID(kIDOMEditorAppCoreIID, NS_IDOMEDITORAPPCORE_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("EditorAppCore", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsComponentManager::CreateInstance(classID,
                                        nsnull,
                                        kIDOMEditorAppCoreIID,
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
// EditorAppCore class initialization
//
extern "C" NS_DOM nsresult NS_InitEditorAppCoreClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "EditorAppCore", &vp)) ||
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
                         &EditorAppCoreClass,      // JSClass
                         EditorAppCore,            // JSNative ctor
                         0,             // ctor args
                         EditorAppCoreProperties,  // proto props
                         EditorAppCoreMethods,     // proto funcs
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
// Method for creating a new EditorAppCore JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptEditorAppCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptEditorAppCore");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMEditorAppCore *aEditorAppCore;

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

  if (NS_OK != NS_InitEditorAppCoreClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIEditorAppCoreIID, (void **)&aEditorAppCore);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &EditorAppCoreClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aEditorAppCore);
  }
  else {
    NS_RELEASE(aEditorAppCore);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
