/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
	nsPluginsDirMac.cpp
	
	Mac OS implementation of the nsPluginsDir/nsPluginsFile classes.
	
	by Patrick C. Beard.
 */

#include "nsPluginsDir.h"
#include "prlink.h"

#include <Processes.h>
#include <Folders.h>
#include <Resources.h>
#include <TextUtils.h>
#include <Aliases.h>
#include <string.h>

static nsresult getApplicationDir(nsFileSpec& outAppDir)
{
	// Use the process manager to get the application's FSSpec,
	// then construct an nsFileSpec that encapsulates it.
	FSSpec spec;
	ProcessInfoRec info;
	info.processInfoLength = sizeof(info);
	info.processName = NULL;
	info.processAppSpec = &spec;
	ProcessSerialNumber psn = { 0, kCurrentProcess };
	OSErr result = GetProcessInformation(&psn, &info);
	nsFileSpec appSpec(spec);
	appSpec.GetParent(outAppDir);
	return NS_OK;
}

nsPluginsDir::nsPluginsDir()
{
	// Use the folder manager to get location of Extensions folder, and
	// build an FSSpec for "Netscape Plugins" within it.
	FSSpec& pluginsDir = *this;
	OSErr result = FindFolder(kOnSystemDisk,
								 kExtensionFolderType,
								 kDontCreateFolder,
								&pluginsDir.vRefNum,
								&pluginsDir.parID);
	if (result == noErr) {
		SetLeafName("Netscape Plugins");
	}
}

nsPluginsDir::~nsPluginsDir() {}

PRBool nsPluginsDir::IsPluginFile(const nsFileSpec& fileSpec)
{
	// look at file's creator/type and make sure it is a code fragment, etc.
	FInfo info;
	const FSSpec& spec = fileSpec;
	OSErr result = FSpGetFInfo(&spec, &info);
	if (result == noErr)
		return (info.fdType == 'shlb' && info.fdCreator == 'MOSS');
	return false;
}

nsPluginFile::nsPluginFile(const nsFileSpec& spec)
	:	nsFileSpec(spec)
{
}

nsPluginFile::~nsPluginFile() {}

/**
 * Loads the plugin into memory using NSPR's shared-library loading
 * mechanism. Handles platform differences in loading shared libraries.
 */
nsresult nsPluginFile::LoadPlugin(PRLibrary* &outLibrary)
{
	// How can we convert to a full path names for using with NSPR?
	nsFilePath path(*this);
	outLibrary = PR_LoadLibrary(path);
	return NS_OK;
}

static char* p2cstrdup(StringPtr pstr)
{
	int len = pstr[0];
	char* cstr = new char[len + 1];
	if (cstr != NULL) {
		::BlockMoveData(pstr + 1, cstr, len);
		cstr[len] = '\0';
	}
	return cstr;
}

static char* GetPluginString(short id, short index)
{
	Str255 str;
	::GetIndString(str, id, index);
	return p2cstrdup(str);
}

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info)
{
	// need to open the plugin's resource file and read some resources.
	FSSpec spec = *this;
	Boolean targetIsFolder, wasAliased;
	OSErr err = ::ResolveAliasFile(&spec, true, &targetIsFolder, &wasAliased);
	short refNum = ::FSpOpenResFile(&spec, fsRdPerm);
	if (refNum != -1) {
		if (info.fPluginInfoSize >= sizeof(nsPluginInfo)) {
			// 'STR#', 126, 2 => plugin name.
			info.fName = GetPluginString(126, 2);
			
			// 'STR#', 126, 1 => plugin description.
			info.fDescription = GetPluginString(126, 1);
			
			// 'STR#', 128, 1 => MIME type.
			info.fMimeType = GetPluginString(128, 1);

			// 'STR#', 127, 1 => MIME description.
			info.fMimeDescription = GetPluginString(127, 1);
			
			// 'STR#', 128, 2 => extensions.
			info.fExtensions = GetPluginString(128, 2);

			// Determine how many  'STR#' resource for all MIME types/extensions.
			Handle typeList = ::Get1Resource('STR#', 128);
			if (typeList != NULL) {
				short stringCount = **(short**)typeList;
				info.fVariantCount = stringCount / 2;
				::ReleaseResource(typeList);
			}

			int variantCount = info.fVariantCount;
			info.fMimeTypeArray = new char*[variantCount];
			info.fMimeDescriptionArray = new char*[variantCount];
			info.fExtensionArray = new char*[variantCount];
			
			short mimeIndex = 1, descriptionIndex = 1;
			for (int i = 0; i < variantCount; i++) {
				info.fMimeTypeArray[i] = GetPluginString(128, mimeIndex++);
				info.fExtensionArray[i] = GetPluginString(128, mimeIndex++);
				info.fMimeDescriptionArray[i] = GetPluginString(127, descriptionIndex++);
			}
		}
		
		::CloseResFile(refNum);
	}
	return NS_OK;
}
