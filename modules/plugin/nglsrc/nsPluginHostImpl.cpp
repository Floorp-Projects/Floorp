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

#include "nsPluginHostImpl.h"
#include <stdio.h>
#include "prio.h"
#include "prmem.h"
#include "ns4xPlugin.h"

#ifdef XP_PC
#include "windows.h"
#endif

nsPluginTag :: nsPluginTag()
{
  mNext = nsnull;
  mName = nsnull;
  mDescription = nsnull;
  mMimeType = nsnull;
  mMimeDescription = nsnull;
  mExtensions = nsnull;
  mVariants = 0;
  mMimeTypeArray = nsnull;
  mMimeDescriptionArray = nsnull;
  mExtensionsArray = nsnull;
  mLibrary = nsnull;
  mEntryPoint = nsnull;
  mFlags = NS_PLUGIN_FLAG_ENABLED;
}

nsPluginTag :: ~nsPluginTag()
{
  if (nsnull != mName)
  {
    PR_Free(mName);
    mName = nsnull;
  }

  if (nsnull != mDescription)
  {
    PR_Free(mDescription);
    mDescription = nsnull;
  }

  if (nsnull != mMimeType)
  {
    PR_Free(mMimeType);
    mMimeType = nsnull;
  }

  if (nsnull != mMimeDescription)
  {
    PR_Free(mMimeDescription);
    mMimeDescription = nsnull;
  }

  if (nsnull != mExtensions)
  {
    PR_Free(mExtensions);
    mExtensions = nsnull;
  }

  if (nsnull != mMimeTypeArray)
  {
    PR_Free(mMimeTypeArray);
    mMimeTypeArray = nsnull;
  }

  if (nsnull != mMimeDescriptionArray)
  {
    PR_Free(mMimeDescriptionArray);
    mMimeDescriptionArray = nsnull;
  }

  if (nsnull != mExtensionsArray)
  {
    PR_Free(mExtensionsArray);
    mExtensionsArray = nsnull;
  }

  if (nsnull != mLibrary)
  {
    PR_UnloadLibrary(mLibrary);
    mLibrary = nsnull;
  }

  mEntryPoint = nsnull;
}

static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginHostIID, NS_IPLUGINHOST_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsPluginHostImpl :: nsPluginHostImpl()
{
}

nsPluginHostImpl :: ~nsPluginHostImpl()
{
  if (nsnull != mPluginPath)
  {
    PR_Free(mPluginPath);
    mPluginPath = nsnull;
  }

  while (nsnull != mPlugins)
  {
    nsPluginTag *temp = mPlugins->mNext;
    delete mPlugins;
    mPlugins = temp;
  }
}

NS_IMPL_ADDREF(nsPluginHostImpl)
NS_IMPL_RELEASE(nsPluginHostImpl)

