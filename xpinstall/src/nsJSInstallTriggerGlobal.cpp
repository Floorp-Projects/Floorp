/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jsapi.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIDOMInstallVersion.h"
#include "nsIDOMInstallTriggerGlobal.h"
#include "nsInstallTrigger.h"
#include "nsXPITriggerInfo.h"

#include "nsIComponentManager.h"

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

//
// InstallTriggerGlobal finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeInstallTriggerGlobal(JSContext *cx, JSObject *obj)
{
  nsISupports *nativeThis = (nsISupports*)JS_GetPrivate(cx, obj);

  if (nsnull != nativeThis) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == nativeThis->QueryInterface(NS_GET_IID(nsIScriptObjectOwner), 
                                            (void**)&owner)) {
      owner->SetScriptObject(nsnull);
      NS_RELEASE(owner);
    }
    
    // The addref was part of JSObject construction
    NS_RELEASE(nativeThis);
  }
}

static JSBool CreateNativeObject(JSContext *cx, JSObject *obj, nsIDOMInstallTriggerGlobal **aResult)
{
    nsresult result;
    nsIScriptObjectOwner *owner = nsnull;
    nsIDOMInstallTriggerGlobal *nativeThis;

    static NS_DEFINE_CID(kInstallTrigger_CID,
                         NS_SoftwareUpdateInstallTrigger_CID);

    result = nsComponentManager::CreateInstance(kInstallTrigger_CID,
                                        nsnull,
                                        NS_GET_IID(nsIDOMInstallTriggerGlobal),
                                        (void **)&nativeThis);

    if (NS_OK != result) return JS_FALSE;
    
    result = nativeThis->QueryInterface(NS_GET_IID(nsIScriptObjectOwner),
                                        (void **)&owner);

    if (NS_OK != result) 
    {
        NS_RELEASE(nativeThis);
        return JS_FALSE;
    }

    owner->SetScriptObject((void *)obj);
    JS_SetPrivate(cx, obj, nativeThis);
    
    *aResult = nativeThis;
    
    NS_RELEASE(nativeThis);  // we only want one refcnt. JSUtils cleans us up.
    return JS_TRUE;
}

//
// Native method UpdateEnabled
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobalUpdateEnabled(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);
  PRBool nativeRet;

  *rval = JSVAL_NULL;

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) )
    return JS_FALSE;

  if (argc >= 0) {

    if (NS_OK != nativeThis->UpdateEnabled(&nativeRet)) {
      return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function UpdateEnabled requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}

//
// Native method Install
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobalInstall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);

  *rval = JSVAL_FALSE;

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) )
    return JS_FALSE;


  // make sure XPInstall is enabled, return false if not
  PRBool enabled = PR_FALSE;
  nativeThis->UpdateEnabled(&enabled);
  if (!enabled)
      return JS_TRUE;


  // get window.location to construct relative URLs
  nsString baseURL;
  JSObject* global = JS_GetGlobalObject(cx);
  if (global)
  {
    jsval v;
    if (JS_GetProperty(cx,global,"location",&v))
    {
      ConvertJSValToStr( baseURL, cx, v );
      PRInt32 lastslash = baseURL.RFindChar('/');
      if (lastslash != kNotFound)
      {
        baseURL.Truncate(lastslash+1);
      }
    }
  }

  
  // parse associative array of installs
  if ( argc >= 1 && JSVAL_IS_OBJECT(argv[0]) )
  {
    nsXPITriggerInfo *trigger = new nsXPITriggerInfo();
    if (!trigger)
      return JS_FALSE;

    JSIdArray *ida = JS_Enumerate( cx, JSVAL_TO_OBJECT(argv[0]) );
    if ( ida ) 
    {
      jsval v;
      const PRUnichar *name, *URL;

      for (int i = 0; i < ida->length; i++ )
      {
        JS_IdToValue( cx, ida->vector[i], &v );
        name = NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars( JS_ValueToString( cx, v ) ));

        JS_GetUCProperty( cx, JSVAL_TO_OBJECT(argv[0]), NS_REINTERPRET_CAST(const jschar*, name), nsCRT::strlen(name), &v );
        URL = NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars( JS_ValueToString( cx, v ) ));

        if ( name && URL )
        {
            nsXPITriggerItem *item = new nsXPITriggerItem( name, URL );
            if ( item )
            {
                if ( item->IsRelativeURL() )
                {
                  item->mURL.Insert( baseURL, 0 );
                }
                trigger->Add( item );
            }
            else
                ; // XXX signal error somehow
        }
        else
            ; // XXX need to signal error
      }
      JS_DestroyIdArray( cx, ida );
    }


    // save callback function if any (ignore bad args for now)
    if ( argc >= 2 && JS_TypeOfValue(cx,argv[1]) == JSTYPE_FUNCTION )
    {
      trigger->SaveCallback( cx, argv[1] );
    }


    // pass on only if good stuff found
    if (trigger->Size() > 0)
    {
        PRBool result;

        nsCOMPtr<nsIScriptContext> scriptContext = (nsIScriptContext*) JS_GetContextPrivate(cx);
        if (scriptContext)
        {
            nsCOMPtr<nsIScriptGlobalObject> globalObject;
            scriptContext->GetGlobalObject(getter_AddRefs(globalObject));
            if (globalObject)
            {
                nativeThis->Install(globalObject, trigger,&result);
                *rval = BOOLEAN_TO_JSVAL(result);
                return JS_TRUE;
            }
        }
    }
    else
        delete trigger;
  }

  JS_ReportError(cx, "Incorrect arguments to InstallTrigger.Install()");
  return JS_FALSE;
}


