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

#include "nsIFileSpec.h" // Always first, to ensure that it compiles alone.

#include "nsFileSpec.h"
#include "nsIFileStream.h"
#include "nsFileStream.h"

#include "nsIFileWidget.h"
#include "nsWidgetsCID.h"

static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);

#include "nsIComponentManager.h"

#include "prmem.h"

#ifdef NS_DEBUG
#define TEST_OUT_PTR(p) \
	if (!(p)) \
		return NS_ERROR_NULL_POINTER;
#else
#define TEST_OUT_PTR(p)
#endif
//========================================================================================
class nsFileSpecImpl
//========================================================================================
	: public nsIFileSpec
{

 public: 

	NS_DECL_ISUPPORTS

	NS_IMETHOD fromFileSpec(const nsIFileSpec *original);
	NS_IMETHOD chooseOutputFile(const char *windowTitle, const char *suggestedLeafName);

	NS_IMETHOD chooseInputFile(
		const char *title,
		nsIFileSpec::StandardFilterMask standardFilterMask,
		const char *extraFilterTitle, const char *extraFilter);

	NS_IMETHOD chooseDirectory(const char *title);

	NS_IMETHOD GetURLString(char * *aURLString);
	NS_IMETHOD SetURLString(char * aURLString);

	/* attribute string UnixStyleFilePath; */
	NS_IMETHOD GetUnixStyleFilePath(char * *aUnixStyleFilePath);
	NS_IMETHOD SetUnixStyleFilePath(char * aUnixStyleFilePath);

	/* attribute string PersistentDescriptorString; */
	NS_IMETHOD GetPersistentDescriptorString(char * *aPersistentDescriptorString);
	NS_IMETHOD SetPersistentDescriptorString(char * aPersistentDescriptorString);

	/* attribute string NativePath; */
	NS_IMETHOD GetNativePath(char * *aNativePath);
	NS_IMETHOD SetNativePath(char * aNativePath);

	/* readonly attribute string NSPRPath; */
	NS_IMETHOD GetNSPRPath(char * *aNSPRPath);

	/* readonly attribute nsresult Error; */
	NS_IMETHOD error();

	/* boolean isValid (); */
	NS_IMETHOD isValid(PRBool *_retval);

	/* boolean failed (); */
	NS_IMETHOD failed(PRBool *_retval);

	/* attribute string LeafName; */
	NS_IMETHOD GetLeafName(char * *aLeafName);
	NS_IMETHOD SetLeafName(char * aLeafName);

	/* readonly attribute nsIFileSpec Parent; */
	NS_IMETHOD GetParent(nsIFileSpec * *aParent);

	/* nsIFileSpec makeUnique (); */
	NS_IMETHOD makeUnique();

	/* nsIFileSpec makeUniqueWithSuggestedName (in string suggestedName); */
	NS_IMETHOD makeUniqueWithSuggestedName(const char* inSuggestedLeafName);

	/* readonly attribute unsigned long ModDate; */
	NS_IMETHOD GetModDate(PRUint32 *aModDate);

	/* boolean modDateChanged (in unsigned long oldStamp); */
	NS_IMETHOD modDateChanged(PRUint32 oldStamp, PRBool *_retval);

	/* boolean isDirectory (); */
	NS_IMETHOD isDirectory(PRBool *_retval);

	/* boolean isFile (); */
	NS_IMETHOD isFile(PRBool *_retval);

	/* boolean exists (); */
	NS_IMETHOD exists(PRBool *_retval);

	/* readonly attribute unsigned long FileSize; */
	NS_IMETHOD GetFileSize(PRUint32 *aFileSize);

	/* readonly attribute unsigned long DiskSpaceAvailable; */
	NS_IMETHOD GetDiskSpaceAvailable(PRUint32 *aDiskSpaceAvailable);

	/* nsIFileSpec AppendRelativeUnixPath (in string relativePath); */
	NS_IMETHOD AppendRelativeUnixPath(const char *relativePath);

	/* void createDir (); */
	NS_IMETHOD createDir();

	/* void rename ([const] in string newLeafName); */
	NS_IMETHOD rename(const char *newLeafName);

	/* void copyToDir ([const] in nsIFileSpec newParentDir); */
	NS_IMETHOD copyToDir(const nsIFileSpec *newParentDir);

	/* void moveToDir ([const] in nsIFileSpec newParentDir); */
	NS_IMETHOD moveToDir(const nsIFileSpec *newParentDir);

	/* void execute ([const] in string args); */
	NS_IMETHOD execute(const char *args);

	/* void openStreamForReading (); */
	NS_IMETHOD openStreamForReading();

	/* void openStreamForWriting (); */
	NS_IMETHOD openStreamForWriting();

	/* void openStreamForReadingAndWriting (); */
	NS_IMETHOD openStreamForReadingAndWriting();

	/* void close (); */
	NS_IMETHOD closeStream();

	/* boolean isOpen (); */
	NS_IMETHOD isStreamOpen(PRBool *_retval);

	/* boolean eof (); */
	NS_IMETHOD eof(PRBool *_retval);

	NS_IMETHOD read(char** buffer, PRInt32 requestedCount, PRInt32 *_retval);

	NS_IMETHOD readLine(char** line, PRInt32 bufferSize, PRBool *wasTruncated);
					// Check eof() before each call.
					// CAUTION: false result only indicates line was truncated
					// to fit buffer, or an error occurred (OTHER THAN eof).


	NS_IMETHOD write(const char* data, PRInt32 requestedCount, PRInt32 *_retval);

	/* void flush (); */
	NS_IMETHOD flush();

	/* void seek (in long offset); */
	NS_IMETHOD seek(PRInt32 offset);

	/* long tell (); */
	NS_IMETHOD tell(PRInt32 *_retval);

	/* void endline (); */
	NS_IMETHOD endline();

	//----------------------
	// Implementation
	//----------------------

			nsFileSpecImpl();
			nsFileSpecImpl(const nsFileSpec& inSpec);
			virtual ~nsFileSpecImpl();
			static nsresult MakeInterface(const nsFileSpec& inSpec, nsIFileSpec** outSpec);

	//----------------------
	// Data
	//----------------------
	
		nsFileSpec							mFileSpec;
		nsIInputStream*						mInputStream;
		nsIOutputStream*					mOutputStream;

}; // class nsFileSpecImpl

