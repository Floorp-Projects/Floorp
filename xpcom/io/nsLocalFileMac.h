/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Steve Dagley <sdagley@netscape.com>
 */

#ifndef _nsLocalFileMAC_H_
#define _nsLocalFileMAC_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsIFactory.h"
#include "nsLocalFile.h"

#include <Files.h>

class NS_COM nsLocalFile : public nsILocalFile, public nsILocalFileMac
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)
    
    nsLocalFile();
    virtual ~nsLocalFile();

    static NS_METHOD nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    // nsISupports interface
    NS_DECL_ISUPPORTS
    
    // nsIFile interface
    NS_DECL_NSIFILE
    
    // nsILocalFile interface
    NS_DECL_NSILOCALFILE

	NS_IMETHOD GetInitType(nsLocalFileMacInitType *type);

	NS_IMETHOD InitWithFSSpec(const FSSpec *fileSpec);
	NS_IMETHOD InitFindingAppByCreatorCode(OSType aAppCreator);

	NS_IMETHOD GetFSSpec(FSSpec *fileSpec);
	NS_IMETHOD GetResolvedFSSpec(FSSpec *fileSpec);
	NS_IMETHOD GetTargetFSSpec(FSSpec *fileSpec);

	NS_IMETHOD SetAppendedPath(const char *aPath);
	NS_IMETHOD GetAppendedPath(char * *aPath);

	NS_IMETHOD GetFileTypeAndCreator(OSType *type, OSType *creator);
	NS_IMETHOD SetFileTypeAndCreator(OSType type, OSType creator);

	NS_IMETHOD SetFileTypeFromSuffix(const char *suffix);
	NS_IMETHOD SetFileTypeAndCreatorFromMIMEType(const char *aMIMEType);

	NS_IMETHOD GetFileSizeWithResFork(PRInt64 *aFileSize);

	NS_IMETHOD LaunchAppWithDoc(nsILocalFile* aDocToLoad, PRBool aLaunchInBackground);
	NS_IMETHOD OpenDocWithApp(nsILocalFile* aAppToOpenWith, PRBool aLaunchInBackground);
    static nsresult ParseURL(const char* inURL, char **outHost, char **outDirectory,
                             char **outFileBaseName, char **outFileExtension);


protected:

    void 			MakeDirty();
    nsresult 	ResolveAndStat(PRBool resolveTerminal);

    nsresult  FindAppOnLocalVolumes(OSType sig, FSSpec &outSpec);
    
    nsresult  FindRunningAppBySignature(OSType sig, FSSpec& outSpec, ProcessSerialNumber& outPsn);
    nsresult  FindRunningAppByFSSpec(const FSSpec& appSpec, ProcessSerialNumber& outPsn);

    nsresult  MyLaunchAppWithDoc(const FSSpec& appSpec, const FSSpec* aDocToLoad, PRBool aLaunchInBackground);
    
		nsresult	TestFinderFlag(PRUint16 flagMask, PRBool *outFlagSet, PRBool testTargetSpec = PR_TRUE);

    OSErr			GetTargetSpecCatInfo(CInfoPBRec& outInfo);
	nsresult MoveCopy( nsIFile* newParentDir, const char* newName, PRBool isCopy, PRBool followLinks );
		
	// Passing nsnull for the extension uses leaf name
	nsresult SetOSTypeAndCreatorFromExtension(const char* extension = nsnull);
	
	nsresult ExtensionIsOnExceptionList(const char *extension, PRBool *onList);
						
private:

    // It's important we keep track of how we were initialized
    nsLocalFileMacInitType	mInitType;
        
    // this is the flag which indicates if I can used cached information about the file
    PRPackedBool	mStatDirty;
    PRPackedBool    mLastResolveFlag;
    
    PRPackedBool    mFollowSymlinks;

    // Is the mResolvedSpec member valid?  Only after we resolve the mSpec or mWorkingPath
    // PRPackedBool		mHaveValidSpec;

    // If we're inited with a path then we store it here
    nsCString	mWorkingPath;
    
    // Any nodes added with AppendPath if we were initialized with an FSSpec are stored here
    nsCString	mAppendedPath;

    // this will be the resolved path which will *NEVER* be returned to the user
    nsCString	mResolvedPath;
    
    // The Mac data structure for a file system object
    FSSpec		mSpec;					// This is the raw spec from InitWithPath or InitWithFSSpec
    FSSpec		mResolvedSpec;	// This is the spec that we get from the initial spec + path. It might be an alias
    FSSpec		mTargetSpec;		// This is the spec we've called ResolveAlias on
    
    Boolean		mResolvedWasAlias;	// mResolvedSpec was for an alias
    Boolean		mResolvedWasFolder;	// mResolvedSpec was for a directory
    
    
    PRPackedBool	mHaveFileInfo;					// have we got the file info?    
    CInfoPBRec  	mTargetFileInfoRec;			// cached file info, for the mTargetSpec
    
    OSType			mType, mCreator;

    static void     InitClassStatics();

    static OSType   sCurrentProcessSignature;
    static PRBool   sHasHFSPlusAPIs;        
};

#endif

