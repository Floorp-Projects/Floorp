/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Pinkerton   <pinkerton@netscape.com>
 */

#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIWidget.h"

#include "nsIStringBundle.h"

#include "nsBaseFilePicker.h"


static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
#define FILEPICKER_PROPERTIES "chrome://global/locale/filepicker.properties"


nsBaseFilePicker::nsBaseFilePicker()
{

}

nsBaseFilePicker::~nsBaseFilePicker()
{

}

/* XXX aaaarrrrrrgh! */
NS_IMETHODIMP nsBaseFilePicker::DOMWindowToWidget(nsIDOMWindow *dw, nsIWidget **aResult)
{
  nsresult rv = NS_ERROR_FAILURE;
  *aResult = nsnull;

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(dw);
  if (sgo) {
    nsCOMPtr<nsIDocShell> docShell;
    sgo->GetDocShell(getter_AddRefs(docShell));
    
    if (docShell) {

      nsCOMPtr<nsIPresShell> presShell;
      rv = docShell->GetPresShell(getter_AddRefs(presShell));

      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIViewManager> viewManager;
        rv = presShell->GetViewManager(getter_AddRefs(viewManager));
            
        if (NS_SUCCEEDED(rv)) {
          nsIView *view;
          rv = viewManager->GetRootView(view);
              
          if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIWidget> widget;
            rv = view->GetWidget(*getter_AddRefs(widget));

            if (NS_SUCCEEDED(rv)) {
              *aResult = widget;
              NS_ADDREF(*aResult);
              return NS_OK;
            }
		
          }
        }
      }
    }
  }
  return rv;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseFilePicker::Init(nsIDOMWindow *aParent,
                                     const PRUnichar *aTitle,
                                     PRInt16 aMode)
{
  nsCOMPtr<nsIWidget> widget;
  nsresult rv = DOMWindowToWidget(aParent, getter_AddRefs(widget));

  if (NS_SUCCEEDED(rv)) {
    return InitNative(widget, aTitle, aMode);
  } else {
    return InitNative(nsnull, aTitle, aMode);
  }

  return rv;
}


NS_IMETHODIMP
nsBaseFilePicker::AppendFilters(PRInt32 aFilterMask)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> stringService = do_GetService(kStringBundleServiceCID);
  nsCOMPtr<nsIStringBundle> stringBundle;
  nsILocale   *locale = nsnull;

  rv = stringService->CreateBundle(FILEPICKER_PROPERTIES, locale, getter_AddRefs(stringBundle));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  PRUnichar *title;
  PRUnichar *filter;

  if (aFilterMask & filterAll) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("allTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("allFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterHTML) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("htmlTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("htmlFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterText) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("textTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("textFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterImages) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("imageTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("imageFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXML) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("xmlTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("xmlFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXUL) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("xulTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("xulFilter").GetUnicode(), &filter);
    AppendFilter(title, filter);
  }

  return NS_OK;
}