//static NS_DEFINE_IID(kIFileSpecIID, NS_IFILESPEC_IID);
NS_IMPL_ISUPPORTS(nsFileSpecImpl, nsIFileSpec::GetIID())

//----------------------------------------------------------------------------------------
nsFileSpecImpl::nsFileSpecImpl()
//----------------------------------------------------------------------------------------
	:	mInputStream(nsnull)
	,	mOutputStream(nsnull)
{
	NS_INIT_ISUPPORTS();
}

//----------------------------------------------------------------------------------------
nsFileSpecImpl::nsFileSpecImpl(const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
	:	mFileSpec(inSpec)
	,	mInputStream(nsnull)
	,	mOutputStream(nsnull)
{
	NS_INIT_ISUPPORTS();
}

//----------------------------------------------------------------------------------------
nsFileSpecImpl::~nsFileSpecImpl()
//----------------------------------------------------------------------------------------
{
	closeStream();
}

//----------------------------------------------------------------------------------------
/* static */
nsresult nsFileSpecImpl::MakeInterface(const nsFileSpec& inSpec, nsIFileSpec** result)
//----------------------------------------------------------------------------------------
{
	nsFileSpecImpl* it = new nsFileSpecImpl(inSpec);
	if (!it)
		return NS_ERROR_OUT_OF_MEMORY;
	return it->QueryInterface(nsIFileSpec::GetIID(), (void **) result);
} // nsFileSpecImpl::MakeInterface

#define FILESPEC(ifilespec) ((nsFileSpecImpl*)ifilespec)->mFileSpec

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::fromFileSpec(const nsIFileSpec *original)
//----------------------------------------------------------------------------------------
{
	mFileSpec = FILESPEC(original);
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::chooseOutputFile(
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
NS_IMETHODIMP nsFileSpecImpl::chooseInputFile(
		const char *inTitle,
		nsIFileSpec::StandardFilterMask inMask,
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
} // nsFileSpecImpl::chooseInputFile

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::chooseDirectory(const char *title)
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
} // nsFileSpecImpl::chooseDirectory

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetURLString(char * *aURLString)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aURLString)
	if (mFileSpec.Failed())
		return mFileSpec.Error();
	nsFileURL url(mFileSpec);
	*aURLString = nsCRT::strdup(url.GetURLString());
	if (!*aURLString)
		return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
} // nsFileSpecImpl::GetURLString

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::SetURLString(char * aURLString)
//----------------------------------------------------------------------------------------
{
	mFileSpec = nsFileURL(aURLString);
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetUnixStyleFilePath(char * *aUnixStyleFilePath)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aUnixStyleFilePath)
	if (mFileSpec.Failed())
		return mFileSpec.Error();
	nsFilePath path(mFileSpec);
	*aUnixStyleFilePath = nsCRT::strdup((const char*) path);
	if (!*aUnixStyleFilePath)
		return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::SetUnixStyleFilePath(char * aUnixStyleFilePath)
