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
	nsPluginsDirWin.cpp
	
	Windows implementation of the nsPluginsDir/nsPluginsFile classes.
	
	by Alex Musil
 */

#include "nsPluginsDir.h"
#include "prlink.h"
#include "plstr.h"
#include "prmem.h"

#include "windows.h"
#include "winbase.h"

///////////////////////////////////////////////////////////////////////////

/* Local helper functions */

static char* GetFileName(const char* pathname)
{
	const char* filename = nsnull;

	// this is most likely a path, so skip to the filename
	filename = PL_strrchr(pathname, '/');
	if(filename)
		++filename;
	else
		filename = pathname;

	return PL_strdup(filename);
}

static char* GetKeyValue(char* verbuf, char* key)
{
	char        *buf = NULL;
	UINT        blen;

	::VerQueryValue(verbuf,
					TEXT(key),
					(void **)&buf, &blen);

	if(buf != NULL)
		return PL_strdup(buf);
	else
		return nsnull;
}

static PRUint32 CalculateVariantCount(char* mimeTypes)
{
	PRUint32 variants = 0;
	char* index = mimeTypes;
	while (*index)
	{
		if (*index == '|')
			variants++;

		++index;
	}
	return variants;
}

static char** MakeStringArray(PRUint32 variants, char* data)
{
	char** buffer = NULL;
	char* index = data;
	PRUint32 count = 0;
	
	buffer = (char **)PR_Malloc(variants * sizeof(char *));
	buffer[count++] = index;

	while (*index && count<variants)
	{
		if (*index == '|')
		{
		  buffer[count++] = index + 1;
		  *index = 0;
		}
		++index;
	}
	return buffer;
}

///////////////////////////////////////////////////////////////////////////

/* nsPluginsDir implementation */

nsPluginsDir::nsPluginsDir()
{
  HKEY keyloc; 
  DWORD pathlen, type; 
  long result;
  char path[2000];
  const char* allocPath;
  //PRDir *dir = nsnull; 

  // first, let's look for a plugin folder that exists in the
  // same directory as our executable
  if (::GetModuleFileName(NULL, path, sizeof(path)) > 0) 
  { 
    pathlen = PL_strlen(path) - 1; 

    while (pathlen > 0) 
    { 
      if (path[pathlen] == '\\') 
        break; 

      pathlen--; 
    }

    if (pathlen > 0) 
    { 
      PL_strcpy(&path[pathlen + 1], "plugins"); 
      //dir = PR_OpenDir(path);

	  allocPath = PL_strdup(path);
	  *(nsFileSpec*)this = allocPath;
    } 
  } 

  // if we didn't find it, let's look for the plugin folder
  // that the user has in their Communicator 4x install
  if(!Exists())
  { 
    path[0] = 0; 
	//PL_strfree(allocPath);

    result = ::RegOpenKeyEx(HKEY_CURRENT_USER, 
                            "Software\\Netscape\\Netscape Navigator\\Main", 
                            0, KEY_READ, &keyloc); 

    if (result == ERROR_SUCCESS) 
    { 
      pathlen = sizeof(path); 

      result = ::RegQueryValueEx(keyloc, "Install Directory", 
                                 NULL, &type, (LPBYTE)&path, &pathlen); 

      if (result == ERROR_SUCCESS) 
      { 
        PL_strcat(path, "\\Program\\Plugins"); 
      } 

      ::RegCloseKey(keyloc); 
    } 

    //dir = PR_OpenDir(path);
	  allocPath = PL_strdup(path);
	  *(nsFileSpec*)this = allocPath;
  } 

#ifdef NS_DEBUG 
  if (path[0] != 0) 
    printf("plugins at: %s\n", path); 
#endif 
}

nsPluginsDir::~nsPluginsDir()
{
	// do nothing
}

PRBool nsPluginsDir::IsPluginFile(const nsFileSpec& fileSpec)
{
	const char* filename;
	char* extension;
	PRUint32 len;
	const char* pathname = fileSpec.GetCString();

	// this is most likely a path, so skip to the filename
	filename = PL_strrchr(pathname, '\\');
	if(filename)
		++filename;
	else
		filename = pathname;

	len = PL_strlen(filename);
	// the filename must be: "np*.dll"
	extension = PL_strrchr(filename, '.');
	if(extension)
	    ++extension;

	if(len > 5)
	{
		if(!PL_strncasecmp(filename, "np", 2) && !PL_strcasecmp(extension, "dll"))
			return PR_TRUE;
	}

	return PR_FALSE;
}

///////////////////////////////////////////////////////////////////////////

/* nsPluginFile implementation */

nsPluginFile::nsPluginFile(const nsFileSpec& spec)
	:	nsFileSpec(spec)
{
	// nada
}

nsPluginFile::~nsPluginFile()
{
	// nada
}

/**
 * Loads the plugin into memory using NSPR's shared-library loading
 * mechanism. Handles platform differences in loading shared libraries.
 */
nsresult nsPluginFile::LoadPlugin(PRLibrary* &outLibrary)
{
	// How can we convert to a full path names for using with NSPR?
	//nsFilePath path(*this);
	const char* path = this->GetCString();
	outLibrary = PR_LoadLibrary(path);
	return NS_OK;
}

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info)
{
	DWORD zerome, versionsize;
	char* verbuf = nsnull;

	const char* path = this->GetCString();
	versionsize = ::GetFileVersionInfoSize((char*)path, &zerome);
	if (versionsize > 0)
		verbuf = (char *)PR_Malloc(versionsize);
	if(!verbuf)
		return NS_ERROR_OUT_OF_MEMORY;

	if(::GetFileVersionInfo((char*)path, NULL, versionsize, verbuf))
	{
		info.fName = GetFileName(path);
		info.fDescription = GetKeyValue(verbuf, "\\StringFileInfo\\040904E4\\FileDescription");

		info.fMimeType = GetKeyValue(verbuf, "\\StringFileInfo\\040904E4\\MIMEType");
		info.fMimeDescription = GetKeyValue(verbuf, "\\StringFileInfo\\040904E4\\FileOpenName");
		info.fExtensions = GetKeyValue(verbuf, "\\StringFileInfo\\040904E4\\FileExtents");

		info.fVariantCount = CalculateVariantCount(info.fMimeType);
		info.fMimeTypeArray = MakeStringArray(info.fVariantCount, info.fMimeType);
		info.fMimeDescriptionArray = MakeStringArray(info.fVariantCount, info.fMimeDescription);
		info.fExtensionArray = MakeStringArray(info.fVariantCount, info.fExtensions);
	}
	else
		return NS_ERROR_FAILURE;

	return NS_OK;
}


