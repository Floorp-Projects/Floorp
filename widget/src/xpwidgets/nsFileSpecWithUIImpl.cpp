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

#include "nsIFileSpecWithUI.h" // Always first, to ensure that it compiles alone.

#include "nsFileSpecImpl.h"
#include "nsIFileWidget.h"
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"

#undef NS_FILE_FAILURE
#define NS_FILE_FAILURE NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES,(0xFFFF))

//========================================================================================
class nsFileSpecWithUIImpl
//========================================================================================
	: public nsFileSpecImpl
	, public nsIFileSpecWithUI
{

 public: 

	NS_DECL_ISUPPORTS_INHERITED
	
	nsFileSpecWithUIImpl();
	virtual ~nsFileSpecWithUIImpl();

	NS_IMETHOD chooseOutputFile(const char *windowTitle, const char *suggestedLeafName);

	NS_IMETHOD chooseInputFile(
		const char *title,
		nsIFileSpecWithUI::StandardFilterMask standardFilterMask,
		const char *extraFilterTitle, const char *extraFilter);

	NS_IMETHOD chooseDirectory(const char *title);
	
	//------------------
	// INHERITED/FORWARDED METHODS
	//------------------
	
	NS_IMETHOD fromFileSpec(const nsIFileSpec *original)
		{ return nsFileSpecImpl::fromFileSpec(original); }

	NS_IMETHOD GetURLString(char * *aURLString)
		{ return nsFileSpecImpl::GetURLString(aURLString); }
	NS_IMETHOD SetURLString(char * aURLString)
		{ return nsFileSpecImpl::SetURLString(aURLString); }

	/* attribute string UnixStyleFilePath; */
	NS_IMETHOD GetUnixStyleFilePath(char * *aUnixStyleFilePath)
		{ return nsFileSpecImpl::GetUnixStyleFilePath(aUnixStyleFilePath); }
	NS_IMETHOD SetUnixStyleFilePath(char * aUnixStyleFilePath)
		{ return nsFileSpecImpl::SetUnixStyleFilePath(aUnixStyleFilePath); }

	/* attribute string PersistentDescriptorString; */
	NS_IMETHOD GetPersistentDescriptorString(char * *aPersistentDescriptorString)
		{ return nsFileSpecImpl::GetPersistentDescriptorString(aPersistentDescriptorString); }
	NS_IMETHOD SetPersistentDescriptorString(char * aPersistentDescriptorString)
		{ return nsFileSpecImpl::SetPersistentDescriptorString(aPersistentDescriptorString); }

	/* attribute string NativePath; */
	NS_IMETHOD GetNativePath(char * *aNativePath)
		{ return nsFileSpecImpl::GetNativePath(aNativePath); }
	NS_IMETHOD SetNativePath(char * aNativePath)
		{ return nsFileSpecImpl::SetNativePath(aNativePath); }

	/* readonly attribute string NSPRPath; */
	NS_IMETHOD GetNSPRPath(char * *aNSPRPath)
		{ return nsFileSpecImpl::GetNSPRPath(aNSPRPath); }

	/* readonly attribute nsresult Error; */
	NS_IMETHOD error()
		{ return nsFileSpecImpl::error(); }

	/* boolean isValid (); */
	NS_IMETHOD isValid(PRBool *_retval)
		{ return nsFileSpecImpl::isValid(_retval); }

	/* boolean failed (); */
	NS_IMETHOD failed(PRBool *_retval)
		{ return nsFileSpecImpl::failed(_retval); }

	/* attribute string LeafName; */
	NS_IMETHOD GetLeafName(char * *aLeafName)
		{ return nsFileSpecImpl::GetLeafName(aLeafName); }
	NS_IMETHOD SetLeafName(char * aLeafName)
		{ return nsFileSpecImpl::SetLeafName(aLeafName); }

	/* readonly attribute nsIFileSpec Parent; */
	NS_IMETHOD GetParent(nsIFileSpec * *aParent)
		{ return nsFileSpecImpl::GetParent(aParent); }

	/* nsIFileSpec makeUnique (); */
	NS_IMETHOD makeUnique()
		{ return nsFileSpecImpl::makeUnique(); }

	/* nsIFileSpec makeUniqueWithSuggestedName (in string suggestedName); */
	NS_IMETHOD makeUniqueWithSuggestedName(const char* inSuggestedLeafName)
		{ return nsFileSpecImpl::makeUniqueWithSuggestedName(inSuggestedLeafName); }

	/* readonly attribute unsigned long ModDate; */
	NS_IMETHOD GetModDate(PRUint32 *aModDate)
		{ return nsFileSpecImpl::GetModDate(aModDate); }

	/* boolean modDateChanged (in unsigned long oldStamp); */
	NS_IMETHOD modDateChanged(PRUint32 oldStamp, PRBool *_retval)
		{ return nsFileSpecImpl::modDateChanged(oldStamp, _retval); }

	/* boolean isDirectory (); */
	NS_IMETHOD isDirectory(PRBool *_retval)
		{ return nsFileSpecImpl::isDirectory(_retval); }

	/* boolean isFile (); */
	NS_IMETHOD isFile(PRBool *_retval)
		{ return nsFileSpecImpl::isFile(_retval); }

	/* boolean exists (); */
	NS_IMETHOD exists(PRBool *_retval)
		{ return nsFileSpecImpl::exists(_retval); }

	/* readonly attribute unsigned long FileSize; */
	NS_IMETHOD GetFileSize(PRUint32 *aFileSize)
		{ return nsFileSpecImpl::GetFileSize(aFileSize); }

	/* readonly attribute unsigned long DiskSpaceAvailable; */
	NS_IMETHOD GetDiskSpaceAvailable(PRUint32 *aDiskSpaceAvailable)
		{ return nsFileSpecImpl::GetDiskSpaceAvailable(aDiskSpaceAvailable); }

	/* nsIFileSpec AppendRelativeUnixPath (in string relativePath); */
	NS_IMETHOD AppendRelativeUnixPath(const char *relativePath)
		{ return nsFileSpecImpl::AppendRelativeUnixPath(relativePath); }

	/* void createDir (); */
	NS_IMETHOD createDir()
		{ return nsFileSpecImpl::createDir(); }

	/* void rename ([const] in string newLeafName); */
	NS_IMETHOD rename(const char *newLeafName)
		{ return nsFileSpecImpl::rename(newLeafName); }

	/* void copyToDir ([const] in nsIFileSpec newParentDir); */
	NS_IMETHOD copyToDir(const nsIFileSpec *newParentDir)
		{ return nsFileSpecImpl::copyToDir(newParentDir); }

	/* void moveToDir ([const] in nsIFileSpec newParentDir); */
	NS_IMETHOD moveToDir(const nsIFileSpec *newParentDir)
		{ return nsFileSpecImpl::moveToDir(newParentDir); }

	/* void execute ([const] in string args); */
	NS_IMETHOD execute(const char *args)
		{ return nsFileSpecImpl::execute(args); }

	/* void openStreamForReading (); */
	NS_IMETHOD openStreamForReading()
		{ return nsFileSpecImpl::openStreamForReading(); }

	/* void openStreamForWriting (); */
	NS_IMETHOD openStreamForWriting()
		{ return nsFileSpecImpl::openStreamForWriting(); }

	/* void openStreamForReadingAndWriting (); */
	NS_IMETHOD openStreamForReadingAndWriting()
		{ return nsFileSpecImpl::openStreamForReadingAndWriting(); }

	/* void close (); */
	NS_IMETHOD closeStream()
		{ return nsFileSpecImpl::closeStream(); }

	/* boolean isOpen (); */
	NS_IMETHOD isStreamOpen(PRBool *_retval)
		{ return nsFileSpecImpl::isStreamOpen(_retval); }

	/* boolean eof (); */
	NS_IMETHOD eof(PRBool *_retval)
		{ return nsFileSpecImpl::eof(_retval); }

	NS_IMETHOD read(char** buffer, PRInt32 requestedCount, PRInt32 *_retval)
		{ return nsFileSpecImpl::read(buffer, requestedCount, _retval); }

	NS_IMETHOD readLine(char** line, PRInt32 bufferSize, PRBool *wasTruncated)
		{ return nsFileSpecImpl::readLine(line, bufferSize, wasTruncated); }
					// Check eof() before each call.
					// CAUTION: false result only indicates line was truncated
					// to fit buffer, or an error occurred (OTHER THAN eof).


	NS_IMETHOD write(const char* data, PRInt32 requestedCount, PRInt32 *_retval)
		{ return nsFileSpecImpl::write(data, requestedCount, _retval); }

	/* void flush (); */
	NS_IMETHOD flush()
		{ return nsFileSpecImpl::flush(); }

	/* void seek (in long offset); */
	NS_IMETHOD seek(PRInt32 offset)
		{ return nsFileSpecImpl::seek(offset); }

	/* long tell (); */
	NS_IMETHOD tell(PRInt32 *_retval)
		{ return nsFileSpecImpl::tell(_retval); }

	/* void endline (); */
	NS_IMETHOD endline()
		{ return nsFileSpecImpl::endline(); }


}; // class nsFileSpecWithUIImpl

