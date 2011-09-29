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
 *   Dave Townsend <dtownsend@oxymoronical.com>
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
#include "nsAutoPtr.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIDOMInstallTriggerGlobal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIObserverService.h"
#include "nsInstallTrigger.h"
#include "nsXPITriggerInfo.h"
#include "nsDOMJSUtils.h"
#include "nsXPIInstallInfo.h"

#include "nsIComponentManager.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"

#include "nsSoftwareUpdateIIDs.h"

void ConvertJSValToStr(nsString&  aString,
                      JSContext* aContext,
                      jsval      aValue)
{
  JSString *jsstring;

  if ( !JSVAL_IS_NULL(aValue) &&
       (jsstring = JS_ValueToString(aContext, aValue)) != nsnull)
  {
    aString.Assign(reinterpret_cast<const PRUnichar*>(JS_GetStringChars(jsstring)));
  }
  else
  {
    aString.Truncate();
  }
}

static void
FinalizeInstallTriggerGlobal(JSContext *cx, JSObject *obj);

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
// InstallTriggerGlobal finalizer
//
static void
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

    result = CallCreateInstance(kInstallTrigger_CID, &nativeThis);
    if (NS_FAILED(result)) return JS_FALSE;

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
// Helper function for URI verification
//
static nsresult
InstallTriggerCheckLoadURIFromScript(JSContext *cx, const nsAString& uriStr)
{
    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> secman(
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID,&rv));
    NS_ENSURE_SUCCESS(rv, rv);

    // get the script principal
    nsCOMPtr<nsIPrincipal> principal;
    rv = secman->GetSubjectPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!principal)
        return NS_ERROR_FAILURE;

    // convert the requested URL string to a URI
    // Note that we use a null base URI here, since that's what we use when we
    // actually convert the string into a URI to load.
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), uriStr);
    NS_ENSURE_SUCCESS(rv, rv);

    // are we allowed to load this one?
    rv = secman->CheckLoadURIWithPrincipal(principal, uri,
                    nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL);
    return rv;
}

//
// Helper function to get native object
//
// This is our own version of JS_GetInstancePrivate() that in addition
// performs the delayed creation of the native InstallTrigger if necessary
//
static nsIDOMInstallTriggerGlobal* getTriggerNative(JSContext *cx, JSObject *obj)
{
  if (!JS_InstanceOf(cx, obj, &InstallTriggerGlobalClass, nsnull))
    return nsnull;

  nsIDOMInstallTriggerGlobal *native = (nsIDOMInstallTriggerGlobal*)JS_GetPrivate(cx, obj);
  if (!native) {
    // xpinstall script contexts delay creation of the native.
    CreateNativeObject(cx, obj, &native);
  }
  return native;
}

//
// Native method UpdateEnabled
//
static JSBool
InstallTriggerGlobalUpdateEnabled(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = getTriggerNative(cx, obj);
  if (!nativeThis)
    return JS_FALSE;

  *rval = JSVAL_FALSE;

  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (scriptContext)
    globalObject = scriptContext->GetGlobalObject();

  bool nativeRet = false;
  if (globalObject)
    nativeThis->UpdateEnabled(globalObject, XPI_GLOBAL, &nativeRet);

  *rval = BOOLEAN_TO_JSVAL(nativeRet);
  return JS_TRUE;
}


