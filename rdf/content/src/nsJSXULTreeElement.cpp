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
#include "nsIRDFCompositeDataSource.h"
#include "nsIDOMXULTreeElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIXULTreeElementIID, NS_IDOMXULTREEELEMENT_IID);

NS_DEF_PTR(nsIRDFCompositeDataSource);
NS_DEF_PTR(nsIDOMXULTreeElement);

//
// XULTreeElement property ids
//
enum XULTreeElement_slots {
  XULTREEELEMENT_DATABASE = -1
};

/***********************************************************************/
//
// XULTreeElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetXULTreeElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULTreeElement *a = (nsIDOMXULTreeElement*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case XULTREEELEMENT_DATABASE:
      {
        nsIRDFCompositeDataSource* prop;
        if (NS_OK == a->GetDatabase(&prop)) {
          // get the js object
#ifdef XPIDL_JS_STUBS
          *vp = OBJECT_TO_JSVAL(nsIRDFCompositeDataSource::GetJSObject(cx, prop));
#else
          nsJSUtils::nsConvertXPCObjectToJSVal(prop, nsIRDFCompositeDataSource::GetIID(), cx, vp);
#endif
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
// XULTreeElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetXULTreeElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULTreeElement *a = (nsIDOMXULTreeElement*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case XULTREEELEMENT_DATABASE:
      {
        nsIRDFCompositeDataSource* prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                kIRDFCompositeDataSourceIID, "RDFCompositeDataSource",
                                                cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetDatabase(prop);
        
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
// XULTreeElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeXULTreeElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// XULTreeElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateXULTreeElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// XULTreeElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULTreeElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for XULTreeElement
//
JSClass XULTreeElementClass = {
  "XULTreeElement", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetXULTreeElementProperty,
  SetXULTreeElementProperty,
  EnumerateXULTreeElement,
  ResolveXULTreeElement,
  JS_ConvertStub,
  FinalizeXULTreeElement
};


//
// XULTreeElement class properties
//
static JSPropertySpec XULTreeElementProperties[] =
{
  {"database",    XULTREEELEMENT_DATABASE,    JSPROP_ENUMERATE},
  {0}
};


//
// XULTreeElement class methods
//
static JSFunctionSpec XULTreeElementMethods[] = 
{
  {0}
};


//
// XULTreeElement constructor
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// XULTreeElement class initialization
//
extern "C" NS_DOM nsresult NS_InitXULTreeElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XULTreeElement", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitXULElementClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &XULTreeElementClass,      // JSClass
                         XULTreeElement,            // JSNative ctor
                         0,             // ctor args
                         XULTreeElementProperties,  // proto props
                         XULTreeElementMethods,     // proto funcs
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
// Method for creating a new XULTreeElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptXULTreeElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptXULTreeElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMXULTreeElement *aXULTreeElement;

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

  if (NS_OK != NS_InitXULTreeElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIXULTreeElementIID, (void **)&aXULTreeElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &XULTreeElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aXULTreeElement);
  }
  else {
    NS_RELEASE(aXULTreeElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
