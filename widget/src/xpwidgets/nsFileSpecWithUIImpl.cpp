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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsFileSpecWithUIImpl.h"

#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"

#undef NS_FILE_FAILURE
#define NS_FILE_FAILURE NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES,(0xFFFF))

NS_IMPL_ISUPPORTS2(nsFileSpecWithUIImpl, nsIFileSpecWithUI, nsIFileSpec)

static NS_DEFINE_CID(kCFileWidgetCID, NS_FILEWIDGET_CID);

#include "nsIComponentManager.h"

// Get widget from DOM window.
// Note that this does not AddRef the resulting widget!
//
// This was cribbed from nsBaseFilePicker.cpp.  I will
// echo the comment that appears there: aaaarrrrrrgh!
static nsIWidget *parentWidget( nsIDOMWindowInternal *window ) {
  nsIWidget *result = 0;
  if ( window ) {
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface( window );
    if ( sgo ) {
      nsCOMPtr<nsIDocShell> docShell;
      sgo->GetDocShell( getter_AddRefs( docShell ) );
      if ( docShell ) {
        nsCOMPtr<nsIPresShell> presShell;
        docShell->GetPresShell( getter_AddRefs( presShell ) );
        if ( presShell ) {
          nsCOMPtr<nsIViewManager> viewManager;
          presShell->GetViewManager( getter_AddRefs( viewManager ) );
          if ( viewManager ) {
            nsIView *view; // GetRootView doesn't AddRef!
            viewManager->GetRootView( view );
            if ( view ) {
              nsCOMPtr<nsIWidget> widget;
              view->GetWidget( *getter_AddRefs( widget ) );
              if ( widget ) {
                  result = widget.get();
              }
            }
          }
        }
      }
    }
  }
  return result;
}

//----------------------------------------------------------------------------------------
nsFileSpecWithUIImpl::nsFileSpecWithUIImpl()
    : mParentWindow( 0 )
//----------------------------------------------------------------------------------------
{
    NS_INIT_REFCNT();
	nsresult rv = nsComponentManager::CreateInstance(
		(const char*)NS_FILESPEC_CONTRACTID,
		(nsISupports*)nsnull,
		(const nsID&)NS_GET_IID(nsIFileSpec),
		(void**)getter_AddRefs(mBaseFileSpec));
	NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a file spec.");
}

//----------------------------------------------------------------------------------------
nsFileSpecWithUIImpl::~nsFileSpecWithUIImpl()
//----------------------------------------------------------------------------------------
{
    // Release parent window.
    NS_IF_RELEASE( mParentWindow );
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
        NS_GET_IID(nsIFileWidget),
        (void**)getter_AddRefs(fileWidget));
    if (NS_FAILED(rv))
    	return rv;
    
    fileWidget->SetDefaultString(NS_ConvertASCIItoUCS2(suggestedLeafName));

    SetFileWidgetFilterList(fileWidget, outMask, nsnull, nsnull);

    // If there is a filespec specified, then start there.
    SetFileWidgetStartDir(fileWidget);

    nsFileSpec spec;
    
    nsString winTitle; winTitle.AssignWithConversion(windowTitle);

    nsFileDlgResults result = fileWidget->PutFile(parentWidget(mParentWindow), winTitle, spec);
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
		(nextTitle++)->AssignWithConversion("All Readable Files");
		(nextFilter++)->AssignWithConversion("*.eml; *.txt; *.htm; *.html; *.xml; *.gif; *.jpg; *.jpeg; *.png");
	}
  if (mask & eMailFiles)
  {
    (nextTitle++)->AssignWithConversion("Mail Files (*.eml)");
    (nextFilter++)->AssignWithConversion("*.eml");
  }
	if (mask & eHTMLFiles)
	{
		(nextTitle++)->AssignWithConversion("HTML Files (*.htm; *.html)");
		(nextFilter++)->AssignWithConversion("*.htm; *.html");
	}
	if (mask & eXMLFiles)
	{
		(nextTitle++)->AssignWithConversion("XML Files (*.xml)");
		(nextFilter++)->AssignWithConversion("*.xml");
	}
	if (mask & eImageFiles)
	{
		(nextTitle++)->AssignWithConversion("Image Files (*.gif; *.jpg; *.jpeg; *.png)");
		(nextFilter++)->AssignWithConversion("*.gif; *.jpg; *.jpeg; *.png");
	}
  if (mask & eTextFiles)
  {
    (nextTitle++)->AssignWithConversion("Text Files (*.txt)");
    (nextFilter++)->AssignWithConversion("*.txt");
  }
	if (mask & eExtraFilter)
	{
		(nextTitle++)->AssignWithConversion(inExtraFilterTitle);
		(nextFilter++)->AssignWithConversion(inExtraFilter);
	}
	if (mask & eAllFiles)
	{
		(nextTitle++)->AssignWithConversion("All Files");
		(nextFilter++)->AssignWithConversion("*.*");
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
                                          NS_GET_IID(nsIFileWidget),
                                          (void**)getter_AddRefs(fileWidget));
  if (NS_FAILED(rv))
		return rv;

  SetFileWidgetFilterList(fileWidget, inMask, 
                          inExtraFilterTitle, inExtraFilter);
  SetFileWidgetStartDir(fileWidget);
  nsString winTitle; winTitle.AssignWithConversion(inTitle);
	if (fileWidget->GetFile(parentWidget(mParentWindow), winTitle, spec) != nsFileDlgResults_OK)
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
        NS_GET_IID(nsIFileWidget),
        (void**)getter_AddRefs(fileWidget));
	if (NS_FAILED(rv))
		return rv;
    SetFileWidgetStartDir(fileWidget);
    nsFileSpec spec;
	if (fileWidget->GetFolder(nsnull, NS_ConvertASCIItoUCS2(title), spec) != nsFileDlgResults_OK)
		rv = NS_FILE_FAILURE;

  rv = mBaseFileSpec->SetFromFileSpec(spec);
	if (NS_FAILED(rv))
		return rv;
    
  return GetURLString(_retval);
} // nsFileSpecWithUIImpl::chooseDirectory

NS_IMETHODIMP
nsFileSpecWithUIImpl::GetParentWindow( nsIDOMWindowInternal **aResult ) {
    nsresult rv = NS_OK;

    if ( aResult ) {
        *aResult = mParentWindow;
        NS_IF_ADDREF( *aResult );
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsFileSpecWithUIImpl::SetParentWindow( nsIDOMWindowInternal *aParentWindow ) {
    nsresult rv = NS_OK;

    // Release current parent.
    NS_IF_RELEASE( mParentWindow );

    // Set new parent.
    mParentWindow = aParentWindow;
    NS_IF_ADDREF( mParentWindow );

    return rv;
}
