/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
	nsPluginsDirWin.cpp
	
	Windows implementation of the nsPluginsDir/nsPluginsFile classes.
	
	by Alex Musil
 */

#include "nsPluginsDir.h"
#include "prlink.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"

#include "windows.h"
#include "winbase.h"

#include "nsString.h"

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
	PRUint32 variants = 1;

  if(mimeTypes == NULL)
    return 0;

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
// The number of variants has been calculated based on the mime
// type array. Plugins are not explicitely required to match
// this number in two other arrays: file extention array and mime
// description array, and some of them actually don't. 
// We should handle such situations gracefully

  if((variants <= 0) || (data == NULL))
    return NULL;

  char ** array = (char **)PR_Calloc(variants, sizeof(char *));
  if(array == NULL)
    return NULL;

  char * start = data;

  for(PRUint32 i = 0; i < variants; i++)
  {
    char * p = PL_strchr(start, '|');
    if(p != NULL)
      *p = 0;

    array[i] = PL_strdup(start);

    if(p == NULL)
    { 
      // nothing more to look for, fill everything left 
      // with empty strings and break
      while(++i < variants)
        array[i] = PL_strdup("");

      break;
    }

    start = ++p;
  }
  return array;
}

static void FreeStringArray(PRUint32 variants, char ** array)
{
  if((variants == 0) || (array == NULL))
    return;

  for(PRUint32 i = 0; i < variants; i++)
  {
    if(array[i] != NULL)
    {
      PL_strfree(array[i]);
      array[i] = NULL;
    }
  }
  PR_Free(array);
}
///////////////////////////////////////////////////////////////////////////

/* nsPluginsDir implementation */

PRBool nsPluginsDir::IsPluginFile(nsIFile* file)
{
    PRBool ret = PR_FALSE;
    nsCAutoString path;
    if (NS_FAILED(file->GetNativePath(path)))
        return PR_FALSE;

    const char *pathname = path.get();
	const char* filename;
	char* extension;
	PRUint32 len;
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
    return ret;
}

///////////////////////////////////////////////////////////////////////////

/* nsPluginFile implementation */

nsPluginFile::nsPluginFile(nsIFile* file)
: mPlugin(file)
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
    if (!mPlugin)
        return NS_ERROR_NULL_POINTER;

    nsCAutoString temp;
    mPlugin->GetNativePath(temp);

    char* index;
    char* pluginFolderPath = PL_strdup(temp.get());
    
    index = PL_strrchr(pluginFolderPath, '\\');
    *index = 0;
    
	BOOL restoreOrigDir = FALSE;
	char aOrigDir[MAX_PATH + 1];
	DWORD dwCheck = ::GetCurrentDirectory(sizeof(aOrigDir), aOrigDir);
	NS_ASSERTION(dwCheck <= MAX_PATH + 1, "Error in Loading plugin");

	if (dwCheck <= MAX_PATH + 1)
  	{
		restoreOrigDir = ::SetCurrentDirectory(pluginFolderPath);
		NS_ASSERTION(restoreOrigDir, "Error in Loading plugin");
    }
    
	outLibrary = PR_LoadLibrary(temp.get());
    
	if (restoreOrigDir)
	{
        BOOL bCheck = ::SetCurrentDirectory(aOrigDir);
		NS_ASSERTION(bCheck, "Error in Loading plugin");
    }
    
    PL_strfree(pluginFolderPath);
    
	return NS_OK;
}

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info)
{
    nsresult res = NS_OK;
	DWORD zerome, versionsize;
	char* verbuf = nsnull;

	const char* path;

    if (!mPlugin)
        return NS_ERROR_NULL_POINTER;

    nsCAutoString temp;
    mPlugin->GetNativePath(temp);
    path = temp.get();
    
    versionsize = ::GetFileVersionInfoSize((char*)path, &zerome);
	if (versionsize > 0)
		verbuf = (char *)PR_Malloc(versionsize);
	if(!verbuf)
		return NS_ERROR_OUT_OF_MEMORY;
    
	if(::GetFileVersionInfo((char*)path, NULL, versionsize, verbuf))
    {
        info.fName = GetKeyValue(verbuf, "\\StringFileInfo\\040904E4\\ProductName");
		info.fDescription = GetKeyValue(verbuf, "\\StringFileInfo\\040904E4\\FileDescription");

		char *mimeType = GetKeyValue(verbuf, "\\StringFileInfo\\040904E4\\MIMEType");
		char *mimeDescription = GetKeyValue(verbuf, "\\StringFileInfo\\040904E4\\FileOpenName");
		char *extensions = GetKeyValue(verbuf, "\\StringFileInfo\\040904E4\\FileExtents");

		info.fVariantCount = CalculateVariantCount(mimeType);
		info.fMimeTypeArray = MakeStringArray(info.fVariantCount, mimeType);
		info.fMimeDescriptionArray = MakeStringArray(info.fVariantCount, mimeDescription);
		info.fExtensionArray = MakeStringArray(info.fVariantCount, extensions);
        info.fFileName = PL_strdup(path);
        
        PL_strfree(mimeType);
        PL_strfree(mimeDescription);
        PL_strfree(extensions);
	}
	else
		res = NS_ERROR_FAILURE;


	PR_Free(verbuf);

  return res;
}

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
  if(info.fName != NULL)
    PL_strfree(info.fName);

  if(info.fDescription != NULL)
    PL_strfree(info.fDescription);

  if(info.fMimeTypeArray != NULL)
    FreeStringArray(info.fVariantCount, info.fMimeTypeArray);

  if(info.fMimeDescriptionArray != NULL)
    FreeStringArray(info.fVariantCount, info.fMimeDescriptionArray);

  if(info.fExtensionArray != NULL)
    FreeStringArray(info.fVariantCount, info.fExtensionArray);

  if(info.fFileName != NULL)
    PL_strfree(info.fFileName);

  ZeroMemory((void *)&info, sizeof(info));

  return NS_OK;
}
