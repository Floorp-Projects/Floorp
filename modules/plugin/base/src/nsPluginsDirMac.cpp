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

static nsresult getApplicationDir(nsNativeFileSpec& outAppDir)
{
	// Use the process manager to get the application's FSSpec,
	// then construct an nsNativeFileSpec that encapsulates it.
	FSSpec spec;
	ProcessInfoRec info;
	info.processInfoLength = sizeof(info);
	info.processName = NULL;
	info.processAppSpec = &spec;
	ProcessSerialNumber psn = { 0, kCurrentProcess };
	OSErr result = GetProcessInformation(&psn, &info);
	nsNativeFileSpec appSpec(spec);
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

PRBool nsPluginsDir::IsPluginFile(const nsNativeFileSpec& fileSpec)
{
	// look at file's creator/type and make sure it is a code fragment, etc.
	FInfo info;
	const FSSpec& spec = fileSpec;
	OSErr result = FSpGetFInfo(&spec, &info);
	if (result == noErr)
		return (info.fdType == 'shlb' && info.fdCreator == 'MOSS');
	return false;
}

nsPluginFile::nsPluginFile(const nsNativeFileSpec& spec)
	:	nsNativeFileSpec(spec)
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

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo &outPluginInfo)
{
	return NS_OK;
}
