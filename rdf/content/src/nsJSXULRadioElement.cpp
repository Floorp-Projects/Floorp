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
#include "nsIDOMXULRadioElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIXULRadioElementIID, NS_IDOMXULRADIOELEMENT_IID);

//
// XULRadioElement property ids
//
enum XULRadioElement_slots {
  XULRADIOELEMENT_VALUE = -1,
  XULRADIOELEMENT_CROP = -2,
  XULRADIOELEMENT_DISABLED = -3,
  XULRADIOELEMENT_SRC = -4,
  XULRADIOELEMENT_IMGALIGN = -5,
  XULRADIOELEMENT_ACCESSKEY = -6,
  XULRADIOELEMENT_CHECKED = -7
};

/***********************************************************************/
//
// XULRadioElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetXULRadioElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULRadioElement *a = (nsIDOMXULRadioElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECMAN_ERR);
    }
    switch(JSVAL_TO_INT(id)) {
      case XULRADIOELEMENT_VALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_VALUE, PR_FALSE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        nsAutoString prop;
        nsresult result = NS_OK;
        result = a->GetValue(prop);
        if (NS_SUCCEEDED(result)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case XULRADIOELEMENT_CROP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_CROP, PR_FALSE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        nsAutoString prop;
        nsresult result = NS_OK;
        result = a->GetCrop(prop);
        if (NS_SUCCEEDED(result)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case XULRADIOELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_DISABLED, PR_FALSE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        PRBool prop;
        nsresult result = NS_OK;
        result = a->GetDisabled(&prop);
        if (NS_SUCCEEDED(result)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case XULRADIOELEMENT_SRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_SRC, PR_FALSE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        nsAutoString prop;
        nsresult result = NS_OK;
        result = a->GetSrc(prop);
        if (NS_SUCCEEDED(result)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case XULRADIOELEMENT_IMGALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_IMGALIGN, PR_FALSE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        nsAutoString prop;
        nsresult result = NS_OK;
        result = a->GetImgalign(prop);
        if (NS_SUCCEEDED(result)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case XULRADIOELEMENT_ACCESSKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_ACCESSKEY, PR_FALSE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        nsAutoString prop;
        nsresult result = NS_OK;
        result = a->GetAccesskey(prop);
        if (NS_SUCCEEDED(result)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case XULRADIOELEMENT_CHECKED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_CHECKED, PR_FALSE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        PRBool prop;
        nsresult result = NS_OK;
        result = a->GetChecked(&prop);
        if (NS_SUCCEEDED(result)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
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

  return PR_TRUE;
}

/***********************************************************************/
//
// XULRadioElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetXULRadioElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULRadioElement *a = (nsIDOMXULRadioElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECMAN_ERR);
    }
    switch(JSVAL_TO_INT(id)) {
      case XULRADIOELEMENT_VALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_VALUE, PR_TRUE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetValue(prop);
        
        break;
      }
      case XULRADIOELEMENT_CROP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_CROP, PR_TRUE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCrop(prop);
        
        break;
      }
      case XULRADIOELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_DISABLED, PR_TRUE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return nsJSUtils::nsReportError(cx, obj,  NS_ERROR_DOM_NOT_BOOLEAN_ERR);
        }
      
        a->SetDisabled(prop);
        
        break;
      }
      case XULRADIOELEMENT_SRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_SRC, PR_TRUE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSrc(prop);
        
        break;
      }
      case XULRADIOELEMENT_IMGALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_IMGALIGN, PR_TRUE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetImgalign(prop);
        
        break;
      }
      case XULRADIOELEMENT_ACCESSKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_ACCESSKEY, PR_TRUE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAccesskey(prop);
        
        break;
      }
      case XULRADIOELEMENT_CHECKED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULRADIOELEMENT_CHECKED, PR_TRUE);
        if (NS_FAILED(rv)) {
          return nsJSUtils::nsReportError(cx, obj, rv);
        }
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return nsJSUtils::nsReportError(cx, obj,  NS_ERROR_DOM_NOT_BOOLEAN_ERR);
        }
      
        a->SetChecked(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
  }

  return PR_TRUE;
}


//
// XULRadioElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeXULRadioElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// XULRadioElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateXULRadioElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// XULRadioElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULRadioElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for XULRadioElement
//
JSClass XULRadioElementClass = {
  "XULRadioElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetXULRadioElementProperty,
  SetXULRadioElementProperty,
  EnumerateXULRadioElement,
  ResolveXULRadioElement,
  JS_ConvertStub,
  FinalizeXULRadioElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// XULRadioElement class properties
//
static JSPropertySpec XULRadioElementProperties[] =
{
  {"value",    XULRADIOELEMENT_VALUE,    JSPROP_ENUMERATE},
  {"crop",    XULRADIOELEMENT_CROP,    JSPROP_ENUMERATE},
  {"disabled",    XULRADIOELEMENT_DISABLED,    JSPROP_ENUMERATE},
  {"src",    XULRADIOELEMENT_SRC,    JSPROP_ENUMERATE},
  {"imgalign",    XULRADIOELEMENT_IMGALIGN,    JSPROP_ENUMERATE},
  {"accesskey",    XULRADIOELEMENT_ACCESSKEY,    JSPROP_ENUMERATE},
  {"checked",    XULRADIOELEMENT_CHECKED,    JSPROP_ENUMERATE},
  {0}
};


//
// XULRadioElement class methods
//
static JSFunctionSpec XULRadioElementMethods[] = 
{
  {0}
};


//
// XULRadioElement constructor
//
PR_STATIC_CALLBACK(JSBool)
XULRadioElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// XULRadioElement class initialization
//
extern "C" NS_DOM nsresult NS_InitXULRadioElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XULRadioElement", &vp)) ||
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
                         &XULRadioElementClass,      // JSClass
                         XULRadioElement,            // JSNative ctor
                         0,             // ctor args
                         XULRadioElementProperties,  // proto props
                         XULRadioElementMethods,     // proto funcs
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
// Method for creating a new XULRadioElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptXULRadioElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptXULRadioElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMXULRadioElement *aXULRadioElement;

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

  if (NS_OK != NS_InitXULRadioElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIXULRadioElementIID, (void **)&aXULRadioElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &XULRadioElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aXULRadioElement);
  }
  else {
    NS_RELEASE(aXULRadioElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
