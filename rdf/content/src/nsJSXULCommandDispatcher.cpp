/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsDOMError.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsDOMPropEnums.h"
#include "nsString.h"
#include "nsIController.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIControllers.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIControllerIID, NS_ICONTROLLER_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIWindowInternalIID, NS_IDOMWINDOWINTERNAL_IID);
static NS_DEFINE_IID(kIXULCommandDispatcherIID, NS_IDOMXULCOMMANDDISPATCHER_IID);
static NS_DEFINE_IID(kIControllersIID, NS_ICONTROLLERS_IID);

//
// XULCommandDispatcher property ids
//
enum XULCommandDispatcher_slots {
  XULCOMMANDDISPATCHER_FOCUSEDELEMENT = -1,
  XULCOMMANDDISPATCHER_FOCUSEDWINDOW = -2,
  XULCOMMANDDISPATCHER_SUPPRESSFOCUS = -3,
  XULCOMMANDDISPATCHER_SUPPRESSFOCUSSCROLL = -4,
  XULCOMMANDDISPATCHER_ACTIVE = -5
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

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case XULCOMMANDDISPATCHER_FOCUSEDELEMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_FOCUSEDELEMENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMElement* prop;
          rv = a->GetFocusedElement(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case XULCOMMANDDISPATCHER_FOCUSEDWINDOW:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_FOCUSEDWINDOW, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowInternal* prop;
          rv = a->GetFocusedWindow(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case XULCOMMANDDISPATCHER_SUPPRESSFOCUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_SUPPRESSFOCUS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetSuppressFocus(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case XULCOMMANDDISPATCHER_SUPPRESSFOCUSSCROLL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_SUPPRESSFOCUSSCROLL, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetSuppressFocusScroll(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case XULCOMMANDDISPATCHER_ACTIVE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_ACTIVE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetActive(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
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

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case XULCOMMANDDISPATCHER_FOCUSEDELEMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_FOCUSEDELEMENT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMElement* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIElementIID, NS_ConvertASCIItoUCS2("Element"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetFocusedElement(prop);
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case XULCOMMANDDISPATCHER_FOCUSEDWINDOW:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_FOCUSEDWINDOW, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowInternal* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIWindowInternalIID, NS_ConvertASCIItoUCS2("WindowInternal"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetFocusedWindow(prop);
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case XULCOMMANDDISPATCHER_SUPPRESSFOCUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_SUPPRESSFOCUS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetSuppressFocus(prop);
          
        }
        break;
      }
      case XULCOMMANDDISPATCHER_SUPPRESSFOCUSSCROLL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_SUPPRESSFOCUSSCROLL, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetSuppressFocusScroll(prop);
          
        }
        break;
      }
      case XULCOMMANDDISPATCHER_ACTIVE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_ACTIVE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetActive(prop);
          
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
  return PR_TRUE;
}


//
// XULCommandDispatcher class properties
//
static JSPropertySpec XULCommandDispatcherProperties[] =
{
  {"focusedElement",    XULCOMMANDDISPATCHER_FOCUSEDELEMENT,    JSPROP_ENUMERATE},
  {"focusedWindow",    XULCOMMANDDISPATCHER_FOCUSEDWINDOW,    JSPROP_ENUMERATE},
  {"suppressFocus",    XULCOMMANDDISPATCHER_SUPPRESSFOCUS,    JSPROP_ENUMERATE},
  {"suppressFocusScroll",    XULCOMMANDDISPATCHER_SUPPRESSFOCUSSCROLL,    JSPROP_ENUMERATE},
  {"active",    XULCOMMANDDISPATCHER_ACTIVE,    JSPROP_ENUMERATE},
  {0}
};


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
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// XULCommandDispatcher resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULCommandDispatcher(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method AddCommandUpdater
//
PR_STATIC_CALLBACK(JSBool)
XULCommandDispatcherAddCommandUpdater(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULCommandDispatcher *nativeThis = (nsIDOMXULCommandDispatcher*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMElement> b0;
  nsAutoString b1;
  nsAutoString b2;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_ADDCOMMANDUPDATER, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 3) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIElementIID,
                                           NS_ConvertASCIItoUCS2("Element"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);
    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);

    result = nativeThis->AddCommandUpdater(b0, b1, b2);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsCOMPtr<nsIDOMElement> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_REMOVECOMMANDUPDATER, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIElementIID,
                                           NS_ConvertASCIItoUCS2("Element"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->RemoveCommandUpdater(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_UPDATECOMMANDS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->UpdateCommands(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetControllerForCommand
//
PR_STATIC_CALLBACK(JSBool)
XULCommandDispatcherGetControllerForCommand(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULCommandDispatcher *nativeThis = (nsIDOMXULCommandDispatcher*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIController* nativeRet;
  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_GETCONTROLLERFORCOMMAND, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->GetControllerForCommand(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    // n.b., this will release nativeRet
    nsJSUtils::nsConvertXPCObjectToJSVal(nativeRet, NS_GET_IID(nsIController), cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method GetControllers
//
PR_STATIC_CALLBACK(JSBool)
XULCommandDispatcherGetControllers(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULCommandDispatcher *nativeThis = (nsIDOMXULCommandDispatcher*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIControllers* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCOMMANDDISPATCHER_GETCONTROLLERS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetControllers(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    // n.b., this will release nativeRet
    nsJSUtils::nsConvertXPCObjectToJSVal(nativeRet, NS_GET_IID(nsIControllers), cx, obj, rval);
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
  FinalizeXULCommandDispatcher,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// XULCommandDispatcher class methods
//
static JSFunctionSpec XULCommandDispatcherMethods[] = 
{
  {"addCommandUpdater",          XULCommandDispatcherAddCommandUpdater,     3},
  {"removeCommandUpdater",          XULCommandDispatcherRemoveCommandUpdater,     1},
  {"updateCommands",          XULCommandDispatcherUpdateCommands,     1},
  {"getControllerForCommand",          XULCommandDispatcherGetControllerForCommand,     1},
  {"getControllers",          XULCommandDispatcherGetControllers,     0},
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
