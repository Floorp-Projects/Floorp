/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


#include "nsSilentDownload.h"
#include "nsSilentDownloadPrivate.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"

#include "nsIComponentManager.h"

#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"

#include "pratom.h"
#include "prmem.h"
#include "prio.h"
#include "mkutils.h"
#include "prefapi.h"

#include "nsIURL.h"
#include "nsINetlibURL.h"
#include "nsINetService.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"

#include "nsIDOMSilentDownload.h"
#include "nsIDOMSilentDownloadTask.h"

/* For Javascript Namespace Access */
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

// Globals

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kIScriptNameSetRegistryIID, NS_ISCRIPTNAMESETREGISTRY_IID);
static NS_DEFINE_IID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);
static NS_DEFINE_IID(kIScriptExternalNameSetIID, NS_ISCRIPTEXTERNALNAMESET_IID);

static NS_DEFINE_IID(kInetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kInetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kInetLibURLIID, NS_INETLIBURL_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);

static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);

static NS_DEFINE_IID(kISilentDownloadTaskIID,           NS_IDOMSILENTDOWNLOADTASK_IID);
static NS_DEFINE_IID(kISilentDownloadIID,               NS_IDOMSILENTDOWNLOAD_IID); 
static NS_DEFINE_IID(kSilentDownloadCID,                NS_SilentDownload_CID);
static NS_DEFINE_IID(kSilentDownloadTaskCID,            NS_SilentDownloadTask_CID);
static NS_DEFINE_IID(kSilentDownloadFactoryCID,         NS_SilentDownloadFactory_CID);
static NS_DEFINE_IID(kSilentDownloadTaskFactoryCID,     NS_SilentDownloadTaskFactory_CID);




static PRInt32 gLockCnt = 0;
static PRInt32 gInstanceCnt = 0;

static nsITimer         *gTaskTimer     = nsnull;
static nsINetService    *gInetService   = nsnull;
static SDL_TaskList     *gTasks         = nsnull;
static SDL_TaskList     *gNextReadyTask = nsnull;

static PRInt32          gByteRange = 0;
static PRInt32          gInterval  = 0xFFFF;

/////////////////////////////////////////////////////////////////////////
// static helper meathods
/////////////////////////////////////////////////////////////////////////


static int32
GetCurrentSize(char *file)
{
  struct stat in_stat;
  int stat_result = -1;
  stat_result = stat(file, &in_stat);
  if (stat_result == 0) {
    return in_stat.st_size;
  }
  return 0;
}

static char*
AssureDir(char* path)
{
  char *autoupdt_dir = PR_smprintf("%sSilentDL", path);
  if (PR_SUCCESS != PR_Access(autoupdt_dir, PR_ACCESS_WRITE_OK)) {
    if ((PR_MkDir(autoupdt_dir, 0777)) < 0) {
      /* Creation of directory failed. Don't do SilentDownload. */
      return NULL;
    }
  }
  return autoupdt_dir;
}

static void
GetSilentDownloadDirectory(char* directory)
{
    if (PREF_OK != PREF_CopyCharPref("SilentDownload.directory", (char**)directory)) 
    {
        directory = NULL;
    } 
    else 
    {
        if ((directory) && (XP_STRCMP(directory, "") == 0)) 
        {
            directory = NULL;
        }
    }
}

static void 
GetSilentDownloadDefaults(PRBool* enabled, PRInt32 *bytes_range, PRInt32 *interval)
{
    PREF_GetBoolPref( "SilentDownload.enabled", (XP_Bool*)enabled);
    if (!enabled)
        return;

   
    if (PREF_OK != PREF_GetIntPref("SilentDownload.range", (int32*)*bytes_range)) 
    {
        *bytes_range = 3000;
    }

    if (PREF_OK != PREF_GetIntPref("SilentDownload.interval", (int32*)*interval)) 
    {
        *interval = 10000;
    }
}



