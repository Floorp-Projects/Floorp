/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"

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

  *rval = JSVAL_FALSE;

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) )
    return JS_TRUE;

  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (scriptContext)
    globalObject = scriptContext->GetGlobalObject();

  PRBool nativeRet = PR_FALSE;
  if (globalObject)
    nativeThis->UpdateEnabled(globalObject, XPI_GLOBAL, &nativeRet);

  *rval = BOOLEAN_TO_JSVAL(nativeRet);
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
    return JS_TRUE;


  // make sure XPInstall is enabled, return false if not
  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (scriptContext)
      globalObject = scriptContext->GetGlobalObject();

  PRBool enabled = PR_FALSE;
  nativeThis->UpdateEnabled(globalObject, XPI_WHITELIST, &enabled);
  if (!enabled || !globalObject)
      return JS_TRUE;


  // get window.location to construct relative URLs
  nsCOMPtr<nsIURI> baseURL;
  JSObject* global = JS_GetGlobalObject(cx);
  if (global)
  {
    jsval v;
    if (JS_GetProperty(cx,global,"location",&v))
    {
      nsAutoString location;
      ConvertJSValToStr( location, cx, v );
      NS_NewURI(getter_AddRefs(baseURL), location);
    }
  }

  // if we can't create a security manager we might be in the wizard, allow
  PRBool abortLoad = PR_FALSE;
  nsCOMPtr<nsIScriptSecurityManager> secman(
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));


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
      const PRUnichar *iconURL = nsnull;

      for (int i = 0; i < ida->length && !abortLoad; i++ )
      {
        JS_IdToValue( cx, ida->vector[i], &v );
        name = NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars( JS_ValueToString( cx, v ) ));

        URL = iconURL = nsnull;
        JS_GetUCProperty( cx, JSVAL_TO_OBJECT(argv[0]), NS_REINTERPRET_CAST(const jschar*, name), nsCRT::strlen(name), &v );
        if ( JSVAL_IS_OBJECT(v) ) 
        {
          jsval v2;
          if (JS_GetProperty( cx, JSVAL_TO_OBJECT(v), "URL", &v2 ))
            URL = NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars( JS_ValueToString( cx, v2 ) ));

          if (JS_GetProperty( cx, JSVAL_TO_OBJECT(v), "IconURL", &v2 ))
            iconURL = NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars( JS_ValueToString( cx, v2 ) ));
        }
        else
        {
          URL = NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars( JS_ValueToString( cx, v ) ));
        }

        if ( name && URL )
        {
            // Get relative URL to load
            nsAutoString xpiURL(URL);
            if (baseURL)
            {
                nsCAutoString resolvedURL;
                baseURL->Resolve(NS_ConvertUTF16toUTF8(xpiURL), resolvedURL);
                xpiURL = NS_ConvertUTF8toUTF16(resolvedURL);
            }

            // Make sure we're allowed to load this URL
            if (secman)
            {
                nsCOMPtr<nsIURI> uri;
                nsresult rv = NS_NewURI(getter_AddRefs(uri), xpiURL);
                if (NS_SUCCEEDED(rv))
                {
                    rv = secman->CheckLoadURIFromScript(cx, uri);
                    if (NS_FAILED(rv))
                        abortLoad = PR_TRUE;
                }
            }

            nsAutoString icon(iconURL);
            if (iconURL && baseURL)
            {
                nsCAutoString resolvedIcon;
                baseURL->Resolve(NS_ConvertUTF16toUTF8(icon), resolvedIcon);
                icon = NS_ConvertUTF8toUTF16(resolvedIcon);
            }

            nsXPITriggerItem *item = new nsXPITriggerItem( name, xpiURL.get(), icon.get() );
            if ( item )
            {
                trigger->Add( item );
            }
            else
                abortLoad = PR_TRUE;
        }
        else
            abortLoad = PR_TRUE;
      }
      JS_DestroyIdArray( cx, ida );
    }


    // save callback function if any (ignore bad args for now)
    if ( argc >= 2 && JS_TypeOfValue(cx,argv[1]) == JSTYPE_FUNCTION )
    {
        trigger->SaveCallback( cx, argv[1] );
    }


    // pass on only if good stuff found
    if (!abortLoad && trigger->Size() > 0)
    {
        PRBool result;
        nativeThis->Install(globalObject, trigger, &result);
        *rval = BOOLEAN_TO_JSVAL(result);
        return JS_TRUE;
    }
    // didn't pass it on so we must delete trigger
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
  uint32       chromeType = NOT_CHROME;
  nsAutoString sourceURL;
  nsAutoString name;

  *rval = JSVAL_FALSE;

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) ) {
    return JS_TRUE;
  }

  // get chromeType first, the update enabled check for skins skips whitelisting
  if (argc >=1)
      JS_ValueToECMAUint32(cx, argv[0], &chromeType);

  // make sure XPInstall is enabled, return if not
  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (scriptContext)
      globalObject = scriptContext->GetGlobalObject();

  PRBool enabled = PR_FALSE;
  PRBool useWhitelist = ( chromeType != CHROME_SKIN );
  nativeThis->UpdateEnabled(globalObject, useWhitelist, &enabled);
  if (!enabled || !globalObject)
      return JS_TRUE;


  // get window.location to construct relative URLs
  nsCOMPtr<nsIURI> baseURL;
  JSObject* global = JS_GetGlobalObject(cx);
  if (global)
  {
    jsval v;
    if (JS_GetProperty(cx,global,"location",&v))
    {
      nsAutoString location;
      ConvertJSValToStr( location, cx, v );
      NS_NewURI(getter_AddRefs(baseURL), location);
    }
  }


  if ( argc >= 3 )
  {
    ConvertJSValToStr(sourceURL, cx, argv[1]);
    ConvertJSValToStr(name, cx, argv[2]);

    if (baseURL)
    {
        nsCAutoString resolvedURL;
        baseURL->Resolve(NS_ConvertUTF16toUTF8(sourceURL), resolvedURL);
        sourceURL = NS_ConvertUTF8toUTF16(resolvedURL);
    }

    // Make sure caller is allowed to load this url.
    // if we can't create a security manager we might be in the wizard, allow
    nsCOMPtr<nsIScriptSecurityManager> secman(
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
    if (secman)
    {
        nsCOMPtr<nsIURI> uri;
        nsresult rv = NS_NewURI(getter_AddRefs(uri), sourceURL);
        if (NS_SUCCEEDED(rv))
        {
            rv = secman->CheckLoadURIFromScript(cx, uri);
            if (NS_FAILED(rv))
                return JS_FALSE;
        }
    }

    if ( chromeType & CHROME_ALL )
    {
        // there's at least one known chrome type
        nsXPITriggerItem* item = new nsXPITriggerItem(name.get(),
                                                      sourceURL.get(), 
                                                      nsnull);

        PRBool nativeRet = PR_FALSE;
        nativeThis->InstallChrome(globalObject, chromeType, item, &nativeRet);
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
  PRInt32      flags = 0;

  *rval = JSVAL_FALSE;

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) )
    return JS_TRUE;

  // make sure XPInstall is enabled, return if not
  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (scriptContext)
      globalObject = scriptContext->GetGlobalObject();

  PRBool enabled = PR_FALSE;
  nativeThis->UpdateEnabled(globalObject, XPI_WHITELIST, &enabled);
  if (!enabled || !globalObject)
      return JS_TRUE;

  // get window.location to construct relative URLs
  nsCOMPtr<nsIURI> baseURL;
  JSObject* global = JS_GetGlobalObject(cx);
  if (global)
  {
    jsval v;
    if (JS_GetProperty(cx,global,"location",&v))
    {
      nsAutoString location;
      ConvertJSValToStr( location, cx, v );
      NS_NewURI(getter_AddRefs(baseURL), location);
    }
  }

  
  if ( argc >= 1 )
  {
    nsAutoString xpiURL;
    ConvertJSValToStr(xpiURL, cx, argv[0]);
    if (baseURL)
    {
        nsCAutoString resolvedURL;
        baseURL->Resolve(NS_ConvertUTF16toUTF8(xpiURL), resolvedURL);
        xpiURL = NS_ConvertUTF8toUTF16(resolvedURL);
    }

    // Make sure caller is allowed to load this url.
    // if we can't create a security manager we might be in the wizard, allow
    nsCOMPtr<nsIScriptSecurityManager> secman(
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
    if (secman)
    {
        nsCOMPtr<nsIURI> uri;
        nsresult rv = NS_NewURI(getter_AddRefs(uri), xpiURL);
        if (NS_SUCCEEDED(rv))
        {
            rv = secman->CheckLoadURIFromScript(cx, uri);
            if (NS_FAILED(rv))
                return JS_FALSE;
        }
    }

    if (argc >= 2 && !JS_ValueToInt32(cx, argv[1], (int32 *)&flags))
    {
        JS_ReportError(cx, "StartSoftwareUpdate() 2nd parameter must be a number");
        return JS_FALSE;
    }

    if(NS_OK == nativeThis->StartSoftwareUpdate(globalObject, xpiURL, flags, &nativeRet))
    {
        *rval = BOOLEAN_TO_JSVAL(nativeRet);
    }
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
  nsAutoString regname;
  nsAutoString version;
  int32        major,minor,release,build;

  // In case of error or disabled return NOT_FOUND
  PRInt32 nativeRet = nsIDOMInstallTriggerGlobal::NOT_FOUND;
  *rval = INT_TO_JSVAL(nativeRet);

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) )
    return JS_TRUE;


  // make sure XPInstall is enabled, return if not
  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (scriptContext)
      globalObject = scriptContext->GetGlobalObject();

  PRBool enabled = PR_FALSE;
  nativeThis->UpdateEnabled(globalObject, XPI_WHITELIST, &enabled);
  if (!enabled)
      return JS_TRUE;


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

  // In case of error return a null value
  *rval = JSVAL_NULL;

  if (nsnull == nativeThis  &&  (JS_FALSE == CreateNativeObject(cx, obj, &nativeThis)) )
      return JS_TRUE;


  // make sure XPInstall is enabled, return if not
  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (scriptContext)
      globalObject = scriptContext->GetGlobalObject();

  PRBool enabled = PR_FALSE;
  nativeThis->UpdateEnabled(globalObject, XPI_WHITELIST, &enabled);
  if (!enabled)
      return JS_TRUE;


  // get the registry name argument
  ConvertJSValToStr(regname, cx, argv[0]);

  if(nativeThis->GetVersion(regname, version) == NS_OK && !version.IsEmpty() )
  {
      ConvertStrToJSVal(version, cx, rval);
  }

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
    { nsIDOMInstallTriggerGlobal::NOT_FOUND,     "NOT_FOUND"  },
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

  result = CallQueryInterface(aSupports, &installTriggerGlobal);
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