//
// Native method InstallChrome
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobalInstallChrome(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);
  PRBool       nativeRet;
  uint32       chromeType;
  nsAutoString baseURL;
  nsAutoString sourceURL;
  nsAutoString name;

  *rval = JSVAL_FALSE;

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) ) {
    return JS_FALSE;
  }


  // make sure XPInstall is enabled, return if not
  PRBool enabled = PR_FALSE;
  nativeThis->UpdateEnabled(&enabled);
  if (!enabled)
      return JS_TRUE;


  // get window.location to construct relative URLs
  JSObject* global = JS_GetGlobalObject(cx);
  if (global)
  {
    jsval v;
    if (JS_GetProperty(cx,global,"location",&v))
    {
      ConvertJSValToStr( baseURL, cx, v );
      PRInt32 lastslash = baseURL.RFindChar('/');
      if (lastslash != kNotFound)
      {
        baseURL.Truncate(lastslash+1);
      }
    }
  }

  
  if ( argc >= 3 )
  {
    JS_ValueToECMAUint32(cx, argv[0], &chromeType);
    ConvertJSValToStr(sourceURL, cx, argv[1]);
    ConvertJSValToStr(name, cx, argv[2]);

    if ( chromeType & CHROME_ALL )
    {
        // there's at least one known chrome type
        nsXPITriggerItem* item = new nsXPITriggerItem(name.get(),
                                                      sourceURL.get());

        if (item && item->IsRelativeURL())
            item->mURL.Insert( baseURL, 0 );

        nsCOMPtr<nsIScriptContext> scriptContext = (nsIScriptContext*) JS_GetContextPrivate(cx);
        if (scriptContext)
        {
            nsCOMPtr<nsIScriptGlobalObject> globalObject;
            scriptContext->GetGlobalObject(getter_AddRefs(globalObject));
            if (globalObject)
            {
                nsresult rv = nativeThis->InstallChrome(globalObject, chromeType, item, &nativeRet);
                if (NS_FAILED(rv))
                    return JS_FALSE;
            }
            else
                return JS_FALSE;
        }
        else
            return JS_FALSE;

        *rval = BOOLEAN_TO_JSVAL(nativeRet);
    }
  }
  return JS_TRUE;
}


