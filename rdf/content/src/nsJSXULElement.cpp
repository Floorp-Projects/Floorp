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
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIDOMXULElement.h"
#include "nsIRDFResource.h"
#include "nsIDOMNodeList.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIControllerIID, NS_ICONTROLLER_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kICSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIXULElementIID, NS_IDOMXULELEMENT_IID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kINodeListIID, NS_IDOMNODELIST_IID);

NS_DEF_PTR(nsIController);
NS_DEF_PTR(nsIDOMElement);
NS_DEF_PTR(nsIDOMCSSStyleDeclaration);
NS_DEF_PTR(nsIRDFCompositeDataSource);
NS_DEF_PTR(nsIDOMXULElement);
NS_DEF_PTR(nsIRDFResource);
NS_DEF_PTR(nsIDOMNodeList);

//
// XULElement property ids
//
enum XULElement_slots {
  XULELEMENT_ID = -1,
  XULELEMENT_CLASSNAME = -2,
  XULELEMENT_STYLE = -3,
  XULELEMENT_DATABASE = -4,
  XULELEMENT_RESOURCE = -5,
  XULELEMENT_CONTROLLER = -6
};

/***********************************************************************/
//
// XULElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetXULElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULElement *a = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case XULELEMENT_ID:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulelement.id", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_OK == a->GetId(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case XULELEMENT_CLASSNAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulelement.classname", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_OK == a->GetClassName(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case XULELEMENT_STYLE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulelement.style", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMCSSStyleDeclaration* prop;
        if (NS_OK == a->GetStyle(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case XULELEMENT_DATABASE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulelement.database", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIRDFCompositeDataSource* prop;
        if (NS_OK == a->GetDatabase(&prop)) {
          // get the js object; n.b., this will do a release on 'prop'
          nsJSUtils::nsConvertXPCObjectToJSVal(prop, nsIRDFCompositeDataSource::GetIID(), cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case XULELEMENT_RESOURCE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulelement.resource", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIRDFResource* prop;
        if (NS_OK == a->GetResource(&prop)) {
          // get the js object; n.b., this will do a release on 'prop'
          nsJSUtils::nsConvertXPCObjectToJSVal(prop, nsIRDFResource::GetIID(), cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case XULELEMENT_CONTROLLER:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulelement.controller", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIController* prop;
        if (NS_OK == a->GetController(&prop)) {
          // get the js object; n.b., this will do a release on 'prop'
          nsJSUtils::nsConvertXPCObjectToJSVal(prop, nsIController::GetIID(), cx, vp);
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
// XULElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetXULElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULElement *a = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case XULELEMENT_ID:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulelement.id", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetId(prop);
        
        break;
      }
      case XULELEMENT_CLASSNAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulelement.classname", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetClassName(prop);
        
        break;
      }
      case XULELEMENT_DATABASE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulelement.database", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIRDFCompositeDataSource* prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToXPCObject((nsISupports **) &prop,
                                                kIRDFCompositeDataSourceIID, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetDatabase(prop);
        NS_IF_RELEASE(prop);
        break;
      }
      case XULELEMENT_CONTROLLER:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "xulelement.controller", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIController* prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToXPCObject((nsISupports **) &prop,
                                                kIControllerIID, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetController(prop);
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
// XULElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeXULElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// XULElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateXULElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// XULElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method AddBroadcastListener
//
PR_STATIC_CALLBACK(JSBool)
XULElementAddBroadcastListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString b0;
  nsIDOMElementPtr b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulelement.addbroadcastlistener", &ok);
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

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kIElementIID,
                                           "Element",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->AddBroadcastListener(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function addBroadcastListener requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method RemoveBroadcastListener
//
PR_STATIC_CALLBACK(JSBool)
XULElementRemoveBroadcastListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString b0;
  nsIDOMElementPtr b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulelement.removebroadcastlistener", &ok);
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

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kIElementIID,
                                           "Element",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->RemoveBroadcastListener(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function removeBroadcastListener requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DoCommand
//
PR_STATIC_CALLBACK(JSBool)
XULElementDoCommand(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulelement.docommand", &ok);
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

    if (NS_OK != nativeThis->DoCommand()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function doCommand requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetElementsByAttribute
//
PR_STATIC_CALLBACK(JSBool)
XULElementGetElementsByAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMNodeList* nativeRet;
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "xulelement.getelementsbyattribute", &ok);
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

    if (NS_OK != nativeThis->GetElementsByAttribute(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function getElementsByAttribute requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for XULElement
//
JSClass XULElementClass = {
  "XULElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetXULElementProperty,
  SetXULElementProperty,
  EnumerateXULElement,
  ResolveXULElement,
  JS_ConvertStub,
  FinalizeXULElement
};


//
// XULElement class properties
//
static JSPropertySpec XULElementProperties[] =
{
  {"id",    XULELEMENT_ID,    JSPROP_ENUMERATE},
  {"className",    XULELEMENT_CLASSNAME,    JSPROP_ENUMERATE},
  {"style",    XULELEMENT_STYLE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"database",    XULELEMENT_DATABASE,    JSPROP_ENUMERATE},
  {"resource",    XULELEMENT_RESOURCE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"controller",    XULELEMENT_CONTROLLER,    JSPROP_ENUMERATE},
  {0}
};


//
// XULElement class methods
//
static JSFunctionSpec XULElementMethods[] = 
{
  {"addBroadcastListener",          XULElementAddBroadcastListener,     2},
  {"removeBroadcastListener",          XULElementRemoveBroadcastListener,     2},
  {"doCommand",          XULElementDoCommand,     0},
  {"getElementsByAttribute",          XULElementGetElementsByAttribute,     2},
  {0}
};


//
// XULElement constructor
//
PR_STATIC_CALLBACK(JSBool)
XULElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// XULElement class initialization
//
extern "C" NS_DOM nsresult NS_InitXULElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XULElement", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitElementClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &XULElementClass,      // JSClass
                         XULElement,            // JSNative ctor
                         0,             // ctor args
                         XULElementProperties,  // proto props
                         XULElementMethods,     // proto funcs
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
// Method for creating a new XULElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptXULElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptXULElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMXULElement *aXULElement;

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

  if (NS_OK != NS_InitXULElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIXULElementIID, (void **)&aXULElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &XULElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aXULElement);
  }
  else {
    NS_RELEASE(aXULElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
