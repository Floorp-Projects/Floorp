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
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsString.h"
#include "nsIDOMInstallVersion.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsRepository.h"
#include "nsDOMCID.h"

#include "nsSoftwareUpdateIIDs.h"

extern void ConvertJSValToStr(nsString&  aString,
                             JSContext* aContext,
                             jsval      aValue);

extern void ConvertStrToJSVal(const nsString& aProp,
                             JSContext* aContext,
                             jsval* aReturn);

extern PRBool ConvertJSValToBool(PRBool* aProp,
                                JSContext* aContext,
                                jsval aValue);

extern PRBool ConvertJSValToObj(nsISupports** aSupports,
                               REFNSIID aIID,
                               const nsString& aTypeName,
                               JSContext* aContext,
                               jsval aValue);

void ConvertJSvalToVersionString(nsString& versionString, JSContext* cx, jsval* argument);


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIInstallVersionIID, NS_IDOMINSTALLVERSION_IID);

//
// InstallVersion property ids
//
enum InstallVersion_slots {
  INSTALLVERSION_MAJOR = -1,
  INSTALLVERSION_MINOR = -2,
  INSTALLVERSION_RELEASE = -3,
  INSTALLVERSION_BUILD = -4
};

/***********************************************************************/
//
// InstallVersion Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetInstallVersionProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMInstallVersion *a = (nsIDOMInstallVersion*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case INSTALLVERSION_MAJOR:
      {
        PRInt32 prop;
        if (NS_OK == a->GetMajor(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case INSTALLVERSION_MINOR:
      {
        PRInt32 prop;
        if (NS_OK == a->GetMinor(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case INSTALLVERSION_RELEASE:
      {
        PRInt32 prop;
        if (NS_OK == a->GetRelease(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case INSTALLVERSION_BUILD:
      {
        PRInt32 prop;
        if (NS_OK == a->GetBuild(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
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
// InstallVersion Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetInstallVersionProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMInstallVersion *a = (nsIDOMInstallVersion*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case INSTALLVERSION_MAJOR:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetMajor(prop);
        
        break;
      }
      case INSTALLVERSION_MINOR:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetMinor(prop);
        
        break;
      }
      case INSTALLVERSION_RELEASE:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetRelease(prop);
        
        break;
      }
      case INSTALLVERSION_BUILD:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetBuild(prop);
        
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
// InstallVersion finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeInstallVersion(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// InstallVersion enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateInstallVersion(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// InstallVersion resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveInstallVersion(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method Init
//
PR_STATIC_CALLBACK(JSBool)
InstallVersionInit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallVersion *nativeThis = (nsIDOMInstallVersion*)JS_GetPrivate(cx, obj);
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc == 1) 
  {
      nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
  }
  else 
  {
      b0.AssignWithConversion("0.0.0.0");
  }
    
  if (NS_OK != nativeThis->Init(b0)) 
        return JS_FALSE;
  
  *rval = JSVAL_VOID;

  return JS_TRUE;
}


//
// Native method ToString
//
PR_STATIC_CALLBACK(JSBool)
InstallVersionToString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallVersion *nativeThis = (nsIDOMInstallVersion*)JS_GetPrivate(cx, obj);
  nsAutoString nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ToString(nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function toString requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method CompareTo
//
PR_STATIC_CALLBACK(JSBool)
InstallVersionCompareTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallVersion *nativeThis = (nsIDOMInstallVersion*)JS_GetPrivate(cx, obj);
  PRInt32                 nativeRet;
  nsString                b0str;
  PRInt32                 b0int;
  PRInt32                 b1int;
  PRInt32                 b2int;
  PRInt32                 b3int;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 4)
  {
    //  public int CompareTo(int    major,
    //                       int    minor,
    //                       int    release,
    //                       int    build);

    if(!JSVAL_IS_INT(argv[0]))
    {
        JS_ReportError(cx, "1st parameter must be a number");
        return JS_FALSE;
    }
    else if(!JSVAL_IS_INT(argv[1]))
    {
        JS_ReportError(cx, "2nd parameter must be a number");
        return JS_FALSE;
    }
    else if(!JSVAL_IS_INT(argv[2]))
    {
        JS_ReportError(cx, "3rd parameter must be a number");
        return JS_FALSE;
    }
    else if(!JSVAL_IS_INT(argv[3]))
    {
        JS_ReportError(cx, "4th parameter must be a number");
        return JS_FALSE;
    }

    b0int = JSVAL_TO_INT(argv[0]);
    b1int = JSVAL_TO_INT(argv[1]);
    b2int = JSVAL_TO_INT(argv[2]);
    b3int = JSVAL_TO_INT(argv[3]);

    if(NS_OK != nativeThis->CompareTo(b0int, b1int, b2int, b3int, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if(argc >= 1)
  {
     //   public int AddDirectory(String version);  --OR--  VersionInfo version

    if(JSVAL_IS_OBJECT(argv[0]))
    {
        nsCOMPtr<nsIDOMInstallVersion> versionObj;

        if(JS_FALSE == ConvertJSValToObj(getter_AddRefs(versionObj),
                                         kIInstallVersionIID,
                                         NS_ConvertASCIItoUCS2("InstallVersion"),
                                         cx,
                                         argv[0]))
        {
          return JS_FALSE;
        }

        if(NS_OK != nativeThis->CompareTo(versionObj, &nativeRet))
        {
          return JS_FALSE;
        }
    }
    else
    {
        ConvertJSValToStr(b0str, cx, argv[0]);

        if(NS_OK != nativeThis->CompareTo(b0str, &nativeRet))
        {
          return JS_FALSE;
        }
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function compareTo requires 4 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for InstallVersion
//
JSClass InstallVersionClass = {
  "InstallVersion", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetInstallVersionProperty,
  SetInstallVersionProperty,
  EnumerateInstallVersion,
  ResolveInstallVersion,
  JS_ConvertStub,
  FinalizeInstallVersion
};


//
// InstallVersion class properties
//
static JSPropertySpec InstallVersionProperties[] =
{
  {"major",    INSTALLVERSION_MAJOR,    JSPROP_ENUMERATE},
  {"minor",    INSTALLVERSION_MINOR,    JSPROP_ENUMERATE},
  {"release",  INSTALLVERSION_RELEASE,  JSPROP_ENUMERATE},
  {"build",    INSTALLVERSION_BUILD,    JSPROP_ENUMERATE},
  {0}
};


//
// InstallVersion class methods
//
static JSFunctionSpec InstallVersionMethods[] = 
{
  {"init",          InstallVersionInit,       1},
  {"toString",      InstallVersionToString,   0},
  {"compareTo",     InstallVersionCompareTo,  1},
  {0}
};

static JSConstDoubleSpec version_constants[] = 
{
    { nsIDOMInstallVersion::EQUAL,                  "EQUAL"              },
    { nsIDOMInstallVersion::BLD_DIFF,               "BLD_DIFF"           },
    { nsIDOMInstallVersion::BLD_DIFF_MINUS,         "BLD_DIFF_MINUS"     },
    { nsIDOMInstallVersion::REL_DIFF,               "REL_DIFF"           },
    { nsIDOMInstallVersion::REL_DIFF_MINUS,         "REL_DIFF_MINUS"     },
    { nsIDOMInstallVersion::MINOR_DIFF,             "MINOR_DIFF"         },
    { nsIDOMInstallVersion::MINOR_DIFF_MINUS,       "MINOR_DIFF_MINUS"   },
    { nsIDOMInstallVersion::MAJOR_DIFF,             "MAJOR_DIFF"         },
    { nsIDOMInstallVersion::MAJOR_DIFF_MINUS,       "MAJOR_DIFF_MINUS"   },
    {0}
};



//
// InstallVersion constructor
//
PR_STATIC_CALLBACK(JSBool)
InstallVersion(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  
  nsIDOMInstallVersion *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;

  static NS_DEFINE_IID(kIDOMInstallVersionIID, NS_IDOMINSTALLVERSION_IID);
  static NS_DEFINE_IID(kInstallVersion_CID, NS_SoftwareUpdateInstallVersion_CID);

  result = nsRepository::CreateInstance(kInstallVersion_CID,
                                        nsnull,
                                        kIDOMInstallVersionIID,
                                        (void **)&nativeThis);
  if (NS_OK != result) return JS_FALSE;

            
  result = nativeThis->QueryInterface(kIScriptObjectOwnerIID, (void **)&owner);
  if (NS_OK != result) {
    NS_RELEASE(nativeThis);
    return JS_FALSE;
  }

  owner->SetScriptObject((void *)obj);
  JS_SetPrivate(cx, obj, nativeThis);

  NS_RELEASE(owner);
   
  jsval ignore;
  InstallVersionInit(cx, obj, argc, argv, &ignore);

  return JS_TRUE;
}

nsresult InitInstallVersionClass(JSContext *jscontext, JSObject *global, void** prototype)
{
  JSObject *proto = nsnull;

  if (prototype != nsnull)
    *prototype = nsnull;

  proto = JS_InitClass(jscontext,     // context
                       global,        // global object
                       nsnull,  // parent proto 
                       &InstallVersionClass,      // JSClass
                       InstallVersion,            // JSNative ctor
                       0,             // ctor args
                       InstallVersionProperties,  // proto props
                       InstallVersionMethods,     // proto funcs
                       nsnull,        // ctor props (static)
                       nsnull);       // ctor funcs (static)
  
  if (nsnull == proto)
      return NS_ERROR_FAILURE;
  
  
  if ( PR_FALSE == JS_DefineConstDoubles(jscontext, proto, version_constants) )
            return NS_ERROR_FAILURE;
  
  if (prototype != nsnull)
      *prototype = proto;
  
  return NS_OK;
}

//
// InstallVersion class initialization
//
nsresult NS_InitInstallVersionClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "InstallVersion", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) 
  {
    nsresult rv = InitInstallVersionClass(jscontext, global, (void**)&proto);
    if (NS_FAILED(rv)) return rv;
  }
  else if ((nsnull != constructor) && JSVAL_IS_OBJECT(vp)) 
  {
    proto = JSVAL_TO_OBJECT(vp);
  }
  else 
  {
    return NS_ERROR_FAILURE;
  }

  if (aPrototype) 
    *aPrototype = proto;
  
  return NS_OK;
}


//
// Method for creating a new InstallVersion JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptInstallVersion(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptInstallVersion");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMInstallVersion *aInstallVersion;

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

  if (NS_OK != NS_InitInstallVersionClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIInstallVersionIID, (void **)&aInstallVersion);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &InstallVersionClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aInstallVersion);
  }
  else {
    NS_RELEASE(aInstallVersion);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