nsresult nsPluginHostImpl :: QueryInterface(const nsIID& aIID,
                                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIPluginManagerIID))
  {
    *aInstancePtrResult = (void *)((nsIPluginManager *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kIPluginHostIID))
  {
    *aInstancePtrResult = (void *)((nsIPluginHost *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID))
  {
    *aInstancePtrResult = (void *)((nsISupports *)((nsIPluginHost *)this));
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsresult nsPluginHostImpl :: ReloadPlugins(PRBool reloadPages)
{
  return LoadPlugins();
}

//XXX need to find out score on this one... MMP
nsresult nsPluginHostImpl :: UserAgent(const char **retstring)
{
  *retstring = "NGLayout";
  return NS_OK;
}

nsresult nsPluginHostImpl :: GetValue(nsPluginManagerVariable variable, void *value)
{
  return NS_OK;
}

nsresult nsPluginHostImpl :: SetValue(nsPluginManagerVariable variable, void *value)
{
  return NS_OK;
}

//nsresult nsPluginHostImpl :: FetchURL(nsISupports* peer, nsURLInfo* urlInfo)
//{
//  return NS_OK;
//}

nsresult nsPluginHostImpl :: Init(void)
{
  return NS_OK;
}

nsresult nsPluginHostImpl :: LoadPlugins(void)
{
#ifdef XP_PC
  long result;
  HKEY keyloc;
  DWORD type, pathlen;
  char path[2000];

  path[0] = 0;

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
      strcat(path, "\\Program\\Plugins");
      printf("plugins at: %s\n", path);
    }

    ::RegCloseKey(keyloc);
  }

  PRDir *dir;

  dir = PR_OpenDir(path);

  if (nsnull != dir)
  {
    PRDirEntry  *dent;
    char        *verbuf = NULL;
    DWORD       verbufsize = 0;

    pathlen = strlen(path);
    mPluginPath = (char *)PR_Malloc(pathlen + 2);

    if (nsnull != mPluginPath)
    {
      strcpy(mPluginPath, path);

      mPluginPath[pathlen] = '\\';
      mPluginPath[pathlen + 1] = 0;
    }

    while (nsnull != mPlugins)
    {
      nsPluginTag *temp = mPlugins->mNext;
      delete mPlugins;
      mPlugins = temp;
    }

    while (dent = PR_ReadDir(dir, PR_SKIP_BOTH))
    {
      PRInt32 len = strlen(dent->name);

      if (len > 6)  //np*.dll
      {
        if ((0 == stricmp(&dent->name[len - 4], ".dll")) && //ends in '.dll'
            (0 == strnicmp(dent->name, "np", 2)))           //starts with 'np'
        {
          PRLibrary *plugin;

          strcpy(path, mPluginPath);
          strcat(path, dent->name);

          plugin = PR_LoadLibrary(path);

          if (NULL != plugin)
          {
            DWORD zerome, versionsize;

            versionsize = ::GetFileVersionInfoSize(path, &zerome);

            if (versionsize > 0)
            {
              if (versionsize > verbufsize)
              {
                if (nsnull != verbuf)
                  PR_Free(verbuf);

                verbuf = (char *)PR_Malloc(versionsize);
                verbufsize = versionsize;
              }

              if ((nsnull != verbuf) && ::GetFileVersionInfo(path, NULL, verbufsize, verbuf))
              {
                char        *buf = NULL;
                UINT        blen;
                nsPluginTag *plugintag;
                PRBool      completetag = PR_FALSE;
                PRInt32     variants;

                plugintag = new nsPluginTag();

                while (nsnull != plugintag)
                {
                  plugintag->mName = (char *)PR_Malloc(strlen(dent->name) + 1);

                  if (nsnull == plugintag->mName)
                    break;
                  else
                    strcpy(plugintag->mName, dent->name);

                  ::VerQueryValue(verbuf,
                                  TEXT("\\StringFileInfo\\040904E4\\FileDescription"),
                                  (void **)&buf, &blen);

                  if (NULL == buf)
                    break;
                  else
                  {
                    plugintag->mDescription = (char *)PR_Malloc(blen + 1);

                    if (nsnull == plugintag->mDescription)
                      break;
                    else
                      strcpy(plugintag->mDescription, buf);
                  }

                  ::VerQueryValue(verbuf,
                                  TEXT("\\StringFileInfo\\040904E4\\MIMEType"),
                                  (void **)&buf, &blen);

                  if (NULL == buf)
                    break;
                  else
                  {
                    plugintag->mMimeType = (char *)PR_Malloc(blen + 1);

                    if (nsnull == plugintag->mMimeType)
                      break;
                    else
                      strcpy(plugintag->mMimeType, buf);

                    buf = plugintag->mMimeType;

                    variants = 1;

                    while (*buf)
                    {
                      if (*buf == '|')
                        variants++;

                      buf++;
                    }

                    plugintag->mVariants = variants;

                    plugintag->mMimeTypeArray = (char **)PR_Malloc(variants * sizeof(char *));

                    if (nsnull == plugintag->mMimeTypeArray)
                      break;
                    else
                    {
                      variants = 0;

                      plugintag->mMimeTypeArray[variants++] = plugintag->mMimeType;

                      buf = plugintag->mMimeType;

                      while (*buf)
                      {
                        if (*buf == '|')
                        {
                          plugintag->mMimeTypeArray[variants++] = buf + 1;
                          *buf = 0;
                        }

                        buf++;
                      }
                    }
                  }

                  ::VerQueryValue(verbuf,
                                  TEXT("\\StringFileInfo\\040904E4\\FileOpenName"),
                                  (void **)&buf, &blen);

                  if (NULL == buf)
                    break;
                  else
                  {
                    plugintag->mMimeDescription = (char *)PR_Malloc(blen + 1);

                    if (nsnull == plugintag->mMimeDescription)
                      break;
                    else
                      strcpy(plugintag->mMimeDescription, buf);

                    buf = plugintag->mMimeDescription;

                    variants = 1;

                    while (*buf)
                    {
                      if (*buf == '|')
                        variants++;

                      buf++;
                    }

                    if (variants != plugintag->mVariants)
                      break;

                    plugintag->mMimeDescriptionArray = (char **)PR_Malloc(variants * sizeof(char *));

                    if (nsnull == plugintag->mMimeDescriptionArray)
                      break;
                    else
                    {
                      variants = 0;

                      plugintag->mMimeDescriptionArray[variants++] = plugintag->mMimeDescription;

                      buf = plugintag->mMimeDescription;

                      while (*buf)
                      {
                        if (*buf == '|')
                        {
                          plugintag->mMimeDescriptionArray[variants++] = buf + 1;
                          *buf = 0;
                        }

                        buf++;
                      }
                    }
                  }

                  ::VerQueryValue(verbuf,
                                  TEXT("\\StringFileInfo\\040904E4\\FileExtents"),
                                  (void **)&buf, &blen);

                  if (NULL == buf)
                    break;
                  else
                  {
                    plugintag->mExtensions = (char *)PR_Malloc(blen + 1);

                    if (nsnull == plugintag->mExtensions)
                      break;
                    else
                      strcpy(plugintag->mExtensions, buf);

                    buf = plugintag->mExtensions;

                    variants = 1;

                    while (*buf)
                    {
                      if (*buf == '|')
                        variants++;

                      buf++;
                    }

                    if (variants != plugintag->mVariants)
                      break;

                    plugintag->mExtensionsArray = (char **)PR_Malloc(variants * sizeof(char *));

                    if (nsnull == plugintag->mExtensionsArray)
                      break;
                    else
                    {
                      variants = 0;

                      plugintag->mExtensionsArray[variants++] = plugintag->mExtensions;

                      buf = plugintag->mExtensions;

                      while (*buf)
                      {
                        if (*buf == '|')
                        {
                          plugintag->mExtensionsArray[variants++] = buf + 1;
                          *buf = 0;
                        }

                        buf++;
                      }
                    }
                  }

                  completetag = PR_TRUE;
                  break;
                }

                if (PR_FALSE == completetag)
                  delete plugintag;
                else
                {
                  if (nsnull == PR_FindSymbol(plugin, "NSGetFactory"))
                    plugintag->mFlags |= NS_PLUGIN_FLAG_OLDSCHOOL;

printf("plugin %s added to list %s\n", plugintag->mName, (plugintag->mFlags & NS_PLUGIN_FLAG_OLDSCHOOL) ? "(old school)" : "");
                  plugintag->mNext = mPlugins;
                  mPlugins = plugintag;
                }
              }
            }

            PR_UnloadLibrary(plugin);
          }
        }
      }
    }

    if (nsnull != verbuf)
      PR_Free(verbuf);

    PR_CloseDir(dir);
  }

#else
  printf("Don't know how to locate plugins directory on Unix yet...\n");
#endif

  return NS_OK;
}

nsresult nsPluginHostImpl :: InstantiatePlugin(char *aMimeType, nsISupports ** aPluginInst)
{
  nsPluginTag *plugins = mPlugins;
  PRInt32     variants, cnt;

  while (nsnull != plugins)
  {
    variants = plugins->mVariants;

    for (cnt = 0; cnt < variants; cnt++)
    {
      if (0 == strcmp(plugins->mMimeTypeArray[cnt], aMimeType))
        break;
    }

    if (cnt < variants)
      break;

    plugins = plugins->mNext;
  }

  if (nsnull != plugins)
  {
    if (nsnull == plugins->mLibrary)
    {
      char        path[2000];

      strcpy(path, mPluginPath);
      strcat(path, plugins->mName);

      plugins->mLibrary = PR_LoadLibrary(path);
printf("loaded plugin %s for mime type %s\n", plugins->mName, aMimeType);
    }

    if (nsnull != plugins->mLibrary)
    {
      if (nsnull == plugins->mEntryPoint)
      {
        if (plugins->mFlags & NS_PLUGIN_FLAG_OLDSCHOOL)
          plugins->mEntryPoint = PR_FindSymbol(plugins->mLibrary, "NP_Initialize");
        else
          plugins->mEntryPoint = PR_FindSymbol(plugins->mLibrary, "NSGetFactory");
      }
    }
    else
      return NS_ERROR_UNEXPECTED;

    return NS_OK;
  }
  else
  {
printf("unable to find plugin to handle %s\n", aMimeType);

    return NS_ERROR_FAILURE;
  }
}
