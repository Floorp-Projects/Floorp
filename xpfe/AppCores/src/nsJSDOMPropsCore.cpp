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
#include "nsIDOMNode.h"
#include "nsIDOMDOMPropsCore.h"
#include "nsIDOMWindow.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMPropsCoreIID, NS_IDOMDOMPROPSCORE_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

NS_DEF_PTR(nsIDOMNode);
NS_DEF_PTR(nsIDOMDOMPropsCore);
NS_DEF_PTR(nsIDOMWindow);

//
// DOMPropsCore property ids
//
enum DOMPropsCore_slots {
  DOMPROPSCORE_NODE = -1
};

/***********************************************************************/
//
// DOMPropsCore Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetDOMPropsCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMDOMPropsCore *a = (nsIDOMDOMPropsCore*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case DOMPROPSCORE_NODE:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetNode(&prop)) {
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
// DOMPropsCore Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetDOMPropsCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMDOMPropsCore *a = (nsIDOMDOMPropsCore*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case DOMPROPSCORE_NODE:
      {
        nsIDOMNode* prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                kINodeIID, "Node",
                                                cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetNode(prop);
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
// DOMPropsCore finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeDOMPropsCore(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// DOMPropsCore enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateDOMPropsCore(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// DOMPropsCore resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveDOMPropsCore(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method ShowProperties
//
PR_STATIC_CALLBACK(JSBool)
DOMPropsCoreShowProperties(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMDOMPropsCore *nativeThis = (nsIDOMDOMPropsCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsIDOMWindowPtr b1;
  nsIDOMNodePtr b2;

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

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b2,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[2])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ShowProperties(b0, b1, b2)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function showProperties requires 3 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Commit
//
PR_STATIC_CALLBACK(JSBool)
DOMPropsCoreCommit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMDOMPropsCore *nativeThis = (nsIDOMDOMPropsCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Commit()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function commit requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Cancel
//
PR_STATIC_CALLBACK(JSBool)
DOMPropsCoreCancel(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMDOMPropsCore *nativeThis = (nsIDOMDOMPropsCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Cancel()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function cancel requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for DOMPropsCore
//
JSClass DOMPropsCoreClass = {
  "DOMPropsCore", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetDOMPropsCoreProperty,
  SetDOMPropsCoreProperty,
  EnumerateDOMPropsCore,
  ResolveDOMPropsCore,
  JS_ConvertStub,
  FinalizeDOMPropsCore
};


//
// DOMPropsCore class properties
//
static JSPropertySpec DOMPropsCoreProperties[] =
{
  {"node",    DOMPROPSCORE_NODE,    JSPROP_ENUMERATE},
  {0}
};


//
// DOMPropsCore class methods
//
static JSFunctionSpec DOMPropsCoreMethods[] = 
{
  {"showProperties",          DOMPropsCoreShowProperties,     3},
  {"commit",          DOMPropsCoreCommit,     0},
  {"cancel",          DOMPropsCoreCancel,     0},
  {0}
};


//
// DOMPropsCore constructor
//
PR_STATIC_CALLBACK(JSBool)
DOMPropsCore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMDOMPropsCore *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;

  static NS_DEFINE_IID(kIDOMDOMPropsCoreIID, NS_IDOMDOMPROPSCORE_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("DOMPropsCore", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsComponentManager::CreateInstance(classID,
                                        nsnull,
                                        kIDOMDOMPropsCoreIID,
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
// DOMPropsCore class initialization
//
extern "C" NS_DOM nsresult NS_InitDOMPropsCoreClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "DOMPropsCore", &vp)) ||
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
                         &DOMPropsCoreClass,      // JSClass
                         DOMPropsCore,            // JSNative ctor
                         0,             // ctor args
                         DOMPropsCoreProperties,  // proto props
                         DOMPropsCoreMethods,     // proto funcs
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
// Method for creating a new DOMPropsCore JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptDOMPropsCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptDOMPropsCore");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMDOMPropsCore *aDOMPropsCore;

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

  if (NS_OK != NS_InitDOMPropsCoreClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIDOMPropsCoreIID, (void **)&aDOMPropsCore);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &DOMPropsCoreClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aDOMPropsCore);
  }
  else {
    NS_RELEASE(aDOMPropsCore);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
