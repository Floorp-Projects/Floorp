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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com>
 *   Simon Montagu <smontagu@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
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

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindow.h"
#include "nsIFontPackageService.h"
#include "nsIFontPackageHandler.h"
#include "nsILocale.h"
#include "nsIWindowWatcher.h"
#include "nsReadableUtils.h"
#include "nsFontPackageHandler.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIWindowMediator.h"
#include "nsISupportsPrimitives.h"
#include "nsIStringBundle.h"
#include "nsUnicharUtils.h"

//==============================================================================
/* Implementation file */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsFontPackageHandler, nsIFontPackageHandler)

nsFontPackageHandler::nsFontPackageHandler()
{
}

nsFontPackageHandler::~nsFontPackageHandler()
{
}

/* void NeedFontPackage (in string aFontPackID); */
NS_IMETHODIMP nsFontPackageHandler::NeedFontPackage(const char *aFontPackID)
{
  // no FontPackage is passed, return
  NS_ENSURE_ARG_POINTER(aFontPackID);

  if (!strlen(aFontPackID))
    return NS_ERROR_UNEXPECTED;

  nsresult rv;
  nsCOMPtr <nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr <nsIStringBundle> bundle;
  rv = bundleService->CreateBundle("chrome://global/locale/fontpackage.properties", getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLString handledLanguages;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("handled_languages").get(), getter_Copies(handledLanguages));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // aFontPackID is of the form lang:xx or lang:xx-YY
  const char *colonPos = strchr(aFontPackID,':');
  if (!colonPos || !*(colonPos + 1))
    return NS_ERROR_UNEXPECTED;
  
  // turn (const char *)xx-YY into (PRUnichar *)xx-yy
  nsAutoString langCode;
  CopyASCIItoUCS2(nsDependentCString(colonPos + 1), langCode);
  ToLowerCase(langCode);

  // check for xx or xx-yy in handled_languages
  // if not handled, return now, don't show the font dialog
  if (!FindInReadable(langCode, handledLanguages))
    return NS_OK; // XXX should be error?

  // check whether the topmost window is a mailnews window. If it is,
  // or if any of the calls fail, return NS_ERROR_ABORT
  nsCOMPtr<nsIWindowMediator> windowMediator = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);

  nsCOMPtr<nsISimpleEnumerator> windowEnum;
  rv = windowMediator->GetZOrderDOMWindowEnumerator(nsnull, PR_TRUE, getter_AddRefs(windowEnum));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);

  nsCOMPtr<nsISupports> windowSupports;
  nsCOMPtr<nsIDOMWindow> topMostWindow;
  nsCOMPtr<nsIDOMDocument> domDocument;
  nsCOMPtr<nsIDOMElement> domElement;
  nsAutoString windowType;
  PRBool more;
  windowEnum->HasMoreElements(&more);
  if (more) {
    rv = windowEnum->GetNext(getter_AddRefs(windowSupports));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);
    NS_ENSURE_TRUE(windowSupports, NS_ERROR_ABORT);

    topMostWindow = do_QueryInterface(windowSupports, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);
    NS_ENSURE_TRUE(topMostWindow, NS_ERROR_ABORT);

    rv = topMostWindow->GetDocument(getter_AddRefs(domDocument));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);
    NS_ENSURE_TRUE(domDocument, NS_ERROR_ABORT);

    rv = domDocument->GetDocumentElement(getter_AddRefs(domElement));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);
    NS_ENSURE_TRUE(domElement, NS_ERROR_ABORT);

    rv = domElement->GetAttribute(NS_LITERAL_STRING("windowtype"), windowType);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);

    if (windowType.EqualsLiteral("mail:3pane") ||
        windowType.EqualsLiteral("mail:messageWindow")) {
      return NS_ERROR_ABORT;
    }
  }

  nsCOMPtr<nsIWindowWatcher> windowWatch = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsISupportsString> langID = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  langID->SetData(langCode);

  nsCOMPtr<nsIDOMWindow> dialog;
  rv = windowWatch->OpenWindow(nsnull, "chrome://global/content/fontpackage.xul", 
              "_blank",
              "chrome,centerscreen,titlebar,resizeable=no", // XXX should this be modal?
              langID, 
              getter_AddRefs(dialog));

  nsCOMPtr<nsIFontPackageService> fontService(do_GetService(NS_FONTPACKAGESERVICE_CONTRACTID));
  NS_ENSURE_TRUE(fontService, NS_ERROR_FAILURE);

  fontService->FontPackageHandled(NS_SUCCEEDED(rv), PR_FALSE, aFontPackID);
    return rv;
  }