//
// Native method Install
//
static JSBool
InstallTriggerGlobalInstall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = getTriggerNative(cx, obj);
  if (!nativeThis)
    return JS_FALSE;

  *rval = JSVAL_FALSE;

  // make sure XPInstall is enabled, return false if not
  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (scriptContext)
    globalObject = scriptContext->GetGlobalObject();

  if (!globalObject)
      return JS_TRUE;

  nsCOMPtr<nsIScriptSecurityManager> secman(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
  if (!secman)
  {
    JS_ReportError(cx, "Could not the script security manager service.");
    return JS_FALSE;
  }
  // get the principal.  if it doesn't exist, die.
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = secman->GetSubjectPrincipal(getter_AddRefs(principal));

  if (NS_FAILED(rv) || !principal)
  {
    JS_ReportError(cx, "Could not get the Subject Principal during InstallTrigger.Install()");
    return JS_FALSE;
  }

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

  bool abortLoad = false;

  // parse associative array of installs
  if ( argc >= 1 && JSVAL_IS_OBJECT(argv[0]) && JSVAL_TO_OBJECT(argv[0]) )
  {
    nsXPITriggerInfo *trigger = new nsXPITriggerInfo();
    if (!trigger)
      return JS_FALSE;

    trigger->SetPrincipal(principal);

    JSIdArray *ida = JS_Enumerate( cx, JSVAL_TO_OBJECT(argv[0]) );
    if ( ida )
    {
      jsval v;
      const PRUnichar *name, *URL;
      const PRUnichar *iconURL = nsnull;

      for (int i = 0; i < ida->length && !abortLoad; i++ )
      {
        JS_IdToValue( cx, ida->vector[i], &v );
        JSString * str = JS_ValueToString( cx, v );
        if (!str)
        {
          abortLoad = PR_TRUE;
          break;
        }

        name = reinterpret_cast<const PRUnichar*>(JS_GetStringChars( str ));

        URL = iconURL = nsnull;
        JSAutoByteString hash;
        JS_GetUCProperty( cx, JSVAL_TO_OBJECT(argv[0]), reinterpret_cast<const jschar*>(name), nsCRT::strlen(name), &v );
        if ( JSVAL_IS_OBJECT(v) && JSVAL_TO_OBJECT(v) )
        {
          jsval v2;
          if (JS_GetProperty( cx, JSVAL_TO_OBJECT(v), "URL", &v2 ) && !JSVAL_IS_VOID(v2)) {
            JSString *str = JS_ValueToString(cx, v2);
            if (!str) {
              abortLoad = PR_TRUE;
              break;
            }
            URL = reinterpret_cast<const PRUnichar*>(JS_GetStringChars(str));
          }

          if (JS_GetProperty( cx, JSVAL_TO_OBJECT(v), "IconURL", &v2 ) && !JSVAL_IS_VOID(v2)) {
            JSString *str = JS_ValueToString(cx, v2);
            if (!str) {
              abortLoad = PR_TRUE;
              break;
            }
            iconURL = reinterpret_cast<const PRUnichar*>(JS_GetStringChars(str));
          }

          if (JS_GetProperty( cx, JSVAL_TO_OBJECT(v), "Hash", &v2) && !JSVAL_IS_VOID(v2)) {
            JSString *str = JS_ValueToString(cx, v2);
            if (!str || !hash.encode(cx, str)) {
              abortLoad = PR_TRUE;
              break;
            }
          }
        }
        else
        {
          JSString *str = JS_ValueToString(cx, v);
          if (!str) {
            abortLoad = PR_TRUE;
            break;
          }
          URL = reinterpret_cast<const PRUnichar*>(JS_GetStringChars(str));
        }

        if ( URL )
        {
            // Get relative URL to load
            nsAutoString xpiURL(URL);
            if (baseURL)
            {
                nsCAutoString resolvedURL;
                baseURL->Resolve(NS_ConvertUTF16toUTF8(xpiURL), resolvedURL);
                xpiURL = NS_ConvertUTF8toUTF16(resolvedURL);
            }

            nsAutoString icon(iconURL);
            if (iconURL && baseURL)
            {
                nsCAutoString resolvedIcon;
                baseURL->Resolve(NS_ConvertUTF16toUTF8(icon), resolvedIcon);
                icon = NS_ConvertUTF8toUTF16(resolvedIcon);
            }

            // Make sure we're allowed to load this URL and the icon URL
            rv = InstallTriggerCheckLoadURIFromScript(cx, xpiURL);
            if (NS_FAILED(rv))
                abortLoad = PR_TRUE;

            if (!abortLoad && iconURL)
            {
                rv = InstallTriggerCheckLoadURIFromScript(cx, icon);
                if (NS_FAILED(rv))
                    abortLoad = PR_TRUE;
            }

            if (!abortLoad)
            {
                // Add the install item to the trigger collection
                nsXPITriggerItem *item =
                    new nsXPITriggerItem( name, xpiURL.get(), icon.get(), hash );
                if ( item )
                {
                    trigger->Add( item );
                }
                else
                    abortLoad = PR_TRUE;
            }
        }
        else
            abortLoad = PR_TRUE;
      }
      JS_DestroyIdArray( cx, ida );
    }


    // pass on only if good stuff found
    if (!abortLoad && trigger->Size() > 0)
    {
        nsCOMPtr<nsIURI> checkuri;
        rv = nativeThis->GetOriginatingURI(globalObject,
                                           getter_AddRefs(checkuri));
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIDOMWindow> win = do_QueryInterface(globalObject);
            nsCOMPtr<nsIXPIInstallInfo> installInfo =
                new nsXPIInstallInfo(win, checkuri, trigger, 0);
            if (installInfo)
            {
                // installInfo now owns triggers
                bool enabled = false;
                nativeThis->UpdateEnabled(checkuri, XPI_WHITELIST, &enabled);
                if (!enabled)
                {
                    nsCOMPtr<nsIObserverService> os =
                      mozilla::services::GetObserverService();
                    if (os)
                        os->NotifyObservers(installInfo,
                                            "xpinstall-install-blocked",
                                            nsnull);
                }
                else
                {
                    // save callback function if any (ignore bad args for now)
                    if ( argc >= 2 && JS_TypeOfValue(cx,argv[1]) == JSTYPE_FUNCTION )
                    {
                        trigger->SaveCallback( cx, argv[1] );
                    }

                    bool result;
                    nativeThis->StartInstall(installInfo, &result);
                    *rval = BOOLEAN_TO_JSVAL(result);
                }
                return JS_TRUE;
            }
        }
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
static JSBool
InstallTriggerGlobalInstallChrome(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = getTriggerNative(cx, obj);
  if (!nativeThis)
    return JS_FALSE;

  uint32       chromeType = NOT_CHROME;
  nsAutoString sourceURL;
  nsAutoString name;

  *rval = JSVAL_FALSE;

  // get chromeType first, the update enabled check for skins skips whitelisting
  if (argc >=1)
  {
      if (!JS_ValueToECMAUint32(cx, argv[0], &chromeType))
          return JS_FALSE;
  }

  // make sure XPInstall is enabled, return if not
  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (scriptContext)
      globalObject = scriptContext->GetGlobalObject();

  if (!globalObject)
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
    nsresult rv = InstallTriggerCheckLoadURIFromScript(cx, sourceURL);
    if (NS_FAILED(rv))
        return JS_FALSE;

    if ( chromeType & CHROME_ALL )
    {
        // there's at least one known chrome type
        nsCOMPtr<nsIURI> checkuri;
        nsresult rv = nativeThis->GetOriginatingURI(globalObject,
                                                    getter_AddRefs(checkuri));
        if (NS_SUCCEEDED(rv))
        {
            nsAutoPtr<nsXPITriggerInfo> trigger(new nsXPITriggerInfo());
            nsAutoPtr<nsXPITriggerItem> item(new nsXPITriggerItem(name.get(),
                                                                  sourceURL.get(),
                                                                  nsnull));
            if (trigger && item)
            {
                // trigger will free item when complete
                trigger->Add(item.forget());
                nsCOMPtr<nsIDOMWindow> win = do_QueryInterface(globalObject);
                nsCOMPtr<nsIXPIInstallInfo> installInfo =
                    new nsXPIInstallInfo(win, checkuri, trigger, chromeType);
                if (installInfo)
                {
                    // installInfo owns trigger now
                    trigger.forget();
                    bool enabled = false;
                    nativeThis->UpdateEnabled(checkuri, XPI_WHITELIST,
                                              &enabled);
                    if (!enabled)
                    {
                        nsCOMPtr<nsIObserverService> os =
                          mozilla::services::GetObserverService();
                        if (os)
                            os->NotifyObservers(installInfo,
                                                "xpinstall-install-blocked",
                                                nsnull);
                    }
                    else
                    {
                        bool nativeRet = false;
                        nativeThis->StartInstall(installInfo, &nativeRet);
                        *rval = BOOLEAN_TO_JSVAL(nativeRet);
                    }
                }
            }
        }
    }
  }
  return JS_TRUE;
}


//
// Native method StartSoftwareUpdate
//
static JSBool
InstallTriggerGlobalStartSoftwareUpdate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMInstallTriggerGlobal *nativeThis = getTriggerNative(cx, obj);
  if (!nativeThis)
    return JS_FALSE;

  bool         nativeRet;
  PRInt32      flags = 0;

  *rval = JSVAL_FALSE;

  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (scriptContext)
      globalObject = scriptContext->GetGlobalObject();

  if (!globalObject)
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
    nsresult rv = InstallTriggerCheckLoadURIFromScript(cx, xpiURL);
    if (NS_FAILED(rv))
        return JS_FALSE;

    if (argc >= 2 && !JS_ValueToInt32(cx, argv[1], (int32 *)&flags))
    {
        JS_ReportError(cx, "StartSoftwareUpdate() 2nd parameter must be a number");
        return JS_FALSE;
    }

    nsCOMPtr<nsIURI> checkuri;
    rv = nativeThis->GetOriginatingURI(globalObject, getter_AddRefs(checkuri));
    if (NS_SUCCEEDED(rv))
    {
        nsAutoPtr<nsXPITriggerInfo> trigger(new nsXPITriggerInfo());
        nsAutoPtr<nsXPITriggerItem> item(new nsXPITriggerItem(0,
                                                              xpiURL.get(),
                                                              nsnull));
        if (trigger && item)
        {
            // trigger will free item when complete
            trigger->Add(item.forget());
            nsCOMPtr<nsIDOMWindow> win = do_QueryInterface(globalObject);
            nsCOMPtr<nsIXPIInstallInfo> installInfo =
                                new nsXPIInstallInfo(win, checkuri, trigger, 0);
            if (installInfo)
            {
                // From here trigger is owned by installInfo until passed on to nsXPInstallManager
                trigger.forget();
                bool enabled = false;
                nativeThis->UpdateEnabled(checkuri, XPI_WHITELIST, &enabled);
                if (!enabled)
                {
                    nsCOMPtr<nsIObserverService> os =
                      mozilla::services::GetObserverService();
                    if (os)
                        os->NotifyObservers(installInfo,
                                            "xpinstall-install-blocked",
                                            nsnull);
                }
                else
                {
                    nativeThis->StartInstall(installInfo, &nativeRet);
                    *rval = BOOLEAN_TO_JSVAL(nativeRet);
                }
            }
        }
    }
  }
  else
  {
    JS_ReportError(cx, "Function StartSoftwareUpdate requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// InstallTriggerGlobal class methods
//
static JSFunctionSpec InstallTriggerGlobalMethods[] =
{
  {"UpdateEnabled",         InstallTriggerGlobalUpdateEnabled,         0,0,0},
  {"StartSoftwareUpdate",   InstallTriggerGlobalStartSoftwareUpdate,   2,0,0},
  {"updateEnabled",         InstallTriggerGlobalUpdateEnabled,         0,0,0},
  {"enabled",               InstallTriggerGlobalUpdateEnabled,         0,0,0},
  {"install",               InstallTriggerGlobalInstall,               2,0,0},
  {"installChrome",         InstallTriggerGlobalInstallChrome,         2,0,0},
  {"startSoftwareUpdate",   InstallTriggerGlobalStartSoftwareUpdate,   2,0,0},
  {nsnull,nsnull,0,0,0}
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
    {0,nsnull}
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
  JSContext *jscontext = aContext->GetNativeContext();
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
  JSContext *jscontext = aContext->GetNativeContext();
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

