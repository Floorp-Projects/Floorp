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
#include "nsMalloc.h"     //this is evil...
#include "nsPluginInstancePeer.h"
#include "nsPluginStreamPeer.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIInputStream.h"

#ifdef XP_PC
#include "windows.h"
#endif

static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID); 
static NS_DEFINE_IID(kIPluginInstancePeerIID, NS_IPLUGININSTANCEPEER_IID); 
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kINetworkManagerIID, NS_INETWORKMANAGER_IID);
static NS_DEFINE_IID(kIPluginHostIID, NS_IPLUGINHOST_IID);
static NS_DEFINE_IID(kIPluginStreamPeerIID, NS_IPLUGINSTREAMPEER_IID);
static NS_DEFINE_IID(kIMallocIID, NS_IMALLOC_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);

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

class nsPluginStreamListener : public nsIStreamListener
{
public:
  nsPluginStreamListener();
  ~nsPluginStreamListener();

  NS_DECL_ISUPPORTS

  //nsIStreamObserver interface

  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);

  NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax);

  NS_IMETHOD OnStatus(nsIURL* aURL, const nsString &aMsg);

  NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 aStatus, const nsString &aMsg);

  //nsIStreamListener interface

  NS_IMETHOD GetBindInfo(nsIURL* aURL);

  NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
                             PRInt32 aLength);

  //locals

  nsresult Initialize(nsIURL *aURL, nsIPluginInstance *aInstance, void *aNotifyData);
  nsresult Initialize(nsIURL *aURL, nsIPluginInstance **aInstance,
                      nsPluginWindow *aWindow, nsIPluginHost *aHost);

private:
  nsIURL              *mURL;
  nsPluginStreamPeer  *mPeer;
  nsIPluginInstance   *mInstance;
  nsIPluginInstance   **mFarInstance;
  PRBool              mBound;
  nsIPluginStream     *mStream;
  char                *mMIMEType;
  PRUint8             *mBuffer;
  PRUint32            mBufSize;
  void                *mNotifyData;
  nsPluginWindow      *mWindow;
  nsIPluginHost       *mHost;
  PRInt32             mLength;
  PRBool              mGotProgress;
};

nsPluginStreamListener :: nsPluginStreamListener()
{
  mURL = nsnull;
  mPeer = nsnull;
  mInstance = nsnull;
  mFarInstance = nsnull;
  mBound = PR_FALSE;
  mStream = nsnull;
  mMIMEType = nsnull;
  mBuffer = nsnull;
  mBufSize = 0;
  mNotifyData = nsnull;
  mWindow = nsnull;
  mHost = nsnull;
  mLength = 0;
  mGotProgress = PR_FALSE;
}

nsPluginStreamListener :: ~nsPluginStreamListener()
{
  NS_IF_RELEASE(mURL);
  NS_IF_RELEASE(mPeer);
  NS_IF_RELEASE(mInstance);
  NS_IF_RELEASE(mStream);

  if (nsnull != mMIMEType)
  {
    PR_Free(mMIMEType);
    mMIMEType = nsnull;
  }

  if (nsnull != mBuffer)
  {
    PR_Free(mBuffer);
    mBuffer = nsnull;
  }

  mNotifyData = nsnull;
  mWindow = nsnull;
  mFarInstance = nsnull;

  NS_IF_RELEASE(mHost);
}

NS_IMPL_ADDREF(nsPluginStreamListener)
NS_IMPL_RELEASE(nsPluginStreamListener)

