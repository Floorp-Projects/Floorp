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

#include "xp.h"

DWORD GetPluginsDir(char * path, DWORD maxsize)
{
  if(!path)
    return 0;

  path[0] = '\0';

#ifdef XP_WIN

  DWORD res = GetModuleFileName(NULL, path, maxsize);
  if(res == 0)
    return 0;

  if(path[strlen(path) - 1] == '\\')
    path[lstrlen(path) - 1] = '\0';

  char *p = strrchr(path, '\\');

  if(p)
    *p = '\0';

  strcat(path, "\\plugins");

#endif

#ifdef XP_UNIX
  // Implement UNIX version
#endif

#ifdef XP_MAC
  // Implement Mac version
#endif

  res = strlen(path);
  return res;
}

XP_HLIB LoadRealPlugin(char * mimetype)
{
  if(!mimetype || !strlen(mimetype))
    return NULL;

#ifdef XP_WIN

  BOOL bDone = FALSE;
  WIN32_FIND_DATA ffdataStruct;

  char szPath[_MAX_PATH];
  char szFileName[_MAX_PATH];

  GetPluginsDir(szPath, _MAX_PATH);

  strcpy(szFileName, szPath);
  strcat(szFileName, "\\00*");

  HANDLE handle = FindFirstFile(szFileName, &ffdataStruct);
  if(handle == INVALID_HANDLE_VALUE) 
  {
    FindClose(handle);
    return NULL;
  }

  DWORD versize = 0L;
  DWORD zero = 0L;
  char * verbuf = NULL;

  do
  {
    strcpy(szFileName, szPath);
    strcat(szFileName, "\\");
    strcat(szFileName, ffdataStruct.cFileName);
    if(!(ffdataStruct. dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
      versize = GetFileVersionInfoSize(szFileName, &zero);
	    if (versize > 0)
		    verbuf = new char[versize];
      else 
        continue;

      if(!verbuf)
		    continue;

      GetFileVersionInfo(szFileName, NULL, versize, verbuf);

      char *mimetypes = NULL;
      UINT len = 0;

      if(!VerQueryValue(verbuf, "\\StringFileInfo\\040904E4\\MIMEType", (void **)&mimetypes, &len)
         || !mimetypes || !len)
      {
        delete [] verbuf;
        continue;
      }

      // browse through a string of mimetypes
      mimetypes[len] = '\0';
      char * type = mimetypes;

      BOOL more = TRUE;
      while(more)
      {
        char * p = strchr(type, '|');
        if(p)
          *p = '\0';
        else
          more = FALSE;

        if(0 == stricmp(mimetype, type))
        {
          // this is it!
          delete [] verbuf;
          FindClose(handle);
          HINSTANCE hLib = LoadLibrary(szFileName);
          return hLib;
        }

        type = p;
        type++;
      }

      delete [] verbuf;
    }

  } while(FindNextFile(handle, &ffdataStruct));

  FindClose(handle);

#endif

#ifdef XP_UNIX
  // Implement UNIX version
#endif

#ifdef XP_MAC
  // Implement Mac version
#endif

  return NULL;
}

void UnloadRealPlugin(XP_HLIB hLib)
{
#ifdef XP_WIN
  if(!hLib)
    FreeLibrary(hLib);
#endif

#ifdef XP_UNIX
  // Implement UNIX version
#endif

#ifdef XP_MAC
  // Implement Mac version
#endif
}