static char* 
CreateOutFileLocation(char* url, char* directory)
{
    /*  FIX: The outfile that this functions creates must be compatible with
        PR_Open!
    */
    char* outfile = NULL;

    char* autoupdt_dir;
    char* slash;
    char* filename;
    char  Path[1024 + 1];

    slash = PL_strrchr(url, '/');
    if (slash != NULL) 
    {
        filename = ++slash;
    } 
    else 
    {
        filename = "prg";
    }


#ifdef XP_UNIX
        
    if (directory) 
    {
        /* what if the directory already has a / appended to it? */
        PR_snprintf( Path, MAXPATHLEN, "%s/", directory);
        PR_FREEIF(directory);
    } 
    else 
    {
        directory = getenv("MOZILLA_FIVE_HOME");
        if (directory) 
        {
           PR_snprintf( Path, MAXPATHLEN, "%s/", directory);
        } 
        else 
        {
            extern void fe_GetProgramDirectory( char *, int );
            //fe_GetProgramDirectory( Path, MAXPATHLEN-1 );
        }
    }

    autoupdt_dir = AssureDir(Path);

    if (autoupdt_dir != NULL) 
    {
        outfile = PR_smprintf("%s/%s", autoupdt_dir, filename);
        PR_FREEIF(autoupdt_dir);
    }
   
#elif defined(WIN32)

    if (directory) 
    {
        PR_snprintf( Path, MAXPATHLEN, "%s\\", directory);
        PR_FREEIF(directory);
    } 
    else 
    {
        PR_snprintf( Path, MAXPATHLEN, "C:\\Temp\\");
        extern char * FE_GetProgramDirectory(char *buffer, int length);
        //FE_GetProgramDirectory( Path, _MAX_PATH );
    }
  
    autoupdt_dir = AssureDir(Path);
    if (autoupdt_dir != NULL) 
    {
        outfile = PR_smprintf("%s\\%s", autoupdt_dir, filename);
        PR_FREEIF(autoupdt_dir);
    } 

#elif defined(MAC)
  
  /* XXX: Fix it for Mac with the correct folder */
  
    if (directory) 
    {
        PR_snprintf( Path, MAXPATHLEN, "%s:", directory);
        PR_FREEIF(directory);
    } 
    else 
    {
        directory = XP_TempDirName();
        PR_snprintf( Path, MAXPATHLEN, "%s:", directory);
    }
  
    outfile = PR_smprintf("%s:%s", Path, filename);

#else
    outfile = NULL;
#endif

    return outfile;
}




/////////////////////////////////////////////////////////////////////////
// nsSilentDownloadManager
/////////////////////////////////////////////////////////////////////////

nsSilentDownloadManager::nsSilentDownloadManager()
{
    mScriptObject   = nsnull;
    
    PR_AtomicIncrement(&gInstanceCnt);
    NS_INIT_REFCNT();
}


nsSilentDownloadManager::~nsSilentDownloadManager()
{
    PR_AtomicDecrement(&gInstanceCnt);  
}

nsresult
nsSilentDownloadManager::LoadAllTasks(void)
{
    gNextReadyTask = gTasks;
    return NS_OK;
}

nsresult
nsSilentDownloadManager::StoreAllTasks(void)
{
    return NS_OK;
}


NS_IMPL_ADDREF(nsSilentDownloadManager)
NS_IMPL_RELEASE(nsSilentDownloadManager)



NS_IMETHODIMP 
nsSilentDownloadManager::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    
    if ( aIID.Equals(kISilentDownloadIID) )
    {
        nsIDOMSilentDownload* tmp = this;
        *aInstancePtr = (void*)tmp;
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kIScriptObjectOwnerIID))
    {   
        nsIScriptObjectOwner* tmp = this;
        *aInstancePtr = (void*)tmp;
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kISupportsIID) )
    {
         
        nsIDOMSilentDownload* tmp1 = this;
        nsISupports* tmp2 = tmp1;
        
        *aInstancePtr = (void*)tmp2;
        AddRef();
        return NS_OK;
    }

     return NS_NOINTERFACE;
}


NS_IMETHODIMP 
nsSilentDownloadManager::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    nsresult res = NS_OK;
    
    if (nsnull == mScriptObject) 
    {
        nsIScriptGlobalObject *global = aContext->GetGlobalObject();

        res = NS_NewScriptSilentDownload(aContext, (nsISupports *)(nsIDOMSilentDownload*)this, global, (void**)&mScriptObject);
        
        NS_IF_RELEASE(global);
    }
  

    *aScriptObject = mScriptObject;
    return res;
}

NS_IMETHODIMP 
nsSilentDownloadManager::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

