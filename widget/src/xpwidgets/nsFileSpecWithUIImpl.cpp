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

#include "nsFileSpecWithUIImpl.h"

#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

#undef NS_FILE_FAILURE
#define NS_FILE_FAILURE NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES,(0xFFFF))

NS_IMPL_ADDREF(nsFileSpecWithUIImpl)
NS_IMPL_RELEASE(nsFileSpecWithUIImpl)

NS_IMETHODIMP nsFileSpecWithUIImpl::QueryInterface(REFNSIID aIID, 
                                                   void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  static NS_DEFINE_IID(kClassIID, nsCOMTypeInfo<nsIFileSpecWithUI>::GetIID());
  if (aIID.Equals(kClassIID))
    *aInstancePtr = (void*) this;
  else if (aIID.Equals(nsCOMTypeInfo<nsIFileSpec>::GetIID()))
    *aInstancePtr = (void*) this;
  else if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
    *aInstancePtr = (void*) ((nsISupports*)this);

  if (*aInstancePtr)
  {
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

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
	const char *suggestedLeafName,
    PRUint32 outMask)
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

    SetFileWidgetFilterList(fileWidget, outMask, nsnull, nsnull);

    // If there is a filespec specified, then start there.
    SetFileWidgetStartDir(fileWidget);

    nsFileSpec spec;
    
    nsString winTitle(windowTitle);

    nsFileDlgResults result = fileWidget->PutFile(nsnull, winTitle, spec);
    if (result != nsFileDlgResults_OK)
    {
      if (result == nsFileDlgResults_Cancel)
        return NS_ERROR_ABORT;
      if (spec.Exists() && result != nsFileDlgResults_Replace)
        return NS_FILE_FAILURE;
    }
    return mBaseFileSpec->SetFromFileSpec(spec);
} // nsFileSpecImpl::ChooseOutputFile

NS_IMETHODIMP nsFileSpecWithUIImpl::ChooseFile(const char *title, char **_retval)
{
	nsresult rv = ChooseInputFile(title, eAllFiles, nsnull, nsnull);
	if (NS_FAILED(rv)) return rv;
	rv = GetURLString(_retval);
	return rv;
}

// ---------------------------------------------------------------------------
void nsFileSpecWithUIImpl::SetFileWidgetFilterList(
  nsIFileWidget* fileWidget, PRUint32 mask,
  const char *inExtraFilterTitle, const char *inExtraFilter)
{
  if (!fileWidget) return;
	nsString* nextTitle;
	nsString* nextFilter;
	nsString* titles = nsnull;
	nsString* filters = nsnull;
	titles = new nsString[1 + kNumStandardFilters];
	if (!titles)
	{
		goto Clean;
	}
	filters = new nsString[1 + kNumStandardFilters];
	if (!filters)
	{
		goto Clean;
	}
	nextTitle = titles;
	nextFilter = filters;
	if (mask & eAllReadable)
	{
		*nextTitle++ = "All Readable Files";
		*nextFilter++ = "*.eml; *.txt; *.htm; *.html; *.xml; *.gif; *.jpg; *.jpeg; *.png";
	}
  if (mask & eMailFiles)
  {
    *nextTitle++ = "Mail Files (*.eml)";
    *nextFilter++ = "*.eml";
  }
	if (mask & eHTMLFiles)
	{
		*nextTitle++ = "HTML Files (*.htm; *.html)";
		*nextFilter++ = "*.htm; *.html";
	}
	if (mask & eXMLFiles)
	{
		*nextTitle++ = "XML Files (*.xml)";
		*nextFilter++ =  "*.xml";
	}
	if (mask & eImageFiles)
	{
		*nextTitle++ = "Image Files (*.gif; *.jpg; *.jpeg; *.png)";
		*nextFilter++ = "*.gif; *.jpg; *.jpeg; *.png";
	}
  if (mask & eTextFiles)
  {
    *nextTitle++ = "Text Files (*.txt)";
    *nextFilter++ = "*.txt";
  }
	if (mask & eExtraFilter)
	{
		*nextTitle++ = inExtraFilterTitle;
		*nextFilter++ = inExtraFilter;
	}
	if (mask & eAllFiles)
	{
		*nextTitle++ = "All Files";
		*nextFilter++ = "*.*";
	}

	fileWidget->SetFilterList(nextFilter - filters, titles, filters);

	return;

Clean:
	delete [] titles;
	delete [] filters;
}

//----------------------------------------------------------------------------------------
void nsFileSpecWithUIImpl::SetFileWidgetStartDir( nsIFileWidget *fileWidget ) {
    // We set the file widget's starting directory to the one specified by this nsIFileSpec.
    if ( mBaseFileSpec && fileWidget ) {
        nsFileSpec spec;
        nsresult rv = mBaseFileSpec->GetFileSpec( &spec );
        if ( NS_SUCCEEDED( rv ) && spec.Valid() ) {
            // Set as starting directory in file widget.
            fileWidget->SetDisplayDirectory( spec );
        }
    }
}


//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecWithUIImpl::ChooseInputFile(
  const char *inTitle,
  PRUint32 inMask,
  const char *inExtraFilterTitle, const char *inExtraFilter)
//----------------------------------------------------------------------------------------
{
  if (!mBaseFileSpec)
    return NS_ERROR_NULL_POINTER;
  nsresult rv = NS_OK;
  nsFileSpec spec;
  nsCOMPtr<nsIFileWidget> fileWidget; 
  rv = nsComponentManager::CreateInstance(kCFileWidgetCID,
                                          nsnull,
                                          nsCOMTypeInfo<nsIFileWidget>::GetIID(),
                                          (void**)getter_AddRefs(fileWidget));
  if (NS_FAILED(rv))
		return rv;

  SetFileWidgetFilterList(fileWidget, inMask, 
                          inExtraFilterTitle, inExtraFilter);
  SetFileWidgetStartDir(fileWidget);
  nsString winTitle(inTitle);
	if (fileWidget->GetFile(nsnull, winTitle, spec) != nsFileDlgResults_OK)
		rv = NS_FILE_FAILURE;
  else
    rv = mBaseFileSpec->SetFromFileSpec(spec);

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
    SetFileWidgetStartDir(fileWidget);
    nsFileSpec spec;
	if (fileWidget->GetFolder(nsnull, title, spec) != nsFileDlgResults_OK)
		rv = NS_FILE_FAILURE;

  rv = mBaseFileSpec->SetFromFileSpec(spec);
	if (NS_FAILED(rv))
		return rv;
    
  return GetURLString(_retval);
} // nsFileSpecWithUIImpl::chooseDirectory

