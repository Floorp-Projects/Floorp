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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Roy Yokoyama <yokoyama@netscape.com>
 *
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

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsIDOMWindow.h"
#include "nsIFontPackageService.h"
#include "nsIFontPackageHandler.h"
#include "nsILocale.h"
#include "nsTextFormatter.h"
#include "nsIStringBundle.h"
#include "nsIWindowWatcher.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsFontPackageHandler.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIWindowMediator.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define FONTPACKAGE_REGIONAL_URL "chrome://global-region/locale/region.properties"

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

  // check whether the topmost window is a mailnews window. If it is,
  // or if any of the calls fail, return NS_ERROR_ABORT
  nsresult rv;
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

    if (windowType.Equals(NS_LITERAL_STRING("mail:3pane")) ||
        windowType.Equals(NS_LITERAL_STRING("mail:messageWindow"))) {
      return NS_ERROR_ABORT;
    }
  }

  nsXPIDLCString absUrl;
  rv = CreateURLString(aFontPackID, getter_Copies(absUrl));
  if (NS_FAILED(rv)) 
    return rv;

  nsCOMPtr<nsIWindowWatcher> windowWatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  NS_ENSURE_TRUE(windowWatch, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindow> dialog;
  rv = windowWatch->OpenWindow(nsnull, absUrl, 
              "_blank",
              "chrome,centerscreen,titlebar,resizable", 
              nsnull, 
              getter_AddRefs(dialog));

  nsCOMPtr<nsIFontPackageService> fontService(do_GetService(NS_FONTPACKAGESERVICE_CONTRACTID));
  NS_ENSURE_TRUE(fontService, NS_ERROR_FAILURE);
  fontService->FontPackageHandled(NS_SUCCEEDED(rv), PR_FALSE, aFontPackID);
  return rv;
}

nsresult nsFontPackageHandler::CreateURLString(const char *aPackID, char **aURL)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(aPackID, "illegal value- null ptr- aPackID");
  NS_ASSERTION(aURL, "illegal value- null ptr- aURL");

  if (strlen(aPackID) <= 5)
    return NS_ERROR_INVALID_ARG;
 
  nsCOMPtr<nsIStringBundleService> strings(do_GetService(kStringBundleServiceCID));
  nsCOMPtr<nsIStringBundle> bundle;

  // get the font downloading URL from properties file
  rv = strings->CreateBundle(FONTPACKAGE_REGIONAL_URL, getter_AddRefs(bundle));
  if (NS_FAILED(rv)) 
    return rv;

  nsXPIDLString fontURL;
  bundle->GetStringFromName(NS_LITERAL_STRING("fontDownloadURL").get(), 
                              getter_Copies(fontURL));

  if (!fontURL.get()) {
    NS_ERROR("No item is found. Check chrome://global-region/locale/region.properties");
    return NS_ERROR_FAILURE;
  }

  // aPackID="lang:xxxx", strip "lang:" and get the actual langID
  const char *langID = &aPackID[5]; 
  PRUnichar *urlString = nsnull;
  urlString = nsTextFormatter::smprintf(fontURL.get(), langID);
  if (!urlString) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aURL = ToNewUTF8String(nsDependentString(urlString));
  nsTextFormatter::smprintf_free(urlString);
  
  return NS_OK;
}