void nsSilentDownloadManager::Notify(nsITimer * aTimer)
{
    /* Fix:  maybe some additional check on gNextReadyTask */

    if (gTasks != NULL && gNextReadyTask != NULL)
    {
        if (! gInetService->AreThereActiveConnections() )
        {
            gNextReadyTask->task->DownloadSelf(gByteRange);

            if (gNextReadyTask->next == NULL)
            {
                gNextReadyTask = gTasks;
            }
            else
            {
                gNextReadyTask = gNextReadyTask->next;    
            }
        }
    }

    NS_RELEASE(gTaskTimer);

    if (NS_OK == NS_NewTimer(&gTaskTimer))
        gTaskTimer->Init(this, gInterval);
}

NS_IMETHODIMP
nsSilentDownloadManager::Startup()
{
    PRBool enabled = PR_FALSE;

    GetSilentDownloadDefaults(&enabled, &gByteRange, &gInterval);

    if (enabled == PR_FALSE)
        return -1;

    
    /***************************************/
    /* Add us to the Javascript Name Space */
    /***************************************/

    nsIScriptNameSetRegistry *registry;
    nsresult result = nsServiceManager::GetService(kCScriptNameSetRegistryCID,
                                                   kIScriptNameSetRegistryIID,
                                                  (nsISupports **)&registry);
    if (NS_OK == result) 
    {
        nsSilentDownloadNameSet* nameSet = new nsSilentDownloadNameSet();
        registry->AddExternalNameSet(nameSet);
        /* FIX - do we need to release this service?  When we do, it get deleted,and our name is lost. */
    }

    /************************************************************************/
    /* Create an instance of INet so that we can see if the network is busy */
    /************************************************************************/

    result = nsComponentManager::CreateInstance(kInetServiceCID,   
                                                nsnull,
                                                kInetServiceIID,
                                                (void**)&gInetService);

    if (result != NS_OK)
        return result;

    /************************/
    /* Load all saved tasks */
    /************************/
    result = LoadAllTasks();

    if (result != NS_OK)
        return result;
    
    /************************/
    /* Start Timer          */
    /************************/

    if (NS_OK == NS_NewTimer(&gTaskTimer))
        gTaskTimer->Init(this, gInterval);

    return result;
}

NS_IMETHODIMP
nsSilentDownloadManager::Shutdown()
{
    StoreAllTasks();
    return NS_OK;
}

NS_IMETHODIMP    
nsSilentDownloadManager::Add(nsIDOMSilentDownloadTask* aTask)
{
   
    if (aTask == NULL)
        return NS_ERROR_FAILURE;

    /* Check to see if we already have this task in our list */
    SDL_TaskList *node = gTasks;
    nsString      nodeIDString;
    nsString      addIDString;

    aTask->GetId(addIDString);

    while (node != NULL)
    {
        node->task->GetId(nodeIDString);
         
        if (nodeIDString == addIDString)
        {
            /*we already have this ID in our list, ignore */
            return NS_OK;    
        }

        node = node->next;
    }

    /* add the task to our list */
    SDL_TaskList* taskNode = (SDL_TaskList*)PR_MALLOC(sizeof(SDL_TaskList));
    
    aTask->AddRef();

    taskNode->next = gTasks;
    taskNode->task = aTask;
    gTasks = taskNode;

    /* Lets set the next task to run to this one */
    gNextReadyTask = taskNode;
    
    aTask->SetState(nsIDOMSilentDownloadTask::SDL_STARTED);

    return NS_OK;
}
NS_IMETHODIMP    
nsSilentDownloadManager::Remove(nsIDOMSilentDownloadTask* aTask)
{
    if (aTask == NULL)
        return NS_ERROR_FAILURE;

    /* Remove from our list */
    
    SDL_TaskList *node = gTasks;
    SDL_TaskList *lastnode = gTasks;
    nsString      nodeIDString;
    nsString      doomedIDString;

    aTask->GetId(doomedIDString);

    while (node != NULL)
    {
        node->task->GetId(nodeIDString);
        
        if (nodeIDString == doomedIDString)
        {
            /* we want to delete this node */
            
            if (node == gTasks)
            {
                gTasks = node->next;
            }
            else
            {
                lastnode->next = node->next;
            }

            node->task->Release();
            PR_DELETE(node);
            break;
        }

        lastnode = node;
        node = node->next;
    }
    return NS_OK;
}



