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
#include "nsIDOMNode.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMNodeList.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIXULCommandDispatcherIID, NS_IDOMXULCOMMANDDISPATCHER_IID);
static NS_DEFINE_IID(kIXULDocumentIID, NS_IDOMXULDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);
static NS_DEFINE_IID(kINodeListIID, NS_IDOMNODELIST_IID);

//
// XULDocument property ids
//
enum XULDocument_slots {
  XULDOCUMENT_POPUPNODE = -1,
  XULDOCUMENT_TOOLTIPNODE = -2,
  XULDOCUMENT_COMMANDDISPATCHER = -3,
  XULDOCUMENT_WIDTH = -4,
  XULDOCUMENT_HEIGHT = -5,
  XULDOCUMENT_CONTROLS = -6
};

/***********************************************************************/
//
// XULDocument Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetXULDocumentProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULDocument *a = (nsIDOMXULDocument*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case XULDOCUMENT_POPUPNODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULDOCUMENT_POPUPNODE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNode* prop;
          rv = a->GetPopupNode(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case XULDOCUMENT_TOOLTIPNODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULDOCUMENT_TOOLTIPNODE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNode* prop;
          rv = a->GetTooltipNode(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case XULDOCUMENT_COMMANDDISPATCHER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULDOCUMENT_COMMANDDISPATCHER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMXULCommandDispatcher* prop;
          rv = a->GetCommandDispatcher(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case XULDOCUMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULDOCUMENT_WIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetWidth(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case XULDOCUMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULDOCUMENT_HEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetHeight(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case XULDOCUMENT_CONTROLS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULDOCUMENT_CONTROLS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLCollection* prop;
          rv = a->GetControls(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
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
// XULDocument Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetXULDocumentProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULDocument *a = (nsIDOMXULDocument*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case XULDOCUMENT_POPUPNODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULDOCUMENT_POPUPNODE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNode* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kINodeIID, NS_ConvertASCIItoUCS2("Node"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetPopupNode(prop);
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case XULDOCUMENT_TOOLTIPNODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULDOCUMENT_TOOLTIPNODE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNode* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kINodeIID, NS_ConvertASCIItoUCS2("Node"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetTooltipNode(prop);
          NS_IF_RELEASE(prop);
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
// XULDocument finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeXULDocument(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// XULDocument enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateXULDocument(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// XULDocument resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULDocument(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method GetElementsByAttribute
//
PR_STATIC_CALLBACK(JSBool)
XULDocumentGetElementsByAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULDocument *nativeThis = (nsIDOMXULDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMNodeList* nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULDOCUMENT_GETELEMENTSBYATTRIBUTE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    result = nativeThis->GetElementsByAttribute(b0, b1, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method Persist
//
PR_STATIC_CALLBACK(JSBool)
XULDocumentPersist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULDocument *nativeThis = (nsIDOMXULDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString b0;
  nsAutoString b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULDOCUMENT_PERSIST, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    result = nativeThis->Persist(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for XULDocument
//
JSClass XULDocumentClass = {
  "XULDocument", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetXULDocumentProperty,
  SetXULDocumentProperty,
  EnumerateXULDocument,
  ResolveXULDocument,
  JS_ConvertStub,
  FinalizeXULDocument,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// XULDocument class properties
//
static JSPropertySpec XULDocumentProperties[] =
{
  {"popupNode",    XULDOCUMENT_POPUPNODE,    JSPROP_ENUMERATE},
  {"tooltipNode",    XULDOCUMENT_TOOLTIPNODE,    JSPROP_ENUMERATE},
  {"commandDispatcher",    XULDOCUMENT_COMMANDDISPATCHER,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"width",    XULDOCUMENT_WIDTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"height",    XULDOCUMENT_HEIGHT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"controls",    XULDOCUMENT_CONTROLS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// XULDocument class methods
//
static JSFunctionSpec XULDocumentMethods[] = 
{
  {"getElementsByAttribute",          XULDocumentGetElementsByAttribute,     2},
  {"persist",          XULDocumentPersist,     2},
  {0}
};


//
// XULDocument constructor
//
PR_STATIC_CALLBACK(JSBool)
XULDocument(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// XULDocument class initialization
//
extern "C" NS_DOM nsresult NS_InitXULDocumentClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XULDocument", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitDocumentClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &XULDocumentClass,      // JSClass
                         XULDocument,            // JSNative ctor
                         0,             // ctor args
                         XULDocumentProperties,  // proto props
                         XULDocumentMethods,     // proto funcs
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
// Method for creating a new XULDocument JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptXULDocument(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptXULDocument");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMXULDocument *aXULDocument;

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

  if (NS_OK != NS_InitXULDocumentClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIXULDocumentIID, (void **)&aXULDocument);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &XULDocumentClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aXULDocument);
  }
  else {
    NS_RELEASE(aXULDocument);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
