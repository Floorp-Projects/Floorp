/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#include "nsFileChooser.h"
#include "nsIFileWidget.h"
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsCRT.h"

static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);

#undef NS_FILE_FAILURE
#define NS_FILE_FAILURE NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES,(0xFFFF))

nsFileChooser::nsFileChooser()
    : mFileSpec()
{
    NS_INIT_REFCNT();
    // Add any other initialization in Init, not here.
}

NS_IMETHODIMP
nsFileChooser::Init()
{
    // Add any other initialization here.
    return NS_OK;
}

nsFileChooser::~nsFileChooser()
{
}

NS_METHOD
nsFileChooser::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
  
    nsFileChooser* FileChooser = new nsFileChooser();
    if (FileChooser == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(FileChooser);
    nsresult rv = FileChooser->QueryInterface(aIID, aResult);
    NS_RELEASE(FileChooser);
    return rv;
}

NS_IMPL_ISUPPORTS(nsFileChooser, nsIFileChooser::GetIID());

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChooser::ChooseOutputFile(const char* windowTitle,
                                const char* suggestedLeafName)
{
    nsresult rv;
    nsCOMPtr<nsIFileWidget> fileWidget; 
    rv = nsComponentManager::CreateInstance(kCFileWidgetCID,
                                            nsnull,
                                            nsIFileWidget::GetIID(),
                                            (void**)getter_AddRefs(fileWidget));
    if (NS_FAILED(rv))
    	return rv;
    
    fileWidget->SetDefaultString(suggestedLeafName);
    nsFileDlgResults result = fileWidget->PutFile(nsnull, windowTitle, mFileSpec);
    if ( result != nsFileDlgResults_OK)
    	return NS_FILE_FAILURE;
    if (mFileSpec.Exists() && result != nsFileDlgResults_Replace)
    	return NS_FILE_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChooser::ChooseInputFile(const char* inTitle,
                               StandardFilterMask inMask,
                               const char* inExtraFilterTitle,
                               const char* inExtraFilter)
{
    nsresult rv = NS_OK;
    nsString* nextTitle;
    nsString* nextFilter;
    nsCOMPtr<nsIFileWidget> fileWidget; 
    rv = nsComponentManager::CreateInstance(kCFileWidgetCID,
                                            nsnull,
                                            nsIFileWidget::GetIID(),
                                            (void**)getter_AddRefs(fileWidget));
    if (NS_FAILED(rv))
        return rv;

    nsString* titles = nsnull;
    nsString* filters = nsnull;
    titles = new nsString[1 + kNumStandardFilters];
    if (!titles) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto Clean;
    }
    filters = new nsString[1 + kNumStandardFilters];
    if (!filters) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto Clean;
    }
    nextTitle = titles;
    nextFilter = filters;
    if (inMask & eAllReadable) {
        *nextTitle++ = "All Readable Files";
        *nextFilter++ = "*.htm; *.html; *.xml; *.gif; *.jpg; *.jpeg; *.png";
    }
    if (inMask & eHTMLFiles) {
        *nextTitle++ = "HTML Files";
        *nextFilter++ = "*.htm; *.html";
    }
    if (inMask & eXMLFiles) {
        *nextTitle++ = "XML Files";
        *nextFilter++ =  "*.xml";
    }
    if (inMask & eImageFiles) {
        *nextTitle++ = "Image Files";
        *nextFilter++ = "*.gif; *.jpg; *.jpeg; *.png";
    }
    if (inMask & eExtraFilter) {
        *nextTitle++ = inExtraFilterTitle;
        *nextFilter++ = inExtraFilter;
    }
    if (inMask & eAllFiles) {
        *nextTitle++ = "All Files";
        *nextFilter++ = "*.*";
    }

    fileWidget->SetFilterList(nextFilter - filters, titles, filters);
    if (fileWidget->GetFile(nsnull, inTitle, mFileSpec) != nsFileDlgResults_OK)
        rv = NS_FILE_FAILURE;

  Clean:
    delete [] titles;
    delete [] filters;
    return rv;
}

NS_IMETHODIMP
nsFileChooser::ChooseDirectory(const char* title)
{
    nsresult rv;
    nsCOMPtr<nsIFileWidget> fileWidget;
    rv = nsComponentManager::CreateInstance(kCFileWidgetCID,
                                            nsnull,
                                            nsIFileWidget::GetIID(),
                                            (void**)getter_AddRefs(fileWidget));
    if (NS_FAILED(rv))
        return rv;
    if (fileWidget->GetFolder(nsnull, title, mFileSpec) != nsFileDlgResults_OK)
        rv = NS_FILE_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChooser::GetURLString(char* *result)
{
    nsFileURL fileURL(mFileSpec);
    char* urlStr = nsCRT::strdup(fileURL.GetURLString());
    if (urlStr == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = urlStr;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

PR_IMPLEMENT(nsresult)
NS_NewFileChooser(nsIFileChooser* *result)
{
    nsresult rv = nsFileChooser::Create(NULL, nsIFileChooser::GetIID(),
                                        (void**)result);
    if (NS_SUCCEEDED(rv)) {
        rv = (*result)->Init();
        if (NS_FAILED(rv))
            NS_RELEASE(*result);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