NS_IMETHODIMP    
nsSilentDownloadManager::Find(const nsString& aId, nsIDOMSilentDownloadTask** aReturn)
{
    *aReturn=nsnull;

    SDL_TaskList *node = gTasks;
    nsString      nodeIDString;

    while (node != NULL)
    {
        node->task->GetId(nodeIDString);
        
        if (nodeIDString == aId)
        {
            *aReturn = node->task;
            node->task->AddRef();
            break;
        }
        node = node->next;
    }

    return NS_OK;
}


NS_IMETHODIMP    
nsSilentDownloadManager::GetByteRange(PRInt32* aByterange)
{
    *aByterange = gByteRange;
    return NS_OK;
}

NS_IMETHODIMP    
nsSilentDownloadManager::SetByteRange(PRInt32 aByterange)
{
    gByteRange = aByterange;
    return NS_OK;
}

NS_IMETHODIMP
nsSilentDownloadManager::GetInterval(PRInt32* aInterval)
{
    *aInterval = gInterval;
    return NS_OK;
}

NS_IMETHODIMP    
nsSilentDownloadManager::SetInterval(PRInt32 aInterval)
{
    gInterval = aInterval;
    return NS_OK;
}   



/////////////////////////////////////////////////////////////////////////
// nsSilentDownloadTask
/////////////////////////////////////////////////////////////////////////

nsSilentDownloadTask::nsSilentDownloadTask()
{
    mScriptObject   = nsnull;
    mListener       = nsnull;
    mWindow         = nsnull;
    mWebShell       = nsnull;
    mState          = nsIDOMSilentDownloadTask::SDL_NOT_INITED;
    mNextByte       = 0;

    PR_AtomicIncrement(&gInstanceCnt);
    NS_INIT_REFCNT();
}

nsSilentDownloadTask::~nsSilentDownloadTask()
{
    mListener->Release();
    PR_AtomicDecrement(&gInstanceCnt);  
}


void 
nsSilentDownloadTask::LoadScript(void)
{
    if (mState == nsIDOMSilentDownloadTask::SDL_NOT_ADDED ||
        mState == nsIDOMSilentDownloadTask::SDL_NOT_INITED )
        return;

    if (mWindow == nsnull)
    {
        nsresult rv = nsComponentManager::CreateInstance(kBrowserWindowCID, nsnull,
                                                         kIBrowserWindowIID,
                                                         (void**) &mWindow);
        if (rv == NS_OK) 
        {
            nsRect rect(0, 0, 275, 300);

            rv = mWindow->Init(nsnull, nsnull, rect, PRUint32(0), PR_FALSE);

            if (rv == NS_OK)
            {
                rv = mWindow->GetWebShell(mWebShell);
                mWebShell->LoadURL(mScript);
            }
            else
            {
                mWindow->Release();
                mWindow=nsnull;

                mState = nsIDOMSilentDownloadTask::SDL_ERROR;
                SetErrorMsg("Couldn't Open Window");
            }
        }
    }
    else
    {
        if (mWebShell)
        {
            mWebShell->LoadURL(mScript);
        }
        else
        {
            mState = nsIDOMSilentDownloadTask::SDL_ERROR;
            SetErrorMsg("Couldn't Open Window");
        }
    }   
}


NS_IMPL_ADDREF(nsSilentDownloadTask)
NS_IMPL_RELEASE(nsSilentDownloadTask)


NS_IMETHODIMP 
nsSilentDownloadTask::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kIScriptObjectOwnerIID))
    {
        *aInstancePtr = (void*) ((nsIScriptObjectOwner*)this);
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kISilentDownloadTaskIID) )
    {
        *aInstancePtr = (void*) ((nsIDOMSilentDownloadTask*)this);
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
        AddRef();
        return NS_OK;
    }

     return NS_NOINTERFACE;
}


NS_IMETHODIMP 
nsSilentDownloadTask::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;
    if (nsnull == mScriptObject) 
    {
        res = NS_NewScriptSilentDownloadTask(   aContext, 
                                                (nsISupports *)(nsIDOMSilentDownloadTask*)this, 
                                                nsnull, 
                                                &mScriptObject);
    }
  
    *aScriptObject = mScriptObject;
    return res;
}

NS_IMETHODIMP 
nsSilentDownloadTask::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}


