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

#include "nsSpecialSystemDirectory.h"


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

nsPluginsDir::nsPluginsDir(PRUint16 location)
{
  DWORD pathlen; 
  char path[_MAX_PATH];
  char newestPath[_MAX_PATH + 4]; // to prevent buffer overrun when adding /bin
  newestPath[0] = 0;
  const char* allocPath;
  // Use the Moz_BinDirectory
  nsSpecialSystemDirectory plugDir(nsSpecialSystemDirectory::Moz_BinDirectory);

  if((location == PLUGINS_DIR_LOCATION_AUTO) || (location == PLUGINS_DIR_LOCATION_MOZ_LOCAL))
  {
    // look for a plugin folder that exists in the same directory as 
    // the mozilla bin directory

    plugDir += "plugins";
    *(nsFileSpec*)this = plugDir;
    PR_snprintf(path, sizeof(path), "%s", (const char *) plugDir);
    
  }

  if(((location == PLUGINS_DIR_LOCATION_AUTO) && !Exists()) || 
    (location == PLUGINS_DIR_LOCATION_4DOTX))
  {
    // look for the plugin folder that the user has in their Communicator 4x install
    HKEY keyloc; 
    long result;
    DWORD type; 
    char szKey[512] = "Software\\Netscape\\Netscape Navigator";

    path[0] = 0; 
    result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ, &keyloc); 

    if (result == ERROR_SUCCESS) 
    {
      char current_version[80];
      DWORD length = sizeof(current_version);

      result = ::RegQueryValueEx(keyloc, "CurrentVersion", NULL, &type, (LPBYTE)&current_version, &length); 

      ::RegCloseKey(keyloc);
      PL_strcat(szKey, "\\");
      PL_strcat(szKey, current_version);
      PL_strcat(szKey, "\\Main");
      result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ, &keyloc); 
    }

    if (result == ERROR_SUCCESS) 
    { 
      pathlen = sizeof(path); 

      result = ::RegQueryValueEx(keyloc, "Plugins Directory", NULL, &type, (LPBYTE)&path, &pathlen); 
      ::RegCloseKey(keyloc); 
    } 

	  allocPath = path;
	  *(nsFileSpec*)this = allocPath;
  }

  if (PLUGINS_DIR_LOCATION_JAVA_JRE == location)
  {
    // Case for looking for the Java OJI plugin via the JRE install path
    HKEY baseloc;
    HKEY keyloc;
    FILETIME modTime = {0,0};  // initilize variables
    FILETIME curVer = {0,0};
    DWORD type; 
    DWORD index = 0;
    DWORD numChars = _MAX_PATH;
    char curKey[_MAX_PATH] = "Software\\JavaSoft\\Java Plug-in";

    LONG result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, curKey, 0, KEY_READ, &baseloc);

    // we must enumerate through the keys because what if there is more than one version?
    while (ERROR_SUCCESS == result)
    {
      path[0] = 0;
      numChars = _MAX_PATH;
      pathlen = sizeof(path);
      result = ::RegEnumKeyEx(baseloc, index, curKey, &numChars, NULL, NULL, NULL, &modTime);
      index++;
      if (ERROR_SUCCESS == result)
      {
        if (ERROR_SUCCESS == ::RegOpenKeyEx(baseloc, curKey, 0, KEY_QUERY_VALUE, &keyloc))
        {
          // we have a sub key
          if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, "JavaHome", NULL, &type, (LPBYTE)&path, &pathlen))
          {
            // Compare time stamps from registry lookup
            // Only use the key with the latest time stamp because there could be several
            //
            // NOTE: This may not be the highest version revsion number (szKey)
            // if the user installed an older version AFTER a newer one
            // This assumes the last version installed is the one the user wants to use
            // We can also tweak this for checking for a minimum version on szKey
            if (::CompareFileTime(&modTime,&curVer) >= 0 && atof(curKey) >= 1.3)
            {
              PL_strcpy(newestPath,path);
              curVer = modTime;
            }
            ::RegCloseKey(keyloc);
          }          
        }        
      }
    }

    ::RegCloseKey(baseloc);
    
    // if nothing is found, then don't add \bin dir
    // a null path may be returned so .Valid() should be used.
    if (newestPath[0] != 0)
      PL_strcat(newestPath,"\\bin");

    allocPath = newestPath;
    *(nsFileSpec*)this = allocPath;

#ifdef NS_DEBUG
  if (newestPath[0] != 0)
    PL_strcpy(path,newestPath);  // so the debug below will print out correct
#endif
  }

#ifdef NS_DEBUG 
  if (path[0] != 0) 
    printf("Searching for plugins at: %s\n", path); 
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
	const char* path = this->GetCString();
  char* index;
  char* pluginFolderPath = PL_strdup(path);

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

	outLibrary = PR_LoadLibrary(path);

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
	const char* path = this->GetCString();

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