nsresult nsPluginStreamListener :: QueryInterface(const nsIID& aIID,
                                                  void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIStreamListenerIID))
  {
    *aInstancePtrResult = (void *)((nsIStreamListener *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kIStreamObserverIID))
  {
    *aInstancePtrResult = (void *)((nsIStreamObserver *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID))
  {
    *aInstancePtrResult = (void *)((nsISupports *)((nsIStreamListener *)this));
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsresult nsPluginStreamListener :: Initialize(nsIURL *aURL, nsIPluginInstance *aInstance, void *aNotifyData)
{
  mURL = aURL;
  NS_ADDREF(mURL);

  mInstance = aInstance;
  NS_ADDREF(mInstance);

  mNotifyData = aNotifyData;

  return NS_OK;
}

nsresult nsPluginStreamListener :: Initialize(nsIURL *aURL, nsIPluginInstance **aInstance,
                                              nsPluginWindow *aWindow, nsIPluginHost *aHost)
{
  mURL = aURL;
  NS_ADDREF(mURL);

  mFarInstance = aInstance;

  mWindow = aWindow;

  mHost = aHost;
  NS_IF_ADDREF(mHost);

  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListener :: OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  nsresult  rv = NS_OK;

  if (nsnull != aContentType)
  {
    PRInt32   len = strlen(aContentType);
    mMIMEType = (char *)PR_Malloc(len + 1);

    if (nsnull != mMIMEType)
      strcpy(mMIMEType, aContentType);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  //now that we have the mime type, see if we need
  //to load a plugin...

  if ((nsnull == mInstance) && (nsnull != mFarInstance) &&
      (nsnull != mHost) && (nsnull != mWindow))
  {
    rv = mHost->InstantiatePlugin(aContentType, aURL, mFarInstance);

    if (NS_OK == rv)
    {
      mInstance = *mFarInstance;
      NS_ADDREF(mInstance);

      (*mFarInstance)->Start();
      (*mFarInstance)->SetWindow(mWindow);
    }
  }

  if ((PR_TRUE == mGotProgress) && (nsnull == mPeer) &&
      (nsnull != mInstance) && (PR_FALSE == mBound))
  {
    //need to create new peer and and tell plugin that we have new stream...

    mPeer = (nsPluginStreamPeer *)new nsPluginStreamPeer();

    NS_ADDREF(mPeer);

    mPeer->Initialize(aURL, mLength, 0, aContentType, mNotifyData);
    mInstance->NewStream(mPeer, &mStream);
  }

  mBound = PR_TRUE;

  return rv;
}

NS_IMETHODIMP nsPluginStreamListener :: OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
  if ((aProgress == 0) && (nsnull == mPeer) && (nsnull != mInstance))
  {
    //need to create new peer and and tell plugin that we have new stream...

    mPeer = (nsPluginStreamPeer *)new nsPluginStreamPeer();

    NS_ADDREF(mPeer);

    mPeer->Initialize(aURL, aProgressMax, 0, mMIMEType, mNotifyData);
    mInstance->NewStream(mPeer, &mStream);
  }

  mLength = aProgressMax;
  mGotProgress = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListener :: OnStatus(nsIURL* aURL, const nsString &aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListener :: OnStopBinding(nsIURL* aURL, PRInt32 aStatus, const nsString &aMsg)
{
  if (nsnull != mStream)
    mStream->Close();

  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListener :: GetBindInfo(nsIURL* aURL)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListener :: OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
                                                        PRInt32 aLength)
{
  if ((PRUint32)aLength > mBufSize)
  {
    if (nsnull != mBuffer)
      PR_Free((void *)mBuffer);

    mBuffer = (PRUint8 *)PR_Malloc(aLength);
    mBufSize = aLength;
  }

  if ((nsnull != mBuffer) && (nsnull != mStream))
  {
    PRInt32 readlen;
    aIStream->Read((char *)mBuffer, 0, aLength, &readlen);
    mStream->Write((char *)mBuffer, 0, aLength, &readlen);
  }

  return NS_OK;
}

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

  NS_IF_RELEASE(mMalloc);
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

  if (aIID.Equals(kINetworkManagerIID))
  {
    *aInstancePtrResult = (void *)((nsINetworkManager *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kIFactoryIID))
  {
    *aInstancePtrResult = (void *)((nsIFactory *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kIMallocIID))
  {
    *aInstancePtrResult = mMalloc;
    NS_IF_ADDREF(mMalloc);
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

NS_IMETHODIMP nsPluginHostImpl :: GetURL(nsISupports* peer, const char* url,
                                         const char* target,
                                         void* notifyData, const char* altHost,
                                         const char* referrer, PRBool forceJSEnabled)
{
  nsAutoString  string = nsAutoString(url);
  nsIPluginInstance     *inst;
  nsresult  rv;

  rv = peer->QueryInterface(kIPluginInstanceIID, (void **)&inst);

  if (NS_OK == rv)
  {
    NewPluginStream(string, inst, notifyData);
    NS_RELEASE(inst);
  }

  return rv;
}

NS_IMETHODIMP nsPluginHostImpl :: PostURL(nsISupports* peer,
                                          const char* url, const char* target,
                                          PRUint32 postDataLen, const char* postData,
                                          PRBool isFile, void* notifyData,
                                          const char* altHost, const char* referrer,
                                          PRBool forceJSEnabled,
                                          PRUint32 postHeadersLength, const char* postHeaders)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginHostImpl::FindProxyForURL(const char* url, char* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsPluginHostImpl :: Init(void)
{
  nsresult rv;
  nsISupports *object;

  rv = nsMalloc::Create(nsnull, kIMallocIID, (void **)&object);

  if (NS_OK == rv)
  {
    rv = object->QueryInterface(kIMallocIID, (void **)&mMalloc);
    NS_RELEASE(object);
  }

  return rv;
}

nsresult nsPluginHostImpl :: LoadPlugins(void)
{
#ifdef XP_PC 
  long result; 
  HKEY keyloc; 
  DWORD type, pathlen; 
  char path[2000]; 
  PRDir *dir = nsnull; 

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
      dir = PR_OpenDir(path); 
    } 
  } 

  if (nsnull == dir) 
  { 
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
        PL_strcat(path, "\\Program\\Plugins"); 
      } 

      ::RegCloseKey(keyloc); 
    } 

    dir = PR_OpenDir(path); 
  } 

#ifdef NS_DEBUG 
  if (path[0] != 0) 
    printf("plugins at: %s\n", path); 
#endif 

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
            (0 == PL_strncasecmp(dent->name, "np", 2)))           //starts with 'np'
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

#ifdef NS_DEBUG
printf("plugin %s added to list %s\n", plugintag->mName, (plugintag->mFlags & NS_PLUGIN_FLAG_OLDSCHOOL) ? "(old school)" : "");
#endif
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
#ifdef NS_DEBUG
  printf("Don't know how to locate plugins directory on Unix yet...\n");
#endif
#endif

  return NS_OK;
}

nsresult nsPluginHostImpl :: InstantiatePlugin(const char *aMimeType, nsIURL *aURL,
                                               nsIPluginInstance ** aPluginInst)
{
  nsPluginTag *plugins = nsnull;
  PRInt32     variants, cnt;

  if (nsnull != aMimeType)
  {
    plugins = mPlugins;

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
  }

  if ((nsnull == plugins) && (nsnull != aURL))
  {
    const char  *name = aURL->GetSpec();
    PRInt32     len = strlen(name);

    //find the plugin by filename extension.

    if ((nsnull != name) && (len > 1))
    {
      len--;

      while ((name[len] != 0) && (name[len] != '.'))
        len--;

      if (name[len] == '.')
      {
        const char  *ext = name + len + 1;

        len = strlen(ext);

        plugins = mPlugins;

        while (nsnull != plugins)
        {
          variants = plugins->mVariants;

          for (cnt = 0; cnt < variants; cnt++)
          {
            char    *extensions = plugins->mExtensionsArray[cnt];
            char    *nextexten;
            PRInt32 extlen;

            while (nsnull != extensions)
            {
              nextexten = strchr(extensions, ',');

              if (nsnull != nextexten)
                extlen = nextexten - extensions;
              else
                extlen = strlen(extensions);

              if (extlen == len)
              {
                if (PL_strncasecmp(extensions, ext, extlen) == 0)
                  break;
              }

              if (nsnull != nextexten)
                extensions = nextexten + 1;
              else
                extensions = nsnull;
            }

            if (nsnull != extensions)
              break;
          }

          if (cnt < variants)
          {
            aMimeType = plugins->mMimeTypeArray[cnt];
#ifdef NS_DEBUG
printf("found plugin via extension %s\n", ext);
#endif
            break;
          }

          plugins = plugins->mNext;
        }
      }
    }
  }

  if (nsnull != plugins)
  {
    if (nsnull == plugins->mLibrary)
    {
      char        path[2000];

      strcpy(path, mPluginPath);
      strcat(path, plugins->mName);

      plugins->mLibrary = PR_LoadLibrary(path);
#ifdef NS_DEBUG
printf("loaded plugin %s for mime type %s\n", plugins->mName, aMimeType);
#endif
    }

    if (nsnull != plugins->mLibrary)
    {
      if (nsnull == plugins->mEntryPoint)
      {
        //create the plugin object

        if (plugins->mFlags & NS_PLUGIN_FLAG_OLDSCHOOL)
        {
          nsresult rv = ns4xPlugin::CreatePlugin(plugins->mLibrary, (nsIPlugin **)&plugins->mEntryPoint);
#ifdef NS_DEBUG
printf("result of creating plugin adapter: %d\n", rv);
#endif
        }
        else
          plugins->mEntryPoint = (nsIPlugin *)PR_FindSymbol(plugins->mLibrary, "NSGetFactory");

        if (nsnull != plugins->mEntryPoint)
          plugins->mEntryPoint->Initialize((nsISupports *)(nsIPluginManager *)this);
      }

      if (nsnull != plugins->mEntryPoint)
      {
        //create an instance

        if (NS_OK == plugins->mEntryPoint->CreateInstance(nsnull, kIPluginInstanceIID, (void **)aPluginInst))
        {
#ifdef NS_DEBUG
printf("successfully created plugin instance\n");
#endif
          nsPluginInstancePeerImpl *peer = new nsPluginInstancePeerImpl();

          peer->Initialize(*aPluginInst);     //this will not add a ref to the instance. MMP
          (*aPluginInst)->Initialize(peer);
        }
      }
    }
    else
      return NS_ERROR_UNEXPECTED;

    return NS_OK;
  }
  else
  {
    if ((nsnull != aURL) || (nsnull != aMimeType))
#ifdef NS_DEBUG
printf("unable to find plugin to handle %s\n", aMimeType ? aMimeType : "(mime type unspecified)")
#endif
    ;

    return NS_ERROR_FAILURE;
  }
}

nsresult nsPluginHostImpl :: InstantiatePlugin(const char *aMimeType, nsIPluginInstance ** aPluginInst,
                                               nsPluginWindow *aWindow, nsString& aURLSpec)
{
  nsresult  rv;

  rv = InstantiatePlugin(aMimeType, nsnull, aPluginInst);

  if ((rv != NS_OK) || (nsnull == aMimeType))
  {
    //either the plugin could not be identified based
    //on the mime type or there was no mime type

    if (aURLSpec.Length() > 0)
    {
      //we need to stream in enough to get the mime type...

      rv = NewPluginStream(aURLSpec, aPluginInst, aWindow);
    }
    else
      rv = NS_ERROR_FAILURE;
  }
  else
  {
    //we got a plugin built, now stream

    (*aPluginInst)->Start();
    (*aPluginInst)->SetWindow(aWindow);
    NewPluginStream(aURLSpec, *aPluginInst, nsnull);
  }

  return rv;
}

NS_IMETHODIMP nsPluginHostImpl :: NewPluginStream(const nsString& aURL, nsIPluginInstance *aInstance, void *aNotifyData)
{
  nsIURL            *mURL;
  nsPluginStreamListener *mListener = (nsPluginStreamListener *)new nsPluginStreamListener();

  if (NS_OK == NS_NewURL(&mURL, aURL))
  {
    mListener->Initialize(mURL, aInstance, aNotifyData);
    mURL->Open(mListener);
  }

  return NS_OK;
}

NS_IMETHODIMP nsPluginHostImpl :: NewPluginStream(const nsString& aURL, nsIPluginInstance **aInstance, nsPluginWindow *aWindow)
{
  nsIURL            *mURL;
  nsPluginStreamListener *mListener = (nsPluginStreamListener *)new nsPluginStreamListener();

  if (NS_OK == NS_NewURL(&mURL, aURL))
  {
    mListener->Initialize(mURL, aInstance, aWindow, (nsIPluginHost *)this);
    mURL->Open(mListener);
  }

  return NS_OK;
}

nsresult nsPluginHostImpl :: CreateInstance(nsISupports *aOuter,  
                                            const nsIID &aIID,  
                                            void **aResult)  
{  
  if (aResult == NULL)
    return NS_ERROR_NULL_POINTER;  

  nsISupports *inst = nsnull;

  if (aIID.Equals(kIPluginStreamPeerIID))
    inst = (nsISupports *)(nsIPluginStreamPeer *)new nsPluginStreamPeer();
  
  if (inst == NULL)
    return NS_ERROR_OUT_OF_MEMORY;  

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  

  return res;  
}  

nsresult nsPluginHostImpl :: LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  
