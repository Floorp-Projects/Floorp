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
#include "nsIDOMXULCheckboxElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIXULCheckboxElementIID, NS_IDOMXULCHECKBOXELEMENT_IID);

//
// XULCheckboxElement property ids
//
enum XULCheckboxElement_slots {
  XULCHECKBOXELEMENT_VALUE = -1,
  XULCHECKBOXELEMENT_CROP = -2,
  XULCHECKBOXELEMENT_DISABLED = -3,
  XULCHECKBOXELEMENT_SRC = -4,
  XULCHECKBOXELEMENT_IMGALIGN = -5,
  XULCHECKBOXELEMENT_ACCESSKEY = -6,
  XULCHECKBOXELEMENT_CHECKED = -7
};

/***********************************************************************/
//
// XULCheckboxElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetXULCheckboxElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULCheckboxElement *a = (nsIDOMXULCheckboxElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case XULCHECKBOXELEMENT_VALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_VALUE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetValue(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case XULCHECKBOXELEMENT_CROP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_CROP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCrop(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case XULCHECKBOXELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_DISABLED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetDisabled(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case XULCHECKBOXELEMENT_SRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_SRC, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSrc(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case XULCHECKBOXELEMENT_IMGALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_IMGALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetImgalign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case XULCHECKBOXELEMENT_ACCESSKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_ACCESSKEY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAccesskey(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case XULCHECKBOXELEMENT_CHECKED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_CHECKED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetChecked(&prop);
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
// XULCheckboxElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetXULCheckboxElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULCheckboxElement *a = (nsIDOMXULCheckboxElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case XULCHECKBOXELEMENT_VALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_VALUE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetValue(prop);
          
        }
        break;
      }
      case XULCHECKBOXELEMENT_CROP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_CROP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCrop(prop);
          
        }
        break;
      }
      case XULCHECKBOXELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_DISABLED, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
          }
      
          rv = a->SetDisabled(prop);
          
        }
        break;
      }
      case XULCHECKBOXELEMENT_SRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_SRC, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSrc(prop);
          
        }
        break;
      }
      case XULCHECKBOXELEMENT_IMGALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_IMGALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetImgalign(prop);
          
        }
        break;
      }
      case XULCHECKBOXELEMENT_ACCESSKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_ACCESSKEY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAccesskey(prop);
          
        }
        break;
      }
      case XULCHECKBOXELEMENT_CHECKED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULCHECKBOXELEMENT_CHECKED, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
          }
      
          rv = a->SetChecked(prop);
          
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
// XULCheckboxElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeXULCheckboxElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// XULCheckboxElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateXULCheckboxElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// XULCheckboxElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULCheckboxElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for XULCheckboxElement
//
JSClass XULCheckboxElementClass = {
  "XULCheckboxElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetXULCheckboxElementProperty,
  SetXULCheckboxElementProperty,
  EnumerateXULCheckboxElement,
  ResolveXULCheckboxElement,
  JS_ConvertStub,
  FinalizeXULCheckboxElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// XULCheckboxElement class properties
//
static JSPropertySpec XULCheckboxElementProperties[] =
{
  {"value",    XULCHECKBOXELEMENT_VALUE,    JSPROP_ENUMERATE},
  {"crop",    XULCHECKBOXELEMENT_CROP,    JSPROP_ENUMERATE},
  {"disabled",    XULCHECKBOXELEMENT_DISABLED,    JSPROP_ENUMERATE},
  {"src",    XULCHECKBOXELEMENT_SRC,    JSPROP_ENUMERATE},
  {"imgalign",    XULCHECKBOXELEMENT_IMGALIGN,    JSPROP_ENUMERATE},
  {"accesskey",    XULCHECKBOXELEMENT_ACCESSKEY,    JSPROP_ENUMERATE},
  {"checked",    XULCHECKBOXELEMENT_CHECKED,    JSPROP_ENUMERATE},
  {0}
};


//
// XULCheckboxElement class methods
//
static JSFunctionSpec XULCheckboxElementMethods[] = 
{
  {0}
};


//
// XULCheckboxElement constructor
//
PR_STATIC_CALLBACK(JSBool)
XULCheckboxElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// XULCheckboxElement class initialization
//
extern "C" NS_DOM nsresult NS_InitXULCheckboxElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XULCheckboxElement", &vp)) ||
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
                         &XULCheckboxElementClass,      // JSClass
                         XULCheckboxElement,            // JSNative ctor
                         0,             // ctor args
                         XULCheckboxElementProperties,  // proto props
                         XULCheckboxElementMethods,     // proto funcs
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
// Method for creating a new XULCheckboxElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptXULCheckboxElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptXULCheckboxElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMXULCheckboxElement *aXULCheckboxElement;

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

  if (NS_OK != NS_InitXULCheckboxElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIXULCheckboxElementIID, (void **)&aXULCheckboxElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &XULCheckboxElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aXULCheckboxElement);
  }
  else {
    NS_RELEASE(aXULCheckboxElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