NS_IMETHODIMP    
nsSilentDownloadTask::Init(const nsString& aId, const nsString& aUrl, const nsString& aScript)
{
    char *defaultDirectoryCString  = NULL;
    char *urlCString               = NULL;
    char *outFileCString           = NULL;
    
    GetSilentDownloadDirectory(defaultDirectoryCString);

    mId           = aId;
    mUrl          = aUrl;
    mScript       = aScript;
    
    urlCString       = aUrl.ToNewCString();
    outFileCString   = CreateOutFileLocation(urlCString, defaultDirectoryCString);
    delete[] urlCString;

    if (outFileCString == NULL) 
    {
        SetState(nsIDOMSilentDownloadTask::SDL_ERROR);
        SetErrorMsg("Couldn't access destination directory to save file");
        return -1;
    }
    else
    {
        mOutFile = outFileCString;
    }
    

    mNextByte = GetCurrentSize(outFileCString);
    /* FIX delete outFileCString and defaultDirectoryCString */

    
    if (mNextByte < 0) 
    {
        SetState(nsIDOMSilentDownloadTask::SDL_ERROR);
        SetErrorMsg("Negative Byte!");
        return NS_OK;

    } 
    
    /* Everything looks good.  Mark as SDL_NOT_ADDED. */
    mState = nsIDOMSilentDownloadTask::SDL_NOT_ADDED;

    mListener = new nsSilentDownloadListener();
    mListener->AddRef();
    mListener->SetSilentDownloadInfo(this);

    nsSilentDownloadManager* sdm = new nsSilentDownloadManager();
    sdm->Add(this);
    delete sdm;

    return NS_OK;
}


NS_IMETHODIMP    
nsSilentDownloadTask::GetId(nsString& aId)
{
    aId = mId;
    return NS_OK;
}


NS_IMETHODIMP    
nsSilentDownloadTask::GetUrl(nsString& aUrl)
{
    aUrl = mUrl;
    return NS_OK;
}

NS_IMETHODIMP    
nsSilentDownloadTask::GetScript(nsString& aScript)
{
    aScript = mScript;
    return NS_OK;
}



NS_IMETHODIMP    
nsSilentDownloadTask::GetState(PRInt32* aState)
{
    *aState = mState;
    return NS_OK;
}


NS_IMETHODIMP    
nsSilentDownloadTask::SetState(PRInt32 aState)
{
/* This function is so that our listener can change the state.  
I would rather not expose this to Javascript if possible*/
    
    mState = aState;
    LoadScript();
    
    return NS_OK;
}

NS_IMETHODIMP    
nsSilentDownloadTask::GetErrorMsg(nsString& aErrorMsg)
{
/* This function is so that our listener can change the state.  
I would rather not expose this to Javascript if possible*/
    aErrorMsg = mErrorMsg;
    return NS_OK;
}

NS_IMETHODIMP    
nsSilentDownloadTask::SetErrorMsg(const nsString& aErrorMsg)
{
    mErrorMsg = aErrorMsg;
    return NS_OK;
}


NS_IMETHODIMP    
nsSilentDownloadTask::GetNextByte(PRInt32* aNext_byte)
{
    *aNext_byte = mNextByte;
    return NS_OK;
}

NS_IMETHODIMP    
nsSilentDownloadTask::SetNextByte(PRInt32 aNextByte)
{
    mNextByte = aNextByte;
    return NS_OK;
}

NS_IMETHODIMP    
nsSilentDownloadTask::GetOutFile(nsString& aOutFile)
{
    aOutFile = mOutFile;
    return NS_OK;
}

NS_IMETHODIMP    
nsSilentDownloadTask::Remove()
{
    mState = nsIDOMSilentDownloadTask::SDL_NOT_ADDED;
    
    mWebShell->Release();
    
    mWindow->Close();
    mWindow->Release();
    mWindow = nsnull;

    nsSilentDownloadManager* sdm = new nsSilentDownloadManager();
    sdm->Remove(this);
    delete sdm;

    return NS_OK;
}


NS_IMETHODIMP    
nsSilentDownloadTask::Suspend()
{
    SetState(nsIDOMSilentDownloadTask::SDL_SUSPENDED);
    return NS_OK;
}

NS_IMETHODIMP    
nsSilentDownloadTask::Resume()
{
    SetState(nsIDOMSilentDownloadTask::SDL_STARTED);
    return NS_OK;
}


