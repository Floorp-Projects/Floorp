/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

static nsresult getApplicationSpec(FSSpec& outAppSpec)
{
	// Use the process manager to get the application's FSSpec,
	// then construct an nsFileSpec that encapsulates it.
	ProcessInfoRec info;
	info.processInfoLength = sizeof(info);
	info.processName = NULL;
	info.processAppSpec = &outAppSpec;
	ProcessSerialNumber psn = { 0, kCurrentProcess };
	OSErr result = GetProcessInformation(&psn, &info);
	return (result == noErr ? NS_OK : NS_ERROR_FAILURE);
}

nsPluginsDir::nsPluginsDir(PRUint16 location)
{
#if 0
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
#else
	// The "Plugins" folder in the application's directory is where plugins are loaded from.
	mError = getApplicationSpec(mSpec);
	if (NS_SUCCEEDED(mError)) 
  {
    if(location == PLUGINS_DIR_LOCATION_MAC_OLD)
		  SetLeafName("Plug-ins");
    else
		  SetLeafName("Plugins");
	}
#endif
}

nsPluginsDir::~nsPluginsDir() {}

PRBool nsPluginsDir::IsPluginFile(const nsFileSpec& fileSpec)
{
	// look at file's creator/type and make sure it is a code fragment, etc.
	FInfo info;
	const FSSpec& spec = fileSpec;
	OSErr result = FSpGetFInfo(&spec, &info);
	if (result == noErr)
		return ((info.fdType == 'shlb' && info.fdCreator == 'MOSS')  || info.fdType == 'NSPL');
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
	const char* path = this->GetCString();
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
   // clear out the info, except for the first field.
   nsCRT::memset(&info.fName, 0, sizeof(info) - sizeof(PRUint32));

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
			info.fFileName = p2cstrdup(spec.name);
			
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

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
  if (info.fPluginInfoSize <= sizeof(nsPluginInfo)) 
  {
    delete[] info.fName;
    delete[] info.fDescription;
    int variantCount = info.fVariantCount;
    for (int i = 0; i < variantCount; i++) 
    {
      delete[] info.fMimeTypeArray[i];
      delete[] info.fExtensionArray[i];
      delete[] info.fMimeDescriptionArray[i];
    }
    delete[] info.fMimeTypeArray;
    delete[] info.fMimeDescriptionArray;
    delete[] info.fExtensionArray;
    delete[] info.fFileName;
  }
  return NS_OK;
}
