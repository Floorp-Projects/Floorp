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

#include "nsFileSpecImpl.h"// Always first, to ensure that it compiles alone.

#include "nsIFileStream.h"
#include "nsFileStream.h"

#include "prmem.h"

NS_IMPL_ISUPPORTS1(nsFileSpecImpl, nsIFileSpec)

#ifdef NS_DEBUG
#define TEST_OUT_PTR(p) \
	if (!(p)) \
		return NS_ERROR_NULL_POINTER;
#else
#define TEST_OUT_PTR(p)
#endif

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
	CloseStream();
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
NS_IMETHODIMP nsFileSpecImpl::FromFileSpec(const nsIFileSpec *original)
//----------------------------------------------------------------------------------------
{
	mFileSpec = FILESPEC(original);
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::IsChildOf(nsIFileSpec *possibleParent,
                                        PRBool *_retval)
{
  *_retval = mFileSpec.IsChildOf(FILESPEC(possibleParent));
  return mFileSpec.Error();
}
//----------------------------------------------------------------------------------------

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
NS_IMETHODIMP nsFileSpecImpl::Error()
//----------------------------------------------------------------------------------------
{
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::IsValid(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.Valid();
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::Failed(PRBool *_retval)
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
NS_IMETHODIMP nsFileSpecImpl::MakeUnique()
//----------------------------------------------------------------------------------------
{
	mFileSpec.MakeUnique();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::MakeUniqueWithSuggestedName(const char *suggestedName)
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
NS_IMETHODIMP nsFileSpecImpl::ModDateChanged(PRUint32 oldStamp, PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.ModDateChanged(oldStamp);
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::IsDirectory(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.IsDirectory();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::IsFile(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.IsFile();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::Exists(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.Exists();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::IsHidden(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.IsHidden();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::IsSymlink(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = mFileSpec.IsSymlink();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::ResolveSymlink()
//----------------------------------------------------------------------------------------
{
    PRBool ignore;
	return mFileSpec.ResolveSymlink(ignore);
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
NS_IMETHODIMP nsFileSpecImpl::Touch()
//----------------------------------------------------------------------------------------
{
	// create an empty file, like the UNIX touch command.
	nsresult rv;
	rv = OpenStreamForReadingAndWriting();
	if (NS_FAILED(rv)) return rv;
	rv = CloseStream();
	return rv;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::CreateDir()
//----------------------------------------------------------------------------------------
{
	mFileSpec.CreateDir();
	return mFileSpec.Error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::Rename(const char *newLeafName)
//----------------------------------------------------------------------------------------
{
	return mFileSpec.Rename(newLeafName);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::CopyToDir(const nsIFileSpec *newParentDir)
//----------------------------------------------------------------------------------------
{
	return mFileSpec.Copy(FILESPEC(newParentDir));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::MoveToDir(const nsIFileSpec *newParentDir)
//----------------------------------------------------------------------------------------
{
	return mFileSpec.Move(FILESPEC(newParentDir));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::Execute(const char *args)
//----------------------------------------------------------------------------------------
{
	return mFileSpec.Execute(args);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::OpenStreamForReading()
//----------------------------------------------------------------------------------------
{
	if (mInputStream || mOutputStream)
		return NS_ERROR_FAILURE;
	return NS_NewTypicalInputFileStream((nsISupports**)&mInputStream, mFileSpec);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::OpenStreamForWriting()
//----------------------------------------------------------------------------------------
{
	if (mInputStream || mOutputStream)
		return NS_ERROR_FAILURE;
	return NS_NewTypicalOutputFileStream((nsISupports**)&mOutputStream, mFileSpec);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::OpenStreamForReadingAndWriting()
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
NS_IMETHODIMP nsFileSpecImpl::CloseStream()
//----------------------------------------------------------------------------------------
{
	NS_IF_RELEASE(mInputStream);
	NS_IF_RELEASE(mOutputStream);
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::IsStreamOpen(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = (mInputStream || mOutputStream);
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetInputStream(nsIInputStream** _retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	if (!mInputStream) {
		nsresult rv = OpenStreamForReading();
		if (NS_FAILED(rv)) return rv;
	}
	*_retval = mInputStream;
	NS_IF_ADDREF(mInputStream);
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetOutputStream(nsIOutputStream** _retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	if (!mOutputStream) {
		nsresult rv = OpenStreamForWriting();
		if (NS_FAILED(rv)) return rv;
	}
	*_retval = mOutputStream;
	NS_IF_ADDREF(mOutputStream);
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::SetFileContents(char* inString)
//----------------------------------------------------------------------------------------
{
	nsresult rv = OpenStreamForWriting();
	if (NS_FAILED(rv)) return rv;
	PRInt32 count;
	rv = Write(inString, PL_strlen(inString), &count);
	nsresult rv2 = CloseStream();
	return NS_FAILED(rv) ? rv : rv2;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetFileContents(char** _retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	*_retval = nsnull;
	nsresult rv = OpenStreamForReading();
	if (NS_FAILED(rv)) return rv;
	PRInt32 theSize;
	rv = GetFileSize((PRUint32*)&theSize);
	if (NS_SUCCEEDED(rv))
		rv = Read(_retval, theSize, &theSize);
	if (NS_SUCCEEDED(rv))
		(*_retval)[theSize] = 0;
	nsresult rv2 = CloseStream();
	return NS_FAILED(rv) ? rv : rv2;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::GetFileSpec(nsFileSpec *aFileSpec)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(aFileSpec)
	*aFileSpec = mFileSpec;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::SetFromFileSpec(const nsFileSpec& aFileSpec)
//----------------------------------------------------------------------------------------
{
	mFileSpec = aFileSpec;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::Eof(PRBool *_retval)
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
NS_IMETHODIMP nsFileSpecImpl::Read(char** buffer, PRInt32 requestedCount, PRInt32 *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	TEST_OUT_PTR(buffer)
	if (!mInputStream) {
		nsresult rv = OpenStreamForReading();
		if (NS_FAILED(rv)) return rv;
	}
	if (!*buffer)
		*buffer = (char*)PR_Malloc(requestedCount + 1);
	if (!mInputStream)
		return NS_ERROR_NULL_POINTER;
	nsInputFileStream s(mInputStream);
	*_retval = s.read(*buffer, requestedCount);
	return s.error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::ReadLine(char** line, PRInt32 bufferSize, PRBool *wasTruncated)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(wasTruncated)
	TEST_OUT_PTR(line)
	if (!mInputStream) {
		nsresult rv = OpenStreamForReading();
		if (NS_FAILED(rv)) return rv;
	}
	if (!*line)
		*line = (char*)PR_Malloc(bufferSize + 1);
	if (!mInputStream)
		return NS_ERROR_NULL_POINTER;
	nsInputFileStream s(mInputStream);
	*wasTruncated = !s.readline(*line, bufferSize);
	return s.error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::Write(const char * data, PRInt32 requestedCount, PRInt32 *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	//if (!mOutputStream)
	//	return NS_ERROR_NULL_POINTER;
	if (!mOutputStream) {
		nsresult rv = OpenStreamForWriting();
		if (NS_FAILED(rv))
			return rv;
	}
	nsOutputFileStream s(mOutputStream);
	*_retval = s.write(data, requestedCount);
	return s.error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::Flush()
//----------------------------------------------------------------------------------------
{
	if (!mOutputStream)
		return NS_ERROR_NULL_POINTER;
	nsOutputFileStream s(mOutputStream);
	s.flush();
	return s.error();
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileSpecImpl::Seek(PRInt32 offset)
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
NS_IMETHODIMP nsFileSpecImpl::Tell(PRInt32 *_retval)
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
NS_IMETHODIMP nsFileSpecImpl::EndLine()
//----------------------------------------------------------------------------------------
{
	nsOutputFileStream s(mOutputStream);
	s << nsEndl;
	return s.error();
}

NS_IMPL_ISUPPORTS1(nsDirectoryIteratorImpl, nsIDirectoryIterator)

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
NS_IMETHODIMP nsDirectoryIteratorImpl::Init(nsIFileSpec *parent, PRBool resolveSymlink)
//----------------------------------------------------------------------------------------
{
	delete mDirectoryIterator;
	mDirectoryIterator = new nsDirectoryIterator(FILESPEC(parent), resolveSymlink);
	if (!mDirectoryIterator)
		return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsDirectoryIteratorImpl::Exists(PRBool *_retval)
//----------------------------------------------------------------------------------------
{
	TEST_OUT_PTR(_retval)
	if (!mDirectoryIterator)
		return NS_ERROR_NULL_POINTER;
	*_retval = mDirectoryIterator->Exists();
	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsDirectoryIteratorImpl::Next()
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
NS_METHOD nsDirectoryIteratorImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aIFileSpec)
//----------------------------------------------------------------------------------------
{
  if (aIFileSpec == NULL)
    return NS_ERROR_NULL_POINTER;

	nsDirectoryIteratorImpl* it = new nsDirectoryIteratorImpl;
  if (!it)
		return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = it->QueryInterface(aIID, aIFileSpec);
  if (NS_FAILED(rv))
  {
    delete it;
    return rv;
  }
  return rv;
}

//----------------------------------------------------------------------------------------
NS_METHOD nsFileSpecImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aIFileSpec)
//----------------------------------------------------------------------------------------
{
  if (aIFileSpec == NULL)
    return NS_ERROR_NULL_POINTER;

	nsFileSpecImpl* it = new nsFileSpecImpl;
  if (!it)
		return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = it->QueryInterface(aIID, aIFileSpec);
  if (NS_FAILED(rv))
  {
    delete it;
    return rv;
  }
  return rv;
}

//----------------------------------------------------------------------------------------
nsresult NS_NewFileSpecWithSpec(const nsFileSpec& aSrcFileSpec, nsIFileSpec **result)
//----------------------------------------------------------------------------------------
{
	if (!result)
		return NS_ERROR_NULL_POINTER;

	return nsFileSpecImpl::MakeInterface(aSrcFileSpec, result);
}

//----------------------------------------------------------------------------------------
nsresult NS_NewFileSpec(nsIFileSpec** result)
//----------------------------------------------------------------------------------------
{
	return nsFileSpecImpl::Create(nsnull, nsIFileSpec::GetIID(), (void**)result);
}

//----------------------------------------------------------------------------------------
nsresult NS_NewDirectoryIterator(nsIDirectoryIterator** result)
//----------------------------------------------------------------------------------------
{
	return nsDirectoryIteratorImpl::Create(nsnull, nsIDirectoryIterator::GetIID(), (void**)result);
}