NS_IMETHODIMP    
nsSilentDownloadTask::DownloadNow()
{
    SetState(nsIDOMSilentDownloadTask::SDL_DOWNLOADING_NOW);
    DownloadSelf(0);
    return NS_OK;
}


NS_IMETHODIMP 
nsSilentDownloadTask::DownloadSelf(PRInt32 range)
{
    long result=0;
    char*                byteRangeString=NULL;
    nsIURL               *pURL;
    nsILoadAttribs       *loadAttr;
    
    if (mState != nsIDOMSilentDownloadTask::SDL_STARTED &&
        mState != nsIDOMSilentDownloadTask::SDL_DOWNLOADING_NOW) 
    {
        /* We do not have to do a download here. */
        return 0;
    }
   

    // Create the URL object...
    pURL = NULL;

    result = NS_NewURL(&pURL, mUrl);

    if (result != NS_OK) 
    {
        SetState(nsIDOMSilentDownloadTask::SDL_ERROR);
        SetErrorMsg("Couldn't set up download. Out of memory");
        return -1;
    }

    pURL->GetLoadAttribs(&loadAttr);
    loadAttr->SetLoadType(nsURLLoadBackground);
    

    byteRangeString =   PR_sprintf_append(  byteRangeString, 
                                            "bytes=%ld-%ld",
                                            mNextByte,
                                            mNextByte+range);

    loadAttr->SetByteRangeHeader(byteRangeString);
    
    PR_FREEIF(byteRangeString);

    if (mState != nsIDOMSilentDownloadTask::SDL_DOWNLOADING_NOW)
    {
        /* Do Byte Range Stuff */
    }

    result = NS_OpenURL(pURL, mListener);

    /* If the open failed... */
    if (NS_OK != result) 
    {
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////
// nsSilentDownloadImplFactory
/////////////////////////////////////////////////////////////////////////

nsSilentDownloadManagerFactory::nsSilentDownloadManagerFactory(void)
{
    mRefCnt=0;
    PR_AtomicIncrement(&gInstanceCnt);
}

nsSilentDownloadManagerFactory::~nsSilentDownloadManagerFactory(void)
{
    PR_AtomicDecrement(&gInstanceCnt);
}



NS_IMETHODIMP 
nsSilentDownloadManagerFactory::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*) this;
    }
    else if ( aIID.Equals(kIFactoryIID) )
    {
        *aInstancePtr = (void*) this;
    }

    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NO_INTERFACE;
    }

    AddRef();
    return NS_OK;
}



NS_IMETHODIMP
nsSilentDownloadManagerFactory::AddRef(void)
{
    return ++mRefCnt;
}


NS_IMETHODIMP
nsSilentDownloadManagerFactory::Release(void)
{
    if (--mRefCnt ==0)
    {
        delete this;
        return 0; // Don't access mRefCnt after deleting!
    }

    return mRefCnt;
}

NS_IMETHODIMP
nsSilentDownloadManagerFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aResult == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aResult = NULL;

    /* do I have to use iSupports? */
    nsSilentDownloadManager *inst = new nsSilentDownloadManager();

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult result =  inst->QueryInterface(aIID, aResult);

    if (result != NS_OK)
        delete inst;

    return result;

}

NS_IMETHODIMP
nsSilentDownloadManagerFactory::LockFactory(PRBool aLock)
{
    if (aLock)
        PR_AtomicIncrement(&gLockCnt);
    else
        PR_AtomicDecrement(&gLockCnt);

    return NS_OK;
}
/////////////////////////////////////////////////////////////////////////
// nsSilentDownloadTaskFactory
/////////////////////////////////////////////////////////////////////////

nsSilentDownloadTaskFactory::nsSilentDownloadTaskFactory(void)
{
    mRefCnt=0;
    PR_AtomicIncrement(&gInstanceCnt);
}

nsSilentDownloadTaskFactory::~nsSilentDownloadTaskFactory(void)
{
    PR_AtomicDecrement(&gInstanceCnt);
}



NS_IMETHODIMP 
nsSilentDownloadTaskFactory::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*) this;
    }
    else if ( aIID.Equals(kIFactoryIID) )
    {
        *aInstancePtr = (void*) this;
    }

    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NO_INTERFACE;
    }

    AddRef();
    return NS_OK;
}



