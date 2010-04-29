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
 * The Original Code is the Extension Manager.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "amInstallTrigger.h"
#include "nsIClassInfoImpl.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsICategoryManager.h"
#include "nsServiceManagerUtils.h"
#include "nsXPIDLString.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsDOMJSUtils.h"
#include "nsIXPConnect.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"

//
// Helper function for URI verification
//
static nsresult
CheckLoadURIFromScript(JSContext *cx, const nsACString& uriStr)
{
  nsresult rv;
  nsCOMPtr<nsIScriptSecurityManager> secman(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
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

NS_IMPL_ISUPPORTS1_CI(amInstallTrigger, amIInstallTrigger)

amInstallTrigger::amInstallTrigger()
{
  mManager = do_GetService("@mozilla.org/addons/integration;1");
}

amInstallTrigger::~amInstallTrigger()
{
}

JSContext*
amInstallTrigger::GetJSContext()
{
  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());

  // get the xpconnect native call context
  nsAXPCNativeCallContext *cc = nsnull;
  xpc->GetCurrentNativeCallContext(&cc);
  if (!cc)
    return nsnull;

  // Get JSContext of current call
  JSContext* cx;
  nsresult rv = cc->GetJSContext(&cx);
  if (NS_FAILED(rv))
    return nsnull;

  return cx;
}

already_AddRefed<nsIDOMWindowInternal>
amInstallTrigger::GetOriginatingWindow(JSContext* cx)
{
  nsIScriptGlobalObject *globalObject = nsnull;
  nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
  if (!scriptContext)
    return nsnull;

  globalObject = scriptContext->GetGlobalObject();
  if (!globalObject)
    return nsnull;

  nsCOMPtr<nsIDOMWindowInternal> window = do_QueryInterface(globalObject);
  return window.forget();
}

already_AddRefed<nsIURI>
amInstallTrigger::GetOriginatingURI(nsIDOMWindowInternal* aWindow)
{
  if (!aWindow)
    return nsnull;

  nsCOMPtr<nsIDOMDocument> domdoc;
  aWindow->GetDocument(getter_AddRefs(domdoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
  nsIURI* uri = doc->GetDocumentURI();
  NS_IF_ADDREF(uri);
  return uri;
}

/* boolean updateEnabled (); */
NS_IMETHODIMP
amInstallTrigger::UpdateEnabled(PRBool *_retval NS_OUTPARAM)
{
  return Enabled(_retval);
}

/* boolean enabled (); */
NS_IMETHODIMP
amInstallTrigger::Enabled(PRBool *_retval NS_OUTPARAM)
{
  nsCOMPtr<nsIDOMWindowInternal> window = GetOriginatingWindow(GetJSContext());
  nsCOMPtr<nsIURI> referer = GetOriginatingURI(window);

  return mManager->IsInstallEnabled(NS_LITERAL_STRING("application/x-xpinstall"), referer, _retval);
}

/* boolean install (in nsIVariant args, [optional] in amIInstallCallback callback); */
NS_IMETHODIMP
amInstallTrigger::Install(nsIVariant *args,
                          amIInstallCallback *callback,
                          PRBool *_retval NS_OUTPARAM)
{
  JSContext *cx = GetJSContext();
  nsCOMPtr<nsIDOMWindowInternal> window = GetOriginatingWindow(cx);
  nsCOMPtr<nsIURI> referer = GetOriginatingURI(window);

  jsval params;
  nsresult rv = args->GetAsJSVal(&params);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!JSVAL_IS_OBJECT(params) || !JSVAL_TO_OBJECT(params))
    return NS_ERROR_INVALID_ARG;

  JSIdArray *ida = JS_Enumerate(cx, JSVAL_TO_OBJECT(params));
  if (!ida)
    return NS_ERROR_FAILURE;

  PRUint32 count = ida->length;

  nsTArray<const PRUnichar*> names;
  nsTArray<const PRUnichar*> uris;
  nsTArray<const PRUnichar*> icons;
  nsTArray<const PRUnichar*> hashes;

  jsval v;
  for (PRUint32 i = 0; i < count; i++ ) {
    JS_IdToValue(cx, ida->vector[i], &v);
    JSString *str = JS_ValueToString(cx, v);
    if (!str) {
      JS_DestroyIdArray(cx, ida);
      return NS_ERROR_FAILURE;
    }

    const PRUnichar* name = reinterpret_cast<const PRUnichar*>(JS_GetStringChars(str));
    const PRUnichar* uri = nsnull;
    const PRUnichar* icon = nsnull;
    const PRUnichar* hash = nsnull;

    JS_GetUCProperty(cx, JSVAL_TO_OBJECT(params), reinterpret_cast<const jschar*>(name), nsCRT::strlen(name), &v);
    if (JSVAL_IS_OBJECT(v) && JSVAL_TO_OBJECT(v)) {
      jsval v2;
      if (JS_GetProperty(cx, JSVAL_TO_OBJECT(v), "URL", &v2) && !JSVAL_IS_VOID(v2)) {
        JSString *str = JS_ValueToString(cx, v2);
        if (!str) {
          JS_DestroyIdArray(cx, ida);
          return NS_ERROR_FAILURE;
        }
        uri = reinterpret_cast<const PRUnichar*>(JS_GetStringChars(str));
      }

      if (JS_GetProperty(cx, JSVAL_TO_OBJECT(v), "IconURL", &v2) && !JSVAL_IS_VOID(v2)) {
        JSString *str = JS_ValueToString(cx, v2);
        if (!str) {
          JS_DestroyIdArray(cx, ida);
          return NS_ERROR_FAILURE;
        }
        icon = reinterpret_cast<const PRUnichar*>(JS_GetStringChars(str));
      }

      if (JS_GetProperty(cx, JSVAL_TO_OBJECT(v), "Hash", &v2) && !JSVAL_IS_VOID(v2)) {
        JSString *str = JS_ValueToString(cx, v2);
        if (!str) {
          JS_DestroyIdArray(cx, ida);
          return NS_ERROR_FAILURE;
        }
        hash = reinterpret_cast<const PRUnichar*>(JS_GetStringChars(str));
      }
    }
    else {
      uri = reinterpret_cast<const PRUnichar*>(JS_GetStringChars(JS_ValueToString(cx, v)));
    }

    if (!uri) {
      JS_DestroyIdArray(cx, ida);
      return NS_ERROR_FAILURE;
    }

    nsCString tmpURI = NS_ConvertUTF16toUTF8(uri);
    // Get relative URL to load
    if (referer) {
      rv = referer->Resolve(tmpURI, tmpURI);
      if (NS_FAILED(rv)) {
        JS_DestroyIdArray(cx, ida);
        return rv;
      }
    }

    rv = CheckLoadURIFromScript(cx, tmpURI);
    if (NS_FAILED(rv)) {
      JS_DestroyIdArray(cx, ida);
      return rv;
    }
    uri = UTF8ToNewUnicode(tmpURI);

    if (icon) {
      nsCString tmpIcon = NS_ConvertUTF16toUTF8(icon);
      if (referer) {
        rv = referer->Resolve(tmpIcon, tmpIcon);
        if (NS_FAILED(rv)) {
          JS_DestroyIdArray(cx, ida);
          return rv;
        }
      }

      // If the page can't load the icon then just ignore it
      rv = CheckLoadURIFromScript(cx, tmpIcon);
      if (NS_FAILED(rv))
        icon = nsnull;
      else
        icon = UTF8ToNewUnicode(tmpIcon);
    }

    names.AppendElement(name);
    uris.AppendElement(uri);
    icons.AppendElement(icon);
    hashes.AppendElement(hash);
  }

  JS_DestroyIdArray(cx, ida);

  rv = mManager->InstallAddonsFromWebpage(NS_LITERAL_STRING("application/x-xpinstall"),
                                          window, referer, uris.Elements(),
                                          hashes.Elements(), names.Elements(),
                                          icons.Elements(), callback, count,
                                          _retval);

  for (PRUint32 i = 0; i < uris.Length(); i++) {
    NS_Free(const_cast<PRUnichar*>(uris[i]));
    if (icons[i])
      NS_Free(const_cast<PRUnichar*>(icons[i]));
  }

  return rv;
}

/* boolean installChrome (in PRUint32 type, in AString url, in AString skin); */
NS_IMETHODIMP
amInstallTrigger::InstallChrome(PRUint32 type,
                                const nsAString & url,
                                const nsAString & skin,
                                PRBool *_retval NS_OUTPARAM)
{
  return StartSoftwareUpdate(url, 0, _retval);
}

/* boolean startSoftwareUpdate (in AString url, [optional] in PRInt32 flags); */
NS_IMETHODIMP
amInstallTrigger::StartSoftwareUpdate(const nsAString & url,
                                      PRInt32 flags,
                                      PRBool *_retval NS_OUTPARAM)
{
  nsresult rv;

  JSContext *cx = GetJSContext();
  nsCOMPtr<nsIDOMWindowInternal> window = GetOriginatingWindow(cx);
  nsCOMPtr<nsIURI> referer = GetOriginatingURI(window);

  nsTArray<const PRUnichar*> names;
  nsTArray<const PRUnichar*> uris;
  nsTArray<const PRUnichar*> icons;
  nsTArray<const PRUnichar*> hashes;

  nsCString tmpURI = NS_ConvertUTF16toUTF8(url);
  // Get relative URL to load
  if (referer) {
    rv = referer->Resolve(tmpURI, tmpURI);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = CheckLoadURIFromScript(cx, tmpURI);
  NS_ENSURE_SUCCESS(rv, rv);

  names.AppendElement((PRUnichar*)nsnull);
  uris.AppendElement(UTF8ToNewUnicode(tmpURI));
  icons.AppendElement((PRUnichar*)nsnull);
  hashes.AppendElement((PRUnichar*)nsnull);

  rv = mManager->InstallAddonsFromWebpage(NS_LITERAL_STRING("application/x-xpinstall"),
                                          window, referer, uris.Elements(),
                                          hashes.Elements(), names.Elements(),
                                          icons.Elements(), nsnull, 1, _retval);

  NS_Free(const_cast<PRUnichar*>(uris[0]));
  return rv;
}

NS_DECL_CLASSINFO(amInstallTrigger)

NS_GENERIC_FACTORY_CONSTRUCTOR(amInstallTrigger)

static NS_METHOD
RegisterInstallTrigger(nsIComponentManager *aCompMgr,
                       nsIFile *aPath,
                       const char *registryLocation,
                       const char *componentType,
                       const nsModuleComponentInfo *info)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsICategoryManager> catman = 
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString previous;
  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY,
                                "InstallTrigger",
                                AM_INSTALLTRIGGER_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// The list of components we register
static const nsModuleComponentInfo components[] =
{
    { "InstallTrigger Component",
       AM_InstallTrigger_CID,
       AM_INSTALLTRIGGER_CONTRACTID,
       amInstallTriggerConstructor,
       RegisterInstallTrigger,
       nsnull, // unregister
       nsnull, // factory destructor
       NS_CI_INTERFACE_GETTER_NAME(amInstallTrigger),
       nsnull, // language helper
       &NS_CLASSINFO_NAME(amInstallTrigger),
       nsIClassInfo::DOM_OBJECT // flags
    }
};

NS_IMPL_NSGETMODULE(AddonsModule, components)
