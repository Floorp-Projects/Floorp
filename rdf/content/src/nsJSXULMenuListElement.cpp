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
#include "nsIDOMElement.h"
#include "nsIDOMXULMenuListElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIXULMenuListElementIID, NS_IDOMXULMENULISTELEMENT_IID);

//
// XULMenuListElement property ids
//
enum XULMenuListElement_slots {
  XULMENULISTELEMENT_VALUE = -1,
  XULMENULISTELEMENT_DATA = -2,
  XULMENULISTELEMENT_SELECTEDITEM = -3,
  XULMENULISTELEMENT_SELECTEDINDEX = -4,
  XULMENULISTELEMENT_CROP = -5,
  XULMENULISTELEMENT_DISABLED = -6,
  XULMENULISTELEMENT_SRC = -7
};

/***********************************************************************/
//
// XULMenuListElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetXULMenuListElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULMenuListElement *a = (nsIDOMXULMenuListElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case XULMENULISTELEMENT_VALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_VALUE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetValue(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case XULMENULISTELEMENT_DATA:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_DATA, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetData(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case XULMENULISTELEMENT_SELECTEDITEM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_SELECTEDITEM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMElement* prop;
          rv = a->GetSelectedItem(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case XULMENULISTELEMENT_SELECTEDINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_SELECTEDINDEX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetSelectedIndex(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case XULMENULISTELEMENT_CROP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_CROP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCrop(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case XULMENULISTELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_DISABLED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetDisabled(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case XULMENULISTELEMENT_SRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_SRC, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSrc(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
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
// XULMenuListElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetXULMenuListElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULMenuListElement *a = (nsIDOMXULMenuListElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case XULMENULISTELEMENT_VALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_VALUE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetValue(prop);
          
        }
        break;
      }
      case XULMENULISTELEMENT_DATA:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_DATA, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetData(prop);
          
        }
        break;
      }
      case XULMENULISTELEMENT_SELECTEDITEM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_SELECTEDITEM, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMElement* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIElementIID, NS_ConvertASCIItoUCS2("Element"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetSelectedItem(prop);
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case XULMENULISTELEMENT_SELECTEDINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_SELECTEDINDEX, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          rv = a->SetSelectedIndex(prop);
          
        }
        break;
      }
      case XULMENULISTELEMENT_CROP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_CROP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCrop(prop);
          
        }
        break;
      }
      case XULMENULISTELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_DISABLED, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetDisabled(prop);
          
        }
        break;
      }
      case XULMENULISTELEMENT_SRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULMENULISTELEMENT_SRC, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSrc(prop);
          
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
// XULMenuListElement class properties
//
static JSPropertySpec XULMenuListElementProperties[] =
{
  {"value",    XULMENULISTELEMENT_VALUE,    JSPROP_ENUMERATE},
  {"data",    XULMENULISTELEMENT_DATA,    JSPROP_ENUMERATE},
  {"selectedItem",    XULMENULISTELEMENT_SELECTEDITEM,    JSPROP_ENUMERATE},
  {"selectedIndex",    XULMENULISTELEMENT_SELECTEDINDEX,    JSPROP_ENUMERATE},
  {"crop",    XULMENULISTELEMENT_CROP,    JSPROP_ENUMERATE},
  {"disabled",    XULMENULISTELEMENT_DISABLED,    JSPROP_ENUMERATE},
  {"src",    XULMENULISTELEMENT_SRC,    JSPROP_ENUMERATE},
  {0}
};


//
// XULMenuListElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeXULMenuListElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// XULMenuListElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateXULMenuListElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// XULMenuListElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULMenuListElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for XULMenuListElement
//
JSClass XULMenuListElementClass = {
  "XULMenuListElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetXULMenuListElementProperty,
  SetXULMenuListElementProperty,
  EnumerateXULMenuListElement,
  ResolveXULMenuListElement,
  JS_ConvertStub,
  FinalizeXULMenuListElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// XULMenuListElement class methods
//
static JSFunctionSpec XULMenuListElementMethods[] = 
{
  {0}
};


//
// XULMenuListElement constructor
//
PR_STATIC_CALLBACK(JSBool)
XULMenuListElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// XULMenuListElement class initialization
//
extern "C" NS_DOM nsresult NS_InitXULMenuListElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XULMenuListElement", &vp)) ||
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
                         &XULMenuListElementClass,      // JSClass
                         XULMenuListElement,            // JSNative ctor
                         0,             // ctor args
                         XULMenuListElementProperties,  // proto props
                         XULMenuListElementMethods,     // proto funcs
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
// Method for creating a new XULMenuListElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptXULMenuListElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptXULMenuListElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMXULMenuListElement *aXULMenuListElement;

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

  if (NS_OK != NS_InitXULMenuListElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIXULMenuListElementIID, (void **)&aXULMenuListElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &XULMenuListElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aXULMenuListElement);
  }
  else {
    NS_RELEASE(aXULMenuListElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