//
// Native method StartSoftwareUpdate
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobalStartSoftwareUpdate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);
  PRBool       nativeRet;
  nsAutoString b0;
  PRInt32      b1 = 0;

  *rval = JSVAL_FALSE;

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) )
    return JS_FALSE;

  if ( argc >= 1 )
  {
    ConvertJSValToStr(b0, cx, argv[0]);

    if (argc >= 2 && !JS_ValueToInt32(cx, argv[1], (int32 *)&b1))
    {
        JS_ReportError(cx, "StartSoftwareUpdate() 2nd parameter must be a number");
        return JS_FALSE;
    }

    nsCOMPtr<nsIScriptContext> scriptContext = (nsIScriptContext*) JS_GetContextPrivate(cx);
    if (scriptContext)
    {
        nsCOMPtr<nsIScriptGlobalObject> globalObject;
        scriptContext->GetGlobalObject(getter_AddRefs(globalObject));
        if (globalObject)
        {
             if(NS_OK != nativeThis->StartSoftwareUpdate(globalObject, b0, b1, &nativeRet))
             {
                  return JS_FALSE;
             }
        }
        else
           return JS_FALSE;
    }
    else
        return JS_FALSE;

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function StartSoftwareUpdate requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method CompareVersion
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobalCompareVersion(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  nsAutoString regname;
  nsAutoString version;
  int32        major,minor,release,build;

  *rval = JSVAL_NULL;

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) )
    return JS_FALSE;

  if (argc < 2 )
  {
    JS_ReportError(cx, "CompareVersion requires at least 2 parameters");
    return JS_FALSE;
  }
  else if ( !JSVAL_IS_STRING(argv[0]) )
  {
    JS_ReportError(cx, "Invalid parameter passed to CompareVersion");
    return JS_FALSE;
  }

  // get the registry name argument
  ConvertJSValToStr(regname, cx, argv[0]);

  if (argc == 2 )
  {
    //  public int CompareVersion(String registryName, String version)
    //  --OR-- CompareVersion(String registryNamve, InstallVersion version)

    ConvertJSValToStr(version, cx, argv[1]);
    if(NS_OK != nativeThis->CompareVersion(regname, version, &nativeRet))
    {
          return JS_FALSE;
    }
  }
  else
  {
    //  public int CompareVersion(String registryName,
    //                            int    major,
    //                            int    minor,
    //                            int    release,
    //                            int    build);
    //
    //  minor, release, and build values are optional

    major = minor = release = build = 0;

    if(!JS_ValueToInt32(cx, argv[1], &major))
    {
      JS_ReportError(cx, "2th parameter must be a number");
      return JS_FALSE;
    }
    if( argc > 2 && !JS_ValueToInt32(cx, argv[2], &minor) )
    {
      JS_ReportError(cx, "3th parameter must be a number");
      return JS_FALSE;
    }
    if( argc > 3 && !JS_ValueToInt32(cx, argv[3], &release) )
    {
      JS_ReportError(cx, "4th parameter must be a number");
      return JS_FALSE;
    }
    if( argc > 4 && !JS_ValueToInt32(cx, argv[4], &build) )
    {
      JS_ReportError(cx, "5th parameter must be a number");
      return JS_FALSE;
    }

    if(NS_OK != nativeThis->CompareVersion(regname, major, minor, release, build, &nativeRet))
    {
      return JS_FALSE;
    }
  }

  *rval = INT_TO_JSVAL(nativeRet);
  return JS_TRUE;
}

//
// Native method GetVersion
//
PR_STATIC_CALLBACK(JSBool)
InstallTriggerGlobalGetVersion(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);
  
  nsAutoString regname;
  nsAutoString version;

  *rval = JSVAL_NULL;

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) )
    return JS_FALSE;

  // get the registry name argument
  ConvertJSValToStr(regname, cx, argv[0]);

  if(NS_OK != nativeThis->GetVersion(regname, version))
  {
        return JS_FALSE;
  }
  
  if(version.IsEmpty())
      *rval = JSVAL_NULL;
    else
      ConvertStrToJSVal(version, cx, rval);

  return JS_TRUE;
}

/***********************************************************************/
//
// class for InstallTriggerGlobal
//
JSClass InstallTriggerGlobalClass = {
  "InstallTrigger", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub,
  FinalizeInstallTriggerGlobal
};

