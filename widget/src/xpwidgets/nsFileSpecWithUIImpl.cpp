/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsFileSpecWithUIImpl.h"

#include "nsIFileWidget.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

#undef NS_FILE_FAILURE
#define NS_FILE_FAILURE NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES,(0xFFFF))

NS_IMPL_ISUPPORTS(nsFileSpecWithUIImpl, nsCOMTypeInfo<nsIFileSpecWithUI>::GetIID())

static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);

#include "nsIComponentManager.h"

//----------------------------------------------------------------------------------------
nsFileSpecWithUIImpl::nsFileSpecWithUIImpl()
//----------------------------------------------------------------------------------------
{
    NS_INIT_REFCNT();
	nsresult rv = nsComponentManager::CreateInstance(
		(const char*)NS_FILESPEC_PROGID,
		(nsISupports*)nsnull,
		(const nsID&)nsCOMTypeInfo<nsIFileSpec>::GetIID(),
		(void**)getter_AddRefs(mBaseFileSpec));
	NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a file spec.");
}

//----------------------------------------------------------------------------------------
nsFileSpecWithUIImpl::~nsFileSpecWithUIImpl()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecWithUIImpl::ChooseOutputFile(
	const char *windowTitle,
	const char *suggestedLeafName)
//----------------------------------------------------------------------------------------
{
    if (!mBaseFileSpec)
    	return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIFileWidget> fileWidget; 
    nsresult rv = nsComponentManager::CreateInstance(
    	kCFileWidgetCID,
        nsnull,
        nsCOMTypeInfo<nsIFileWidget>::GetIID(),
        (void**)getter_AddRefs(fileWidget));
    if (NS_FAILED(rv))
    	return rv;
    
    fileWidget->SetDefaultString(suggestedLeafName);
    nsFileSpec spec;
    nsFileDlgResults result = fileWidget->PutFile(nsnull, windowTitle, spec);
    if (result != nsFileDlgResults_OK)
    	return NS_FILE_FAILURE;
    if (spec.Exists() && result != nsFileDlgResults_Replace)
    	return NS_FILE_FAILURE;
    return mBaseFileSpec->SetFromFileSpec(spec);
} // nsFileSpecImpl::ChooseOutputFile

NS_IMETHODIMP nsFileSpecWithUIImpl::ChooseFile(const char *title, char **_retval)
{
	nsresult rv = ChooseInputFile(title, eAllFiles, nsnull, nsnull);
	if (NS_FAILED(rv)) return rv;
	rv = GetURLString(_retval);
	return rv;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecWithUIImpl::ChooseInputFile(
		const char *inTitle,
		nsIFileSpecWithUI::StandardFilterMask inMask,
		const char *inExtraFilterTitle, const char *inExtraFilter)
//----------------------------------------------------------------------------------------
{
    if (!mBaseFileSpec)
    	return NS_ERROR_NULL_POINTER;
	nsresult rv = NS_OK;
	nsString* nextTitle;
	nsString* nextFilter;
    nsFileSpec spec;
    nsCOMPtr<nsIFileWidget> fileWidget; 
    rv = nsComponentManager::CreateInstance(
    	kCFileWidgetCID,
        nsnull,
        nsCOMTypeInfo<nsIFileWidget>::GetIID(),
        (void**)getter_AddRefs(fileWidget));
	if (NS_FAILED(rv))
		return rv;
	nsString* titles = nsnull;
	nsString* filters = nsnull;
	titles = new nsString[1 + kNumStandardFilters];
	if (!titles)
	{
		rv = NS_ERROR_OUT_OF_MEMORY;
		goto Clean;
	}
	filters = new nsString[1 + kNumStandardFilters];
	if (!filters)
	{
		rv = NS_ERROR_OUT_OF_MEMORY;
		goto Clean;
	}
	nextTitle = titles;
	nextFilter = filters;
	if (inMask & eAllReadable)
	{
		*nextTitle++ = "All Readable Files";
		*nextFilter++ = "*.htm; *.html; *.xml; *.gif; *.jpg; *.jpeg; *.png";
	}
	if (inMask & eHTMLFiles)
	{
		*nextTitle++ = "HTML Files";
		*nextFilter++ = "*.htm; *.html";
	}
	if (inMask & eXMLFiles)
	{
		*nextTitle++ = "XML Files";
		*nextFilter++ =  "*.xml";
	}
	if (inMask & eImageFiles)
	{
		*nextTitle++ = "Image Files";
		*nextFilter++ = "*.gif; *.jpg; *.jpeg; *.png";
	}
	if (inMask & eExtraFilter)
	{
		*nextTitle++ = inExtraFilterTitle;
		*nextFilter++ = inExtraFilter;
	}
	if (inMask & eAllFiles)
	{
		*nextTitle++ = "All Files";
		*nextFilter++ = "*.*";
	}

	fileWidget->SetFilterList(nextFilter - filters, titles, filters);
	if (fileWidget->GetFile(nsnull, inTitle, spec) != nsFileDlgResults_OK)
		rv = NS_FILE_FAILURE;
    rv = mBaseFileSpec->SetFromFileSpec(spec);

Clean:
	delete [] titles;
	delete [] filters;
	return rv;
} // nsFileSpecWithUIImpl::ChooseInputFile

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecWithUIImpl::ChooseDirectory(const char *title, char **_retval)
//----------------------------------------------------------------------------------------
{
    if (!mBaseFileSpec)
    	return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIFileWidget> fileWidget;
    nsresult rv = nsComponentManager::CreateInstance(
    	kCFileWidgetCID,
        nsnull,
        nsCOMTypeInfo<nsIFileWidget>::GetIID(),
        (void**)getter_AddRefs(fileWidget));
	if (NS_FAILED(rv))
		return rv;
    nsFileSpec spec;
	if (fileWidget->GetFolder(nsnull, title, spec) != nsFileDlgResults_OK)
		rv = NS_FILE_FAILURE;

  rv = mBaseFileSpec->SetFromFileSpec(spec);
	if (NS_FAILED(rv))
		return rv;
    
  return GetURLString(_retval);
} // nsFileSpecWithUIImpl::chooseDirectory