NS_IMPL_ISUPPORTS_INHERITED(nsFileSpecWithUIImpl, nsFileSpecImpl, nsIFileSpecWithUI)

static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);

#include "nsIComponentManager.h"

//----------------------------------------------------------------------------------------
nsFileSpecWithUIImpl::nsFileSpecWithUIImpl()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
nsFileSpecWithUIImpl::~nsFileSpecWithUIImpl()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecWithUIImpl::chooseOutputFile(
	const char *windowTitle,
	const char *suggestedLeafName)
//----------------------------------------------------------------------------------------
{
    nsCOMPtr<nsIFileWidget> fileWidget; 
    nsresult rv = nsComponentManager::CreateInstance(
    	kCFileWidgetCID,
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
} // nsFileSpecImpl::chooseOutputFile

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecWithUIImpl::chooseInputFile(
		const char *inTitle,
		nsIFileSpecWithUI::StandardFilterMask inMask,
		const char *inExtraFilterTitle, const char *inExtraFilter)
//----------------------------------------------------------------------------------------
{
	nsresult rv = NS_OK;
	nsString* nextTitle;
	nsString* nextFilter;
    nsCOMPtr<nsIFileWidget> fileWidget; 
    rv = nsComponentManager::CreateInstance(
    	kCFileWidgetCID,
        nsnull,
        nsIFileWidget::GetIID(),
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
	if (fileWidget->GetFile(nsnull, inTitle, mFileSpec) != nsFileDlgResults_OK)
		rv = NS_FILE_FAILURE;

Clean:
	delete [] titles;
	delete [] filters;
	return rv;
} // nsFileSpecWithUIImpl::chooseInputFile

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecWithUIImpl::chooseDirectory(const char *title)
//----------------------------------------------------------------------------------------
{
    nsCOMPtr<nsIFileWidget> fileWidget;
    nsresult rv = nsComponentManager::CreateInstance(
    	kCFileWidgetCID,
        nsnull,
        nsIFileWidget::GetIID(),
        (void**)getter_AddRefs(fileWidget));
	if (NS_FAILED(rv))
		return rv;
	if (fileWidget->GetFolder(nsnull, title, mFileSpec) != nsFileDlgResults_OK)
		rv = NS_FILE_FAILURE;
	return NS_OK;
} // nsFileSpecWithUIImpl::chooseDirectory

//----------------------------------------------------------------------------------------
nsresult NS_NewFileSpecWithUI(nsIFileSpecWithUI** result)
//----------------------------------------------------------------------------------------
{
	if (!result)
		return NS_ERROR_NULL_POINTER;
	nsFileSpecWithUIImpl* it = new nsFileSpecWithUIImpl;
	if (!it)
		return NS_ERROR_OUT_OF_MEMORY;
	return it->QueryInterface(nsIFileSpecWithUI::GetIID(), (void **) result);
}