//
// InstallTriggerGlobal class methods
//
static JSFunctionSpec InstallTriggerGlobalMethods[] = 
{
  // -- obsolete forms, do not document. Kept for 4.x compatibility
  {"UpdateEnabled",             InstallTriggerGlobalUpdateEnabled,             0},
  {"StartSoftwareUpdate",       InstallTriggerGlobalStartSoftwareUpdate,       2},
  {"CompareVersion",            InstallTriggerGlobalCompareVersion,            5},
  {"GetVersion",                InstallTriggerGlobalGetVersion,                2},
  {"updateEnabled",             InstallTriggerGlobalUpdateEnabled,             0},
  // -- new forms to match JS style --
  {"enabled",                   InstallTriggerGlobalUpdateEnabled,             0},
  {"install",                   InstallTriggerGlobalInstall,                   2},
  {"installChrome",             InstallTriggerGlobalInstallChrome,             2},
  {"startSoftwareUpdate",       InstallTriggerGlobalStartSoftwareUpdate,       2},
  {"compareVersion",            InstallTriggerGlobalCompareVersion,            5},
  {"getVersion",                InstallTriggerGlobalGetVersion,                2},
  {0}
};


static JSConstDoubleSpec diff_constants[] = 
{
    { nsIDOMInstallTriggerGlobal::MAJOR_DIFF,    "MAJOR_DIFF" },
    { nsIDOMInstallTriggerGlobal::MINOR_DIFF,    "MINOR_DIFF" },
    { nsIDOMInstallTriggerGlobal::REL_DIFF,      "REL_DIFF"   },
    { nsIDOMInstallTriggerGlobal::BLD_DIFF,      "BLD_DIFF"   },
    { nsIDOMInstallTriggerGlobal::EQUAL,         "EQUAL"      },
    { CHROME_SKIN,                               "SKIN"       },
    { CHROME_LOCALE,                             "LOCALE"     },
    { CHROME_CONTENT,                            "CONTENT"    },
    { CHROME_ALL,                                "PACKAGE"    },
    {0}
};



nsresult InitInstallTriggerGlobalClass(JSContext *jscontext, JSObject *global, void** prototype)
{
  JSObject *proto = nsnull;
  
  if (prototype != nsnull)
    *prototype = nsnull;

    proto = JS_InitClass(jscontext,                       // context
                         global,                          // global object
                         nsnull,                          // parent proto 
                         &InstallTriggerGlobalClass,      // JSClass
                         nsnull,                          // JSNative ctor
                         nsnull,                          // ctor args
                         nsnull,                          // proto props
                         nsnull,                          // proto funcs
                         nsnull,                          // ctor props (static)
                         InstallTriggerGlobalMethods);    // ctor funcs (static)
    
    
    if (nsnull == proto) return NS_ERROR_FAILURE;
    
    if ( PR_FALSE == JS_DefineConstDoubles(jscontext, proto, diff_constants) )
            return NS_ERROR_FAILURE;

    if (prototype != nsnull)
      *prototype = proto;
  
  return NS_OK;
}



//
// InstallTriggerGlobal class initialization
//
nsresult NS_InitInstallTriggerGlobalClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "InstallTriggerGlobal", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) 
  {
    nsresult rv = InitInstallTriggerGlobalClass(jscontext, global, (void**)&proto);
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
// Method for creating a new InstallTriggerGlobal JavaScript object
//
nsresult
NS_NewScriptInstallTriggerGlobal(nsIScriptContext *aContext,
                                 nsISupports *aSupports, nsISupports *aParent,
                                 void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports &&
                  nsnull != aReturn,
                  "null argument to NS_NewScriptInstallTriggerGlobal");

  JSObject *proto;
  JSObject *parent = nsnull;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMInstallTriggerGlobal *installTriggerGlobal;

  nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(aParent));

  if (owner) {
    if (NS_OK != owner->GetScriptObject(aContext, (void **)&parent)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aParent));

    if (sgo) {
      parent = sgo->GetGlobalJSObject();
    } else {
      return NS_ERROR_FAILURE;
    }
  }

  if (NS_OK != NS_InitInstallTriggerGlobalClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(NS_GET_IID(nsIDOMInstallTriggerGlobal),
                                     (void **)&installTriggerGlobal);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &InstallTriggerGlobalClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, installTriggerGlobal);
  }
  else {
    NS_RELEASE(installTriggerGlobal);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
