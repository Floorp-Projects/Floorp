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

#ifndef nsIFileSpecWithUIImpl_h__
#define nsIFileSpecWithUIImpl_h__

#include "nsIFileSpecWithUI.h" // Always first, to ensure that it compiles alone.
#include "nsIFileWidget.h"
#include "nsCOMPtr.h"

//========================================================================================
class nsFileSpecWithUIImpl
//========================================================================================
	: public nsIFileSpecWithUI
{

 public: 

	NS_DECL_ISUPPORTS
	
	nsFileSpecWithUIImpl();
	virtual ~nsFileSpecWithUIImpl();

  NS_DECL_NSIFILESPECWITHUI
	
	//------------------
	// INHERITED/FORWARDED METHODS
	//------------------
	
	NS_IMETHOD FromFileSpec(const nsIFileSpec *original)
		{ return mBaseFileSpec ? mBaseFileSpec->FromFileSpec(original) : NS_ERROR_NOT_INITIALIZED; }

  NS_IMETHOD IsChildOf(nsIFileSpec *possibleParent, PRBool *_retval)
    { return mBaseFileSpec ? mBaseFileSpec->IsChildOf(possibleParent, _retval)
        : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD GetURLString(char * *aURLString)
		{ return mBaseFileSpec ? mBaseFileSpec->GetURLString(aURLString) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetURLString(const char * aURLString)
		{ return mBaseFileSpec ? mBaseFileSpec->SetURLString(aURLString) : NS_ERROR_NOT_INITIALIZED; }

	/* attribute string UnixStyleFilePath; */
	NS_IMETHOD GetUnixStyleFilePath(char * *aUnixStyleFilePath)
		{ return mBaseFileSpec ? mBaseFileSpec->GetUnixStyleFilePath(aUnixStyleFilePath) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetUnixStyleFilePath(const char * aUnixStyleFilePath)
		{ return mBaseFileSpec ? mBaseFileSpec->SetUnixStyleFilePath(aUnixStyleFilePath) : NS_ERROR_NOT_INITIALIZED; }

	/* attribute string PersistentDescriptorString; */
	NS_IMETHOD GetPersistentDescriptorString(char * *aPersistentDescriptorString)
		{ return mBaseFileSpec ? mBaseFileSpec->GetPersistentDescriptorString(aPersistentDescriptorString) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetPersistentDescriptorString(const char * aPersistentDescriptorString)
		{ return mBaseFileSpec ? mBaseFileSpec->SetPersistentDescriptorString(aPersistentDescriptorString) : NS_ERROR_NOT_INITIALIZED; }

	/* attribute string NativePath; */
	NS_IMETHOD GetNativePath(char * *aNativePath)
		{ return mBaseFileSpec ? mBaseFileSpec->GetNativePath(aNativePath) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetNativePath(const char * aNativePath)
		{ return mBaseFileSpec ? mBaseFileSpec->SetNativePath(aNativePath) : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute string NSPRPath; */
	NS_IMETHOD GetNSPRPath(char * *aNSPRPath)
		{ return mBaseFileSpec ? mBaseFileSpec->GetNSPRPath(aNSPRPath) : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute nsresult Error; */
	NS_IMETHOD Error()
		{ return mBaseFileSpec ? mBaseFileSpec->Error() : NS_ERROR_NOT_INITIALIZED; }

	/* boolean isValid (); */
	NS_IMETHOD IsValid(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->IsValid(_retval) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean failed (); */
	NS_IMETHOD Failed(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->Failed(_retval) : NS_ERROR_NOT_INITIALIZED; }

	/* attribute string LeafName; */
	NS_IMETHOD GetLeafName(char * *aLeafName)
		{ return mBaseFileSpec ? mBaseFileSpec->GetLeafName(aLeafName) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetLeafName(const char * aLeafName)
		{ return mBaseFileSpec ? mBaseFileSpec->SetLeafName(aLeafName) : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute nsIFileSpec Parent; */
	NS_IMETHOD GetParent(nsIFileSpec * *aParent)
		{ return mBaseFileSpec ? mBaseFileSpec->GetParent(aParent) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean equals(in nsIFileSpec spec); */
	NS_IMETHOD Equals(nsIFileSpec *spec, PRBool *result)
        { return mBaseFileSpec ? mBaseFileSpec->Equals(spec, result) : NS_ERROR_NOT_INITIALIZED; } 

	/* nsIFileSpec makeUnique (); */
	NS_IMETHOD MakeUnique()
		{ return mBaseFileSpec ? mBaseFileSpec->MakeUnique() : NS_ERROR_NOT_INITIALIZED; }

	/* nsIFileSpec makeUniqueWithSuggestedName (in string suggestedName); */
	NS_IMETHOD MakeUniqueWithSuggestedName(const char* inSuggestedLeafName)
		{ return mBaseFileSpec ? mBaseFileSpec->MakeUniqueWithSuggestedName(inSuggestedLeafName) : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute unsigned long ModDate; */
	NS_IMETHOD GetModDate(PRUint32 *aModDate)
		{ return mBaseFileSpec ? mBaseFileSpec->GetModDate(aModDate) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean modDateChanged (in unsigned long oldStamp); */
	NS_IMETHOD ModDateChanged(PRUint32 oldStamp, PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->ModDateChanged(oldStamp, _retval) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean isDirectory (); */
	NS_IMETHOD IsDirectory(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->IsDirectory(_retval) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean isFile (); */
	NS_IMETHOD IsFile(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->IsFile(_retval) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean exists (); */
	NS_IMETHOD Exists(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->Exists(_retval) : NS_ERROR_NOT_INITIALIZED; }
    
    /* boolean isHidden (); */
	NS_IMETHOD IsHidden(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->IsHidden(_retval) : NS_ERROR_NOT_INITIALIZED; }
    
    /* boolean isSymlink (); */
	NS_IMETHOD IsSymlink(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->IsSymlink(_retval) : NS_ERROR_NOT_INITIALIZED; }

    /* void resolveSymlink (); */
	NS_IMETHOD ResolveSymlink()
		{ return mBaseFileSpec ? mBaseFileSpec->ResolveSymlink() : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute unsigned long FileSize; */
	NS_IMETHOD GetFileSize(PRUint32 *aFileSize)
		{ return mBaseFileSpec ? mBaseFileSpec->GetFileSize(aFileSize) : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute unsigned long DiskSpaceAvailable; */
	NS_IMETHOD GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
		{ return mBaseFileSpec ? mBaseFileSpec->GetDiskSpaceAvailable(aDiskSpaceAvailable) : NS_ERROR_NOT_INITIALIZED; }

	/* nsIFileSpec AppendRelativeUnixPath (in string relativePath); */
	NS_IMETHOD AppendRelativeUnixPath(const char *relativePath)
		{ return mBaseFileSpec ? mBaseFileSpec->AppendRelativeUnixPath(relativePath) : NS_ERROR_NOT_INITIALIZED; }

	/* void createDir (); */
	NS_IMETHOD CreateDir()
		{ return mBaseFileSpec ? mBaseFileSpec->CreateDir() : NS_ERROR_NOT_INITIALIZED; }

	/* void touch (); */
	NS_IMETHOD Touch ()
		{ return mBaseFileSpec ? mBaseFileSpec->Touch() : NS_ERROR_NOT_INITIALIZED; }
       
	/* void delete (in boolean aRecursive); */
	NS_IMETHOD Delete(PRBool aRecursive)
		{ return mBaseFileSpec ? mBaseFileSpec->Delete(aRecursive) : NS_ERROR_NOT_INITIALIZED; }

	/* void truncate (in long aNewLength); */
	NS_IMETHOD Truncate(PRInt32 aNewLength)
		{ return mBaseFileSpec ? mBaseFileSpec->Truncate(aNewLength) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD Rename(const char *newLeafName)
		{ return mBaseFileSpec ? mBaseFileSpec->Rename(newLeafName) : NS_ERROR_NOT_INITIALIZED; }

	/* void copyToDir ([const] in nsIFileSpec newParentDir); */
	NS_IMETHOD CopyToDir(const nsIFileSpec *newParentDir)
		{ return mBaseFileSpec ? mBaseFileSpec->CopyToDir(newParentDir) : NS_ERROR_NOT_INITIALIZED; }

	/* void moveToDir ([const] in nsIFileSpec newParentDir); */
	NS_IMETHOD MoveToDir(const nsIFileSpec *newParentDir)
		{ return mBaseFileSpec ? mBaseFileSpec->MoveToDir(newParentDir) : NS_ERROR_NOT_INITIALIZED; }

	/* void execute ([const] in string args); */
	NS_IMETHOD Execute(const char *args)
		{ return mBaseFileSpec ? mBaseFileSpec->Execute(args) : NS_ERROR_NOT_INITIALIZED; }

	/* void openStreamForReading (); */
	NS_IMETHOD OpenStreamForReading()
		{ return mBaseFileSpec ? mBaseFileSpec->OpenStreamForReading() : NS_ERROR_NOT_INITIALIZED; }

	/* void openStreamForWriting (); */
	NS_IMETHOD OpenStreamForWriting()
		{ return mBaseFileSpec ? mBaseFileSpec->OpenStreamForWriting() : NS_ERROR_NOT_INITIALIZED; }

	/* void openStreamForReadingAndWriting (); */
	NS_IMETHOD OpenStreamForReadingAndWriting()
		{ return mBaseFileSpec ? mBaseFileSpec->OpenStreamForReadingAndWriting() : NS_ERROR_NOT_INITIALIZED; }

	/* void close (); */
	NS_IMETHOD CloseStream()
		{ return mBaseFileSpec ? mBaseFileSpec->CloseStream() : NS_ERROR_NOT_INITIALIZED; }

	/* boolean isOpen (); */
	NS_IMETHOD IsStreamOpen(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->IsStreamOpen(_retval) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD GetInputStream(nsIInputStream** _retval)
		{ return mBaseFileSpec ? mBaseFileSpec->GetInputStream(_retval) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD GetOutputStream(nsIOutputStream** _retval)
		{ return mBaseFileSpec ? mBaseFileSpec->GetOutputStream(_retval) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD GetFileContents(char** _retval)
		{ return mBaseFileSpec ? mBaseFileSpec->GetFileContents(_retval) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetFileContents(const char* s)
		{ return mBaseFileSpec ? mBaseFileSpec->SetFileContents(s) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD GetFileSpec(nsFileSpec *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->GetFileSpec(_retval) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetFromFileSpec(const nsFileSpec& s)
		{ return mBaseFileSpec ? mBaseFileSpec->SetFromFileSpec(s) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean eof (); */
	NS_IMETHOD Eof(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->Eof(_retval) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD Read(char** buffer, PRInt32 requestedCount, PRInt32 *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->Read(buffer, requestedCount, _retval) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD ReadLine(char** line, PRInt32 bufferSize, PRBool *wasTruncated)
		{ return mBaseFileSpec ? mBaseFileSpec->ReadLine(line, bufferSize, wasTruncated) : NS_ERROR_NOT_INITIALIZED; }
					// Check eof() before each call.
					// CAUTION: false result only indicates line was truncated
					// to fit buffer, or an error occurred (OTHER THAN eof).


	NS_IMETHOD Write(const char* data, PRInt32 requestedCount, PRInt32 *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->Write(data, requestedCount, _retval) : NS_ERROR_NOT_INITIALIZED; }

	/* void flush (); */
	NS_IMETHOD Flush()
		{ return mBaseFileSpec ? mBaseFileSpec->Flush() : NS_ERROR_NOT_INITIALIZED; }

	/* void seek (in long offset); */
	NS_IMETHOD Seek(PRInt32 offset)
		{ return mBaseFileSpec ? mBaseFileSpec->Seek(offset) : NS_ERROR_NOT_INITIALIZED; }

	/* long tell (); */
	NS_IMETHOD Tell(PRInt32 *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->Tell(_retval) : NS_ERROR_NOT_INITIALIZED; }

	/* void endline (); */
	NS_IMETHOD EndLine()
		{ return mBaseFileSpec ? mBaseFileSpec->EndLine() : NS_ERROR_NOT_INITIALIZED; }

	// Data
protected:
    // helpers
    void SetFileWidgetFilterList(nsIFileWidget* fileWidget,
                                 PRUint32 mask,
                                 const char *inExtraFilterTitle, 
                                 const char *inExtraFilter);

    void SetFileWidgetStartDir(nsIFileWidget* fileWidget);

	nsCOMPtr<nsIFileSpec>	mBaseFileSpec;

    nsIDOMWindowInternal *mParentWindow;

}; // class nsFileSpecWithUIImpl

#endif // nsIFileSpecWithUIImpl_h__
