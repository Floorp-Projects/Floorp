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

// Mac specific interfaces for nsLocalFileMac

#ifndef _nsILocalFileMAC_H_
#define _nsILocalFileMAC_H_

#include "nsISupports.h"

#include <Files.h>
#include <Processes.h>

#define NS_ILOCALFILEMAC_IID_STR "614c3010-1dd2-11b2-be04-bcd57a64ffc9"

#define NS_ILOCALFILEMAC_IID \
  {0x614c3010, 0x1dd2, 0x11b2, \
    { 0xbe, 0x04, 0xbc, 0xd5, 0x7a, 0x64, 0xcc, 0xc9 }}


typedef enum {
	eNotInitialized = 0,
	eInitWithPath,
	eInitWithFSSpec
} nsLocalFileMacInitType;


class nsILocalFile;

class nsILocalFileMac : public nsISupports
{
public: 
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILOCALFILEMAC_IID)

	// Use with SetFileTypeAndCreator to make creator be that of current process
	enum { CURRENT_PROCESS_CREATOR = 0x8000000 };

	// We need to be able to determine what init method was used as that
	// will affect how we clone a nsILocalFileMac
	NS_IMETHOD GetInitType(nsLocalFileMacInitType *type) = 0;

	// Since the OS native way to represent a file on the Mac is an FSSpec
	// we provide a way to initialize an nsLocalFile with one
	NS_IMETHOD InitWithFSSpec(const FSSpec *fileSpec) = 0;

	// Init this filespec to point to an application which is sought by
	// creator code. If this app is missing, this will fail. It will first
	// look for running application with the given creator.
	NS_IMETHOD InitFindingAppByCreatorCode(OSType aAppCreator) = 0;

	// In case we need to get the FSSpecs at the heart of an nsLocalFileMac
	NS_IMETHOD GetFSSpec(FSSpec *fileSpec) = 0;			// The one we were inited with
	NS_IMETHOD GetResolvedFSSpec(FSSpec *fileSpec) = 0;	// The one we've resolved to
	NS_IMETHOD GetTargetFSSpec(FSSpec *fileSpec) = 0;	// The one we're really really pointing to
	
	// Since we may have both an FSSpec and an appended path we need methods
	// to get/set just the appended path
	NS_IMETHOD SetAppendedPath(const char *aPath) = 0;
	NS_IMETHOD GetAppendedPath(char * *aPath) = 0;

	// Get/Set methods for the file type and creator codes
	// Note that passing null for either the type or creator in the
	// SetFileTypeAndCreator call will preserve the existing code
	NS_IMETHOD GetFileTypeAndCreator(OSType *type, OSType *creator) = 0;
	NS_IMETHOD SetFileTypeAndCreator(OSType type, OSType creator) = 0;
	
	// Method for setting the file type by suffix. Just setting the
	// type is probably enough. The creator is set to that of the current process
	// by default. Failure is likely on these methods - take it lightly.
	NS_IMETHOD SetFileTypeFromSuffix(const char *suffix) = 0;
	
	// Method for setting the file type and creator by MIME type.  Internet
	// Config is consulted for the mapping of MIME type to the appropriate file 
	// type and creator pair.
	NS_IMETHOD SetFileTypeAndCreatorFromMIMEType(const char *aMIMEType) = 0;
	
	// Since Mac files can consist of both a data and resource fork we have a
	// method that will return the combined size of both forks rather than just the
	// size of the data fork as returned by GetFileSize()
	NS_IMETHOD GetFileSizeWithResFork(PRInt64 *aFileSize) = 0;
	
	// Launch the application that this file points to, optionally with a document.
	// If aDocToLoad == NULL, the application is launched without a doc.
	NS_IMETHOD LaunchAppWithDoc(nsILocalFile* aDocToLoad, PRBool aLaunchInBackground)=0;

	// Open the document that this file points to with the given application.
	// If aAppToOpenWith is nil, then we look at the creator code of the document,
	// then find an app with that signature to open with.
	NS_IMETHOD OpenDocWithApp(nsILocalFile* aAppToOpenWith, PRBool aLaunchInBackground)=0;
	
};


extern "C" NS_EXPORT nsresult
NS_NewLocalFileWithFSSpec(FSSpec* inSpec, PRBool followSymlinks, nsILocalFileMac* *result);

extern "C" NS_EXPORT nsresult
NS_NewLocalFile(const char* path, PRBool followLinks, nsILocalFile* *result);

#endif