//----------------------------------------------------------------------------------------
{
	mFileSpec = nsFilePath(aUnixStyleFilePath);
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetPersistentDescriptorString(char * *aPersistentDescriptorString)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aPersistentDescriptorString)
	if (mFileSpec.Failed())
		return mFileSpec.Error();
	nsPersistentFileDescriptor desc(mFileSpec);
	nsSimpleCharString data;
	desc.GetData(data);
	*aPersistentDescriptorString = nsCRT::strdup((const char*) data);
	if (!*aPersistentDescriptorString)
		return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::SetPersistentDescriptorString(char * aPersistentDescriptorString)
//----------------------------------------------------------------------------------------
{
	nsPersistentFileDescriptor desc(mFileSpec);
	desc.SetData(aPersistentDescriptorString);
	mFileSpec = desc;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetNativePath(char * *aNativePath)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aNativePath)
	if (mFileSpec.Failed())
		return mFileSpec.Error();
	*aNativePath = nsCRT::strdup(mFileSpec.GetNativePathCString());
	if (!*aNativePath)
		return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::SetNativePath(char * aNativePath)
//----------------------------------------------------------------------------------------
{
	mFileSpec = aNativePath;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetNSPRPath(char * *aNSPRPath)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aNSPRPath)
	if (mFileSpec.Failed())
		return mFileSpec.Error();
	nsNSPRPath path(mFileSpec);
	*aNSPRPath = nsCRT::strdup((const char*) path);
	if (!*aNSPRPath)
		return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::error()