NS_IMETHODIMP
nsSilentDownloadTaskFactory::AddRef(void)
{
    return ++mRefCnt;
}


NS_IMETHODIMP
nsSilentDownloadTaskFactory::Release(void)
{
    if (--mRefCnt ==0)
    {
        delete this;
        return 0; // Don't access mRefCnt after deleting!
    }

    return mRefCnt;
}

NS_IMETHODIMP
nsSilentDownloadTaskFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aResult == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aResult = NULL;

    /* do I have to use iSupports? */
    nsSilentDownloadTask *inst = new nsSilentDownloadTask();

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult result =  inst->QueryInterface(aIID, aResult);

    if (result != NS_OK)
        delete inst;

    return result;

}

NS_IMETHODIMP
nsSilentDownloadTaskFactory::LockFactory(PRBool aLock)
{
    if (aLock)
        PR_AtomicIncrement(&gLockCnt);
    else
        PR_AtomicDecrement(&gLockCnt);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsSilentDownloadNameSet
////////////////////////////////////////////////////////////////////////////////

nsSilentDownloadNameSet::nsSilentDownloadNameSet()
{
  NS_INIT_REFCNT();
}

nsSilentDownloadNameSet::~nsSilentDownloadNameSet()
{
}

NS_IMPL_ISUPPORTS(nsSilentDownloadNameSet, kIScriptExternalNameSetIID);




NS_IMETHODIMP
nsSilentDownloadNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
    nsresult result = NS_OK;

    result = NS_InitSilentDownloadClass(aScriptContext, nsnull);
    if (NS_OK != result) return result;

    result = NS_InitSilentDownloadTaskClass(aScriptContext, nsnull);

    return result;
}




NS_IMETHODIMP
nsSilentDownloadNameSet::AddNameSet(nsIScriptContext* aScriptContext)
{
    nsresult result = NS_OK;
    nsIScriptNameSpaceManager* manager;

    result = aScriptContext->GetNameSpaceManager(&manager);
    if (NS_OK == result) 
    {
        result = manager->RegisterGlobalName("SilentDownloadTask", 
                                             kSilentDownloadTaskCID, 
                                             PR_TRUE);

        if (NS_OK != result) return result;

        result = manager->RegisterGlobalName("SilentDownloadManager", 
                                             kSilentDownloadCID, 
                                             PR_FALSE);
        

        NS_RELEASE(manager);
    }
    return result;
}



////////////////////////////////////////////////////////////////////////////////
// nsSilentDownloadListener
////////////////////////////////////////////////////////////////////////////////
nsSilentDownloadListener::nsSilentDownloadListener()
{
    NS_INIT_REFCNT();
}

nsSilentDownloadListener::~nsSilentDownloadListener()
{
    mSilentDownloadTask->Release();
    
    if (mOutFileDesc != NULL)
        PR_Close(mOutFileDesc);
}


NS_IMPL_ISUPPORTS( nsSilentDownloadListener, kIStreamListenerIID )

