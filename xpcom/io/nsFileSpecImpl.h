/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0(the "NPL"); you may not use this file except in
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
 * Copyright(C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _FILESPECIMPL_H_
#define _FILESPECIMPL_H_

#include "nsIFileSpec.h" 
#include "nsFileSpec.h"

//========================================================================================
class NS_COM nsFileSpecImpl
//========================================================================================
	: public nsIFileSpec
{

 public: 

	NS_DECL_ISUPPORTS

	NS_IMETHOD fromFileSpec(const nsIFileSpec *original);

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
    
    /* boolean isHidden (); */
	NS_IMETHOD isHidden(PRBool *_retval);

    /* boolean isSymlink (); */
	NS_IMETHOD isSymlink(PRBool *_retval);

    /* void resolveSymlink (); */
	NS_IMETHOD resolveSymlink();

	/* readonly attribute unsigned long FileSize; */
	NS_IMETHOD GetFileSize(PRUint32 *aFileSize);

	/* readonly attribute unsigned long DiskSpaceAvailable; */
	NS_IMETHOD GetDiskSpaceAvailable(PRUint32 *aDiskSpaceAvailable);

	/* nsIFileSpec AppendRelativeUnixPath (in string relativePath); */
	NS_IMETHOD AppendRelativeUnixPath(const char *relativePath);

	/* void createDir (); */
	NS_IMETHOD createDir();

	/* void touch (); */
	NS_IMETHOD touch();

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

	NS_IMETHOD GetInputStream(nsIInputStream**);
	NS_IMETHOD GetOutputStream(nsIOutputStream**);

	NS_IMETHOD GetFileContents(char**);
	NS_IMETHOD SetFileContents(char*);

	NS_IMETHOD GetFileSpec(nsFileSpec *aFileSpec);
    NS_IMETHOD setFromFileSpec(const nsFileSpec& aFileSpec);

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
	// COM Cruft
	//----------------------

      static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aIFileSpec);

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

	//----------------------
	// COM Cruft
	//----------------------

    static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aIFileSpec);

protected:

	nsDirectoryIterator*					mDirectoryIterator;
}; // class nsDirectoryIteratorImpl

#endif // _FILESPECIMPL_H_