//----------------------------------------------------------------------------------------
{
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::isValid(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.Valid();
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::failed(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	*_retval = mFileSpec.Failed();
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetLeafName(char * *aLeafName)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aLeafName)
	*aLeafName = mFileSpec.GetLeafName();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::SetLeafName(char * aLeafName)
//----------------------------------------------------------------------------------------
{
	mFileSpec.SetLeafName(aLeafName);
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetParent(nsIFileSpec * *aParent)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aParent)
	nsFileSpec parent;
	mFileSpec.GetParent(parent);
	return MakeInterface(parent, aParent);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::makeUnique()
//----------------------------------------------------------------------------------------
{
	mFileSpec.MakeUnique();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::makeUniqueWithSuggestedName(const char *suggestedName)
//----------------------------------------------------------------------------------------
{
	mFileSpec.MakeUnique(suggestedName);
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetModDate(PRUint32 *aModDate)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aModDate)
	nsFileSpec::TimeStamp stamp;
	mFileSpec.GetModDate(stamp);
	*aModDate = stamp;
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::modDateChanged(PRUint32 oldStamp, PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.ModDateChanged(oldStamp);
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::isDirectory(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.IsDirectory();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::isFile(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.IsFile();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::exists(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.Exists();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetFileSize(PRUint32 *aFileSize)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aFileSize)
	*aFileSize = mFileSpec.GetFileSize();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetDiskSpaceAvailable(PRUint32 *aDiskSpaceAvailable)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aDiskSpaceAvailable)
	*aDiskSpaceAvailable = mFileSpec.GetDiskSpaceAvailable();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::AppendRelativeUnixPath(const char *relativePath)
//----------------------------------------------------------------------------------------
{
	mFileSpec += relativePath;
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::createDir()
//----------------------------------------------------------------------------------------
{
	mFileSpec.CreateDir();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::rename(const char *newLeafName)
//----------------------------------------------------------------------------------------
{
	return mFileSpec.Rename(newLeafName);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::copyToDir(const nsIFileSpec *newParentDir)
//----------------------------------------------------------------------------------------
{
	return mFileSpec.Copy(FILESPEC(newParentDir));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::moveToDir(const nsIFileSpec *newParentDir)
//----------------------------------------------------------------------------------------
{
	return mFileSpec.Move(FILESPEC(newParentDir));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::execute(const char *args)
//----------------------------------------------------------------------------------------
{
	return mFileSpec.Execute(args);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::openStreamForReading()
//----------------------------------------------------------------------------------------
{
	if (mInputStream || mOutputStream)
		return NS_ERROR_FAILURE;
	return NS_NewTypicalInputFileStream((nsISupports**)&mInputStream, mFileSpec);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::openStreamForWriting()
//----------------------------------------------------------------------------------------
{
	if (mInputStream || mOutputStream)
		return NS_ERROR_FAILURE;
	return NS_NewTypicalOutputFileStream((nsISupports**)&mOutputStream, mFileSpec);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::openStreamForReadingAndWriting()
//----------------------------------------------------------------------------------------
{
	if (mInputStream || mOutputStream)
		return NS_ERROR_FAILURE;
	nsresult result = NS_NewTypicalInputFileStream((nsISupports**)&mInputStream, mFileSpec);
	if (NS_SUCCEEDED(result))
		result = NS_NewTypicalOutputFileStream((nsISupports**)&mOutputStream, mFileSpec);
	return result;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::closeStream()
//----------------------------------------------------------------------------------------
{
	NS_IF_RELEASE(mInputStream);
	NS_IF_RELEASE(mOutputStream);
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::isStreamOpen(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = (mInputStream || mOutputStream);
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::eof(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	if (!mInputStream)
		return NS_ERROR_NULL_POINTER;
	nsInputFileStream s(mInputStream);
	*_retval = s.eof();
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::read(char** buffer, PRInt32 requestedCount, PRInt32 *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	TEST_OUT_PTR(buffer)
	if (!*buffer)
		*buffer = (char*)PR_Malloc(requestedCount + 1);
	if (!mInputStream)
		return NS_ERROR_NULL_POINTER;
	nsInputFileStream s(mInputStream);
	*_retval = s.read(*buffer, requestedCount);
	return s.error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::readLine(char** line, PRInt32 bufferSize, PRBool *wasTruncated)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(wasTruncated)
	TEST_OUT_PTR(line)
	if (!*line)
		*line = (char*)PR_Malloc(bufferSize + 1);
	if (!mInputStream)
		return NS_ERROR_NULL_POINTER;
	nsInputFileStream s(mInputStream);
	*wasTruncated = s.readline(*line, bufferSize);
	return s.error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::write(const char * data, PRInt32 requestedCount, PRInt32 *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	if (!mOutputStream)
		return NS_ERROR_NULL_POINTER;
	nsOutputFileStream s(mOutputStream);
	*_retval = s.write(data, requestedCount);
	return s.error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::flush()
//----------------------------------------------------------------------------------------
{
	if (!mOutputStream)
		return NS_ERROR_NULL_POINTER;
	nsOutputFileStream s(mOutputStream);
	s.flush();
	return s.error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::seek(PRInt32 offset)
//----------------------------------------------------------------------------------------
{
	nsresult result = NS_OK;
	if (mOutputStream)
	{
		nsOutputFileStream os(mOutputStream);
		os.seek(offset);
		result = os.error();
	}
	if (NS_SUCCEEDED(result) && mInputStream)
	{
		nsInputFileStream is(mInputStream);
		is.seek(offset);
		result = is.error();
	}
	return result;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::tell(PRInt32 *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	if (!mInputStream)
		return NS_ERROR_NULL_POINTER;
	nsInputFileStream s(mInputStream);
	*_retval = s.tell();
	return s.error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::endline()
//----------------------------------------------------------------------------------------
{
	nsOutputFileStream s(mOutputStream);
	s << nsEndl;
	return s.error();
}

//========================================================================================
class nsDirectoryIteratorImpl
//========================================================================================
	: public nsIDirectoryIterator
{

public:

	nsDirectoryIteratorImpl();
	virtual ~nsDirectoryIteratorImpl();

	NS_DECL_ISUPPORTS

	NS_IMETHOD Init(nsIFileSpec *parent);

	NS_IMETHOD exists(PRBool *_retval);

	NS_IMETHOD next();

	NS_IMETHOD GetCurrentSpec(nsIFileSpec * *aCurrentSpec);

protected:

	nsDirectoryIterator*					mDirectoryIterator;
}; // class nsDirectoryIteratorImpl

static NS_DEFINE_IID(kIDirectoryIteratorIID, NS_IDIRECTORYITERATOR_IID);
NS_IMPL_ISUPPORTS(nsDirectoryIteratorImpl, kIDirectoryIteratorIID)

//----------------------------------------------------------------------------------------
nsDirectoryIteratorImpl::nsDirectoryIteratorImpl()
//----------------------------------------------------------------------------------------
	: mDirectoryIterator(nsnull)
{
	NS_INIT_ISUPPORTS();
}

//----------------------------------------------------------------------------------------
nsDirectoryIteratorImpl::~nsDirectoryIteratorImpl()
//----------------------------------------------------------------------------------------
{
	delete mDirectoryIterator;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsDirectoryIteratorImpl::Init(nsIFileSpec *parent)
//----------------------------------------------------------------------------------------
{
	delete mDirectoryIterator;
	mDirectoryIterator = new nsDirectoryIterator(FILESPEC(parent));
	if (!mDirectoryIterator)
		return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsDirectoryIteratorImpl::exists(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	if (!mDirectoryIterator)
		return NS_ERROR_NULL_POINTER;
	*_retval = mDirectoryIterator->Exists();
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsDirectoryIteratorImpl::next()
//----------------------------------------------------------------------------------------
{
	if (!mDirectoryIterator)
		return NS_ERROR_NULL_POINTER;
	(*mDirectoryIterator)++;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsDirectoryIteratorImpl::GetCurrentSpec(nsIFileSpec * *aCurrentSpec)
//----------------------------------------------------------------------------------------
{
	if (!mDirectoryIterator)
		return NS_ERROR_NULL_POINTER;
	return nsFileSpecImpl::MakeInterface(mDirectoryIterator->Spec(), aCurrentSpec);
}

//----------------------------------------------------------------------------------------
nsresult NS_NewFileSpec(nsIFileSpec** result)
//----------------------------------------------------------------------------------------
{
	if (!result)
		return NS_ERROR_NULL_POINTER;
	nsFileSpecImpl* it = new nsFileSpecImpl;
	if (!it)
		return NS_ERROR_OUT_OF_MEMORY;
	return it->QueryInterface(nsIFileSpec::GetIID(), (void **) result);
}

//----------------------------------------------------------------------------------------
nsresult NS_NewDirectoryIterator(nsIDirectoryIterator** result)
//----------------------------------------------------------------------------------------
{
	if (!result)
		return NS_ERROR_NULL_POINTER;
	nsDirectoryIteratorImpl* it = new nsDirectoryIteratorImpl();
	if (!it)
		return NS_ERROR_OUT_OF_MEMORY;
	return it->QueryInterface(kIDirectoryIteratorIID, (void **) result);
}
