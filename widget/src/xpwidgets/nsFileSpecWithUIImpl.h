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

#ifndef nsIFileSpecWithUIImpl_h__
#define nsIFileSpecWithUIImpl_h__

#include "nsIFileSpecWithUI.h" // Always first, to ensure that it compiles alone.
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
		{ return mBaseFileSpec ? mBaseFileSpec->fromFileSpec(original) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD GetURLString(char * *aURLString)
		{ return mBaseFileSpec ? mBaseFileSpec->GetURLString(aURLString) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetURLString(char * aURLString)
		{ return mBaseFileSpec ? mBaseFileSpec->SetURLString(aURLString) : NS_ERROR_NOT_INITIALIZED; }

	/* attribute string UnixStyleFilePath; */
	NS_IMETHOD GetUnixStyleFilePath(char * *aUnixStyleFilePath)
		{ return mBaseFileSpec ? mBaseFileSpec->GetUnixStyleFilePath(aUnixStyleFilePath) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetUnixStyleFilePath(char * aUnixStyleFilePath)
		{ return mBaseFileSpec ? mBaseFileSpec->SetUnixStyleFilePath(aUnixStyleFilePath) : NS_ERROR_NOT_INITIALIZED; }

	/* attribute string PersistentDescriptorString; */
	NS_IMETHOD GetPersistentDescriptorString(char * *aPersistentDescriptorString)
		{ return mBaseFileSpec ? mBaseFileSpec->GetPersistentDescriptorString(aPersistentDescriptorString) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetPersistentDescriptorString(char * aPersistentDescriptorString)
		{ return mBaseFileSpec ? mBaseFileSpec->SetPersistentDescriptorString(aPersistentDescriptorString) : NS_ERROR_NOT_INITIALIZED; }

	/* attribute string NativePath; */
	NS_IMETHOD GetNativePath(char * *aNativePath)
		{ return mBaseFileSpec ? mBaseFileSpec->GetNativePath(aNativePath) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetNativePath(char * aNativePath)
		{ return mBaseFileSpec ? mBaseFileSpec->SetNativePath(aNativePath) : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute string NSPRPath; */
	NS_IMETHOD GetNSPRPath(char * *aNSPRPath)
		{ return mBaseFileSpec ? mBaseFileSpec->GetNSPRPath(aNSPRPath) : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute nsresult Error; */
	NS_IMETHOD error()
		{ return mBaseFileSpec ? mBaseFileSpec->error() : NS_ERROR_NOT_INITIALIZED; }

	/* boolean isValid (); */
	NS_IMETHOD isValid(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->isValid(_retval) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean failed (); */
	NS_IMETHOD failed(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->failed(_retval) : NS_ERROR_NOT_INITIALIZED; }

	/* attribute string LeafName; */
	NS_IMETHOD GetLeafName(char * *aLeafName)
		{ return mBaseFileSpec ? mBaseFileSpec->GetLeafName(aLeafName) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetLeafName(char * aLeafName)
		{ return mBaseFileSpec ? mBaseFileSpec->SetLeafName(aLeafName) : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute nsIFileSpec Parent; */
	NS_IMETHOD GetParent(nsIFileSpec * *aParent)
		{ return mBaseFileSpec ? mBaseFileSpec->GetParent(aParent) : NS_ERROR_NOT_INITIALIZED; }

	/* nsIFileSpec makeUnique (); */
	NS_IMETHOD makeUnique()
		{ return mBaseFileSpec ? mBaseFileSpec->makeUnique() : NS_ERROR_NOT_INITIALIZED; }

	/* nsIFileSpec makeUniqueWithSuggestedName (in string suggestedName); */
	NS_IMETHOD makeUniqueWithSuggestedName(const char* inSuggestedLeafName)
		{ return mBaseFileSpec ? mBaseFileSpec->makeUniqueWithSuggestedName(inSuggestedLeafName) : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute unsigned long ModDate; */
	NS_IMETHOD GetModDate(PRUint32 *aModDate)
		{ return mBaseFileSpec ? mBaseFileSpec->GetModDate(aModDate) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean modDateChanged (in unsigned long oldStamp); */
	NS_IMETHOD modDateChanged(PRUint32 oldStamp, PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->modDateChanged(oldStamp, _retval) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean isDirectory (); */
	NS_IMETHOD isDirectory(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->isDirectory(_retval) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean isFile (); */
	NS_IMETHOD isFile(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->isFile(_retval) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean exists (); */
	NS_IMETHOD exists(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->exists(_retval) : NS_ERROR_NOT_INITIALIZED; }
    
    /* boolean isHidden (); */
	NS_IMETHOD isHidden(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->isHidden(_retval) : NS_ERROR_NOT_INITIALIZED; }
    
    /* boolean isSymlink (); */
	NS_IMETHOD isSymlink(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->isSymlink(_retval) : NS_ERROR_NOT_INITIALIZED; }

    /* void resolveSymlink (); */
	NS_IMETHOD resolveSymlink()
		{ return mBaseFileSpec ? mBaseFileSpec->resolveSymlink() : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute unsigned long FileSize; */
	NS_IMETHOD GetFileSize(PRUint32 *aFileSize)
		{ return mBaseFileSpec ? mBaseFileSpec->GetFileSize(aFileSize) : NS_ERROR_NOT_INITIALIZED; }

	/* readonly attribute unsigned long DiskSpaceAvailable; */
	NS_IMETHOD GetDiskSpaceAvailable(PRUint32 *aDiskSpaceAvailable)
		{ return mBaseFileSpec ? mBaseFileSpec->GetDiskSpaceAvailable(aDiskSpaceAvailable) : NS_ERROR_NOT_INITIALIZED; }

	/* nsIFileSpec AppendRelativeUnixPath (in string relativePath); */
	NS_IMETHOD AppendRelativeUnixPath(const char *relativePath)
		{ return mBaseFileSpec ? mBaseFileSpec->AppendRelativeUnixPath(relativePath) : NS_ERROR_NOT_INITIALIZED; }

	/* void createDir (); */
	NS_IMETHOD createDir()
		{ return mBaseFileSpec ? mBaseFileSpec->createDir() : NS_ERROR_NOT_INITIALIZED; }

	/* void touch (); */
	NS_IMETHOD touch ()
		{ return mBaseFileSpec ? mBaseFileSpec->touch() : NS_ERROR_NOT_INITIALIZED; }
       
	/* void rename ([const] in string newLeafName); */
	NS_IMETHOD rename(const char *newLeafName)
		{ return mBaseFileSpec ? mBaseFileSpec->rename(newLeafName) : NS_ERROR_NOT_INITIALIZED; }

	/* void copyToDir ([const] in nsIFileSpec newParentDir); */
	NS_IMETHOD copyToDir(const nsIFileSpec *newParentDir)
		{ return mBaseFileSpec ? mBaseFileSpec->copyToDir(newParentDir) : NS_ERROR_NOT_INITIALIZED; }

	/* void moveToDir ([const] in nsIFileSpec newParentDir); */
	NS_IMETHOD moveToDir(const nsIFileSpec *newParentDir)
		{ return mBaseFileSpec ? mBaseFileSpec->moveToDir(newParentDir) : NS_ERROR_NOT_INITIALIZED; }

	/* void execute ([const] in string args); */
	NS_IMETHOD execute(const char *args)
		{ return mBaseFileSpec ? mBaseFileSpec->execute(args) : NS_ERROR_NOT_INITIALIZED; }

	/* void openStreamForReading (); */
	NS_IMETHOD openStreamForReading()
		{ return mBaseFileSpec ? mBaseFileSpec->openStreamForReading() : NS_ERROR_NOT_INITIALIZED; }

	/* void openStreamForWriting (); */
	NS_IMETHOD openStreamForWriting()
		{ return mBaseFileSpec ? mBaseFileSpec->openStreamForWriting() : NS_ERROR_NOT_INITIALIZED; }

	/* void openStreamForReadingAndWriting (); */
	NS_IMETHOD openStreamForReadingAndWriting()
		{ return mBaseFileSpec ? mBaseFileSpec->openStreamForReadingAndWriting() : NS_ERROR_NOT_INITIALIZED; }

	/* void close (); */
	NS_IMETHOD closeStream()
		{ return mBaseFileSpec ? mBaseFileSpec->closeStream() : NS_ERROR_NOT_INITIALIZED; }

	/* boolean isOpen (); */
	NS_IMETHOD isStreamOpen(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->isStreamOpen(_retval) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD GetInputStream(nsIInputStream** _retval)
		{ return mBaseFileSpec ? mBaseFileSpec->GetInputStream(_retval) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD GetOutputStream(nsIOutputStream** _retval)
		{ return mBaseFileSpec ? mBaseFileSpec->GetOutputStream(_retval) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD GetFileContents(char** _retval)
		{ return mBaseFileSpec ? mBaseFileSpec->GetFileContents(_retval) : NS_ERROR_NOT_INITIALIZED; }
	NS_IMETHOD SetFileContents(char* s)
		{ return mBaseFileSpec ? mBaseFileSpec->SetFileContents(s) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD GetFileSpec(nsFileSpec *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->GetFileSpec(_retval) : NS_ERROR_NOT_INITIALIZED; }
    NS_IMETHOD setFromFileSpec(const nsFileSpec& s)
		{ return mBaseFileSpec ? mBaseFileSpec->setFromFileSpec(s) : NS_ERROR_NOT_INITIALIZED; }

	/* boolean eof (); */
	NS_IMETHOD eof(PRBool *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->eof(_retval) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD read(char** buffer, PRInt32 requestedCount, PRInt32 *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->read(buffer, requestedCount, _retval) : NS_ERROR_NOT_INITIALIZED; }

	NS_IMETHOD readLine(char** line, PRInt32 bufferSize, PRBool *wasTruncated)
		{ return mBaseFileSpec ? mBaseFileSpec->readLine(line, bufferSize, wasTruncated) : NS_ERROR_NOT_INITIALIZED; }
					// Check eof() before each call.
					// CAUTION: false result only indicates line was truncated
					// to fit buffer, or an error occurred (OTHER THAN eof).


	NS_IMETHOD write(const char* data, PRInt32 requestedCount, PRInt32 *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->write(data, requestedCount, _retval) : NS_ERROR_NOT_INITIALIZED; }

	/* void flush (); */
	NS_IMETHOD flush()
		{ return mBaseFileSpec ? mBaseFileSpec->flush() : NS_ERROR_NOT_INITIALIZED; }

	/* void seek (in long offset); */
	NS_IMETHOD seek(PRInt32 offset)
		{ return mBaseFileSpec ? mBaseFileSpec->seek(offset) : NS_ERROR_NOT_INITIALIZED; }

	/* long tell (); */
	NS_IMETHOD tell(PRInt32 *_retval)
		{ return mBaseFileSpec ? mBaseFileSpec->tell(_retval) : NS_ERROR_NOT_INITIALIZED; }

	/* void endline (); */
	NS_IMETHOD endline()
		{ return mBaseFileSpec ? mBaseFileSpec->endline() : NS_ERROR_NOT_INITIALIZED; }

	// Data
protected:
	nsCOMPtr<nsIFileSpec>	mBaseFileSpec;

}; // class nsFileSpecWithUIImpl

#endif // nsIFileSpecWithUIImpl_h__