NS_IMETHODIMP
nsSilentDownloadListener::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* info)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSilentDownloadListener::OnProgress( nsIURL* aURL,
			              PRUint32 Progress,
			              PRUint32 ProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSilentDownloadListener::OnStatus(nsIURL* aURL, 
			           const PRUnichar* aMsg)
{ 
  return NS_OK;
}

NS_IMETHODIMP
nsSilentDownloadListener::OnStartBinding(nsIURL* aURL, 
				             const char *aContentType)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSilentDownloadListener::OnStopBinding(nsIURL* aURL,
				            nsresult status,
				            const PRUnichar* aMsg)
{
    nsresult        result = NS_OK;
    PRInt32         content_length = 0;
    PRInt32         server_status  = 0;
    PRInt32         nextByte;

    switch( status ) 
    {

        case NS_BINDING_SUCCEEDED:
            /*
                What we are going to is check to see if our nextByte that the task will
                get is passed the content_length of the URL.  

                What happens if there is no content_length?
            */
            
            aURL->GetServerStatus(&server_status);
            
            if (server_status == 404)
            {
                mSilentDownloadTask->SetState(nsIDOMSilentDownloadTask::SDL_ERROR);
                mSilentDownloadTask->SetErrorMsg("Server Error 400.");
                return NS_OK;
            }
            else if (server_status == 500)
            {
                mSilentDownloadTask->SetState(nsIDOMSilentDownloadTask::SDL_ERROR);
                mSilentDownloadTask->SetErrorMsg("Server Error 500.");
                return NS_OK;
            }
            
            aURL->GetContentLength(&content_length);

            mSilentDownloadTask->GetNextByte(&nextByte);

            if (content_length <= nextByte || content_length == 0)
            {
                mSilentDownloadTask->SetState(nsIDOMSilentDownloadTask::SDL_COMPLETED);
                PR_Close(mOutFileDesc);
                mOutFileDesc=NULL;
            }
            break;

        case NS_BINDING_FAILED:
        case NS_BINDING_ABORTED:
                mSilentDownloadTask->SetState(nsIDOMSilentDownloadTask::SDL_ERROR);
                mSilentDownloadTask->SetErrorMsg("Could Not Download.");
            break;

        default:
            result = NS_ERROR_ILLEGAL_VALUE;
    }

    return result;
}


NS_IMETHODIMP
nsSilentDownloadListener::OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRUint32 length)
{
    PRUint32 len;
    PRInt32 nextByte;
    nsresult err;
    char buffer[80];
    
    do 
    {
        err = pIStream->Read(buffer, 80, &len);
        if (err == NS_OK) 
        {
            mSilentDownloadTask->GetNextByte(&nextByte);
            if ( PR_Seek(mOutFileDesc, nextByte, PR_SEEK_SET) == -1 )
            {
                /* Error */ 
                mSilentDownloadTask->SetState(nsIDOMSilentDownloadTask::SDL_ERROR);
                mSilentDownloadTask->SetErrorMsg("File Seek Error.");
                return -1;
            }

            if( PR_Write(mOutFileDesc, buffer, len) == -1 )
            {
                /* Error */ 
                mSilentDownloadTask->SetState(nsIDOMSilentDownloadTask::SDL_ERROR);
                mSilentDownloadTask->SetErrorMsg("File Write Error.");
                return -1;
            }

            mSilentDownloadTask->SetNextByte(nextByte+len);
        }
    } while (len > 0);

    return 0;
}

NS_IMETHODIMP
nsSilentDownloadListener::SetSilentDownloadInfo(nsIDOMSilentDownloadTask* con)
{
    nsString aString;
    char *fileName;

    con->AddRef();

    mSilentDownloadTask = con;
    mSilentDownloadTask->GetOutFile(aString);
    fileName = aString.ToNewCString();
    
    mOutFileDesc = PR_Open(fileName,  PR_CREATE_FILE | PR_RDWR, 0644);
    
    delete[] fileName;

    if(mOutFileDesc == NULL)
    {
        mSilentDownloadTask->SetState(nsIDOMSilentDownloadTask::SDL_ERROR);
        mSilentDownloadTask->SetErrorMsg("Could not create OUT file.");
        return NS_OK;
    };


    return NS_OK;
}





////////////////////////////////////////////////////////////////////////////////
// DLL Entry Points:
////////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* aServMgr)
{
    return PRBool (gInstanceCnt == 0 && gLockCnt == 0);
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

#ifdef NS_DEBUG
    printf("*** SilentDownload is being registered\n");
#endif

    rv = compMgr->RegisterComponent(kSilentDownloadCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kSilentDownloadTaskCID, NULL, NULL, path, PR_TRUE, PR_TRUE);

  done:
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

#ifdef NS_DEBUG
    printf("*** SilentDownload is being unregistered\n");
#endif
    
    rv = compMgr->UnregisterFactory(kSilentDownloadCID, path);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterFactory(kSilentDownloadTaskCID, path);

  done:
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}



extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{

    if (aFactory == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aFactory = NULL;
    nsISupports *inst;


    if ( aClass.Equals(kSilentDownloadCID) )
    {
        inst = new nsSilentDownloadManagerFactory();        
    }
    else if ( aClass.Equals(kSilentDownloadTaskCID) )
    {
        inst = new nsSilentDownloadTaskFactory();      
    }
    else
    {
        return NS_ERROR_ILLEGAL_VALUE;
    }


    if (inst == NULL)
    {   
        return NS_ERROR_OUT_OF_MEMORY;
    }


    nsresult res = inst->QueryInterface(kIFactoryIID, (void**) aFactory);

    if (res != NS_OK)
    {   
        delete inst;
    }

    return res;

}


