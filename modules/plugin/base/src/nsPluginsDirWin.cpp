/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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

/* Local helper functions */

static char* GetKeyValue(TCHAR* verbuf, TCHAR* key)
{
  TCHAR *buf = NULL;
  UINT blen;

  ::VerQueryValue(verbuf, key, (void **)&buf, &blen);

  if (buf) {
#ifdef UNICODE
    // the return value needs to always be a char *, regardless
    // of whether we're UNICODE or not
    return PL_strdup(NS_ConvertUTF16toUTF8(buf).get());
#else
    return PL_strdup(buf);
#endif
  }

  return nsnull;
}

static char* GetVersion(TCHAR* verbuf)
{
  VS_FIXEDFILEINFO *fileInfo;
  UINT fileInfoLen;

  ::VerQueryValue(verbuf, TEXT("\\"), (void **)&fileInfo, &fileInfoLen);

  if (fileInfo) {
    return PR_smprintf("%ld.%ld.%ld.%ld",
                       HIWORD(fileInfo->dwFileVersionMS),
                       LOWORD(fileInfo->dwFileVersionMS),
                       HIWORD(fileInfo->dwFileVersionLS),
                       LOWORD(fileInfo->dwFileVersionLS));
  }

  return nsnull;
}

static PRUint32 CalculateVariantCount(char* mimeTypes)
{
  PRUint32 variants = 1;

  if (!mimeTypes)
    return 0;

  char* index = mimeTypes;
  while (*index) {
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

  if ((variants <= 0) || !data)
    return NULL;

  char ** array = (char **)PR_Calloc(variants, sizeof(char *));
  if (!array)
    return NULL;

  char * start = data;

  for (PRUint32 i = 0; i < variants; i++) {
    char * p = PL_strchr(start, '|');
    if (p)
      *p = 0;

    array[i] = PL_strdup(start);

    if (!p) {
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
  if ((variants == 0) || !array)
    return;

  for (PRUint32 i = 0; i < variants; i++) {
    if (array[i]) {
      PL_strfree(array[i]);
      array[i] = NULL;
    }
  }
  PR_Free(array);
}

/* nsPluginsDir implementation */

// The file name must be in the form "np*.dll"
PRBool nsPluginsDir::IsPluginFile(nsIFile* file)
{
  nsCAutoString path;
  if (NS_FAILED(file->GetNativePath(path)))
    return PR_FALSE;

  const char *cPath = path.get();

  // this is most likely a path, so skip to the filename
  const char* filename = PL_strrchr(cPath, '\\');
  if (filename)
    ++filename;
  else
    filename = cPath;

  char* extension = PL_strrchr(filename, '.');
  if (extension)
    ++extension;

  PRUint32 fullLength = PL_strlen(filename);
  PRUint32 extLength = PL_strlen(extension);
  if (fullLength >= 7 && extLength == 3) {
    if (!PL_strncasecmp(filename, "np", 2) && !PL_strncasecmp(extension, "dll", 3)) {
      // don't load OJI-based Java plugins
      if (!PL_strncasecmp(filename, "npoji", 5) ||
          !PL_strncasecmp(filename, "npjava", 6))
        return PR_FALSE;
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

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

#ifndef WINCE
  char* index;
  char* pluginFolderPath = PL_strdup(temp.get());

  index = PL_strrchr(pluginFolderPath, '\\');
  if (!index) {
    PL_strfree(pluginFolderPath);
    return NS_ERROR_FILE_INVALID_PATH;
  }
  *index = 0;

  BOOL restoreOrigDir = FALSE;
  char aOrigDir[MAX_PATH + 1];
  DWORD dwCheck = ::GetCurrentDirectory(sizeof(aOrigDir), aOrigDir);
  NS_ASSERTION(dwCheck <= MAX_PATH + 1, "Error in Loading plugin");

  if (dwCheck <= MAX_PATH + 1) {
    restoreOrigDir = ::SetCurrentDirectory(pluginFolderPath);
    NS_ASSERTION(restoreOrigDir, "Error in Loading plugin");
  }
#endif

  outLibrary = PR_LoadLibrary(temp.get());

#ifndef WINCE    
  if (restoreOrigDir) {
    BOOL bCheck = ::SetCurrentDirectory(aOrigDir);
    NS_ASSERTION(bCheck, "Error in Loading plugin");
  }

  PL_strfree(pluginFolderPath);
#endif

  return NS_OK;
}

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info)
{
  nsresult rv = NS_OK;
  DWORD zerome, versionsize;
  TCHAR* verbuf = nsnull;

  const TCHAR* path;

  if (!mPlugin)
    return NS_ERROR_NULL_POINTER;

  nsCAutoString fullPath;
  if (NS_FAILED(rv = mPlugin->GetNativePath(fullPath)))
    return rv;

  nsCAutoString fileName;
  if (NS_FAILED(rv = mPlugin->GetNativeLeafName(fileName)))
    return rv;

#ifdef UNICODE
  NS_ConvertASCIItoUTF16 utf16Path(fullPath);
  path = utf16Path.get();
  versionsize = ::GetFileVersionInfoSizeW((TCHAR*)path, &zerome);
#else
  path = fullPath.get();
  versionsize = ::GetFileVersionInfoSize((TCHAR*)path, &zerome);
#endif

  if (versionsize > 0)
    verbuf = (TCHAR*)PR_Malloc(versionsize);
  if (!verbuf)
    return NS_ERROR_OUT_OF_MEMORY;

#ifdef UNICODE
  if (::GetFileVersionInfoW((LPWSTR)path, NULL, versionsize, verbuf))
#else
  if (::GetFileVersionInfo(path, NULL, versionsize, verbuf))
#endif
  {
    info.fName = GetKeyValue(verbuf, TEXT("\\StringFileInfo\\040904E4\\ProductName"));
    info.fDescription = GetKeyValue(verbuf, TEXT("\\StringFileInfo\\040904E4\\FileDescription"));

    char *mimeType = GetKeyValue(verbuf, TEXT("\\StringFileInfo\\040904E4\\MIMEType"));
    char *mimeDescription = GetKeyValue(verbuf, TEXT("\\StringFileInfo\\040904E4\\FileOpenName"));
    char *extensions = GetKeyValue(verbuf, TEXT("\\StringFileInfo\\040904E4\\FileExtents"));

    info.fVariantCount = CalculateVariantCount(mimeType);
    info.fMimeTypeArray = MakeStringArray(info.fVariantCount, mimeType);
    info.fMimeDescriptionArray = MakeStringArray(info.fVariantCount, mimeDescription);
    info.fExtensionArray = MakeStringArray(info.fVariantCount, extensions);
    info.fFullPath = PL_strdup(fullPath.get());
    info.fFileName = PL_strdup(fileName.get());
    info.fVersion = GetVersion(verbuf);

    PL_strfree(mimeType);
    PL_strfree(mimeDescription);
    PL_strfree(extensions);
  }
  else {
    rv = NS_ERROR_FAILURE;
  }

  PR_Free(verbuf);

  return rv;
}

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
  if (info.fName)
    PL_strfree(info.fName);

  if (info.fDescription)
    PL_strfree(info.fDescription);

  if (info.fMimeTypeArray)
    FreeStringArray(info.fVariantCount, info.fMimeTypeArray);

  if (info.fMimeDescriptionArray)
    FreeStringArray(info.fVariantCount, info.fMimeDescriptionArray);

  if (info.fExtensionArray)
    FreeStringArray(info.fVariantCount, info.fExtensionArray);

  if (info.fFullPath)
    PL_strfree(info.fFullPath);

  if (info.fFileName)
    PL_strfree(info.fFileName);

  if (info.fVersion)
    PR_smprintf_free(info.fVersion);

  ZeroMemory((void *)&info, sizeof(info));

  return NS_OK;
}
