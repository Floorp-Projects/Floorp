/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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

#ifndef nsSilentDownloadPrivate_h___
#define nsSilentDownloadPrivate_h___

#include "nsSilentDownload.h"

#include "nscore.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptObjectOwner.h"

#include "nsINetService.h"
#include "nsIScriptExternalNameSet.h"
#include "nsIStreamListener.h"

#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"

#include "nsIDOMSilentDownload.h"
#include "nsIDOMSilentDownloadTask.h"

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include "prio.h"

////////////////////////////////////////////////////////////////////////////////
// nsSilentDownloadListener:
////////////////////////////////////////////////////////////////////////////////

class nsSilentDownloadListener : public nsIStreamListener
{

    public:
        NS_DECL_ISUPPORTS
    
        nsSilentDownloadListener();

        NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* info);
        NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 Progress, PRUint32 ProgressMax);
        NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
        NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
        NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRUint32 length);
        NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult status, const PRUnichar* aMsg);
        
        NS_METHOD SetSilentDownloadInfo(nsIDOMSilentDownloadTask* con);

    protected:
        virtual ~nsSilentDownloadListener();

    private:
         //
          nsIDOMSilentDownloadTask* mSilentDownloadTask;
          PRFileDesc *mOutFileDesc;
};



////////////////////////////////////////////////////////////////////////////////
// nsSilentDownloadTask:
////////////////////////////////////////////////////////////////////////////////

class nsSilentDownloadTask : public nsIScriptObjectOwner, public nsIDOMSilentDownloadTask
{
    public:
    
           nsSilentDownloadTask();
           virtual ~nsSilentDownloadTask();
                         

        NS_DECL_ISUPPORTS
            NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
            NS_IMETHOD    SetScriptObject(void* aScriptObject);
            
            NS_IMETHOD    Init(const nsString& aId, const nsString& aUrl, const nsString& aScript);


            NS_IMETHOD    GetId(nsString& aId);
            NS_IMETHOD    GetUrl(nsString& aUrl);
            NS_IMETHOD    GetScript(nsString& aScript);
            NS_IMETHOD    GetOutFile(nsString& aOutFile);

            NS_IMETHOD    GetState(PRInt32* aState);
            NS_IMETHOD    SetState(PRInt32  aState);
            
            NS_IMETHOD    GetErrorMsg(nsString& aErrorMsg);
            NS_IMETHOD    SetErrorMsg(const nsString& aErrorMsg);

            NS_IMETHOD    GetNextByte(PRInt32* aStart_byte);
            NS_IMETHOD    SetNextByte(PRInt32 aStart_byte);
            
            NS_IMETHOD    Remove();
            NS_IMETHOD    Suspend();
            NS_IMETHOD    Resume();
            NS_IMETHOD    DownloadNow();

            NS_IMETHOD    DownloadSelf(PRInt32 aRange);

    private:
        
        void LoadScript(void);
                
        nsString  mId;         /* User ID                                                   */
        nsString  mUrl;        /* What to Download                                          */
        nsString  mScript;     /* What controls the download                                */
    
        PRInt32   mFile_size;  /* How big is the download                                   */
        
        nsString  mErrorMsg;   /* Human readable error message                              */
        nsString  mOutFile;    /* Where on the users system file is stored                  */

        PRInt32   mState;      /* State of task                                             */
        
        PRInt32   mNextByte;   /* The start byte in the file where will will start downloading */
        

        nsSilentDownloadListener *mListener;
        nsIBrowserWindow         *mWindow;
        nsIWebShell              *mWebShell;
        void                     *mScriptObject;
};


////////////////////////////////////////////////////////////////////////////////
// nsSilentDownloadManager:
////////////////////////////////////////////////////////////////////////////////
typedef struct _SDL_TaskList SDL_TaskList;

typedef struct _SDL_TaskList
{
    nsIDOMSilentDownloadTask  *task;
    SDL_TaskList              *next;

} SDL_TaskList;

class nsSilentDownloadManager : public nsIScriptObjectOwner, public nsIDOMSilentDownload, public nsITimerCallback
{
    public:
    
        nsSilentDownloadManager();
        virtual ~nsSilentDownloadManager();
               
        NS_DECL_ISUPPORTS

              NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
              NS_IMETHOD    SetScriptObject(void* aScriptObject);
               
              NS_IMETHOD    GetByteRange(PRInt32* aByterange);
              NS_IMETHOD    SetByteRange(PRInt32 aByterange);
              NS_IMETHOD    GetInterval(PRInt32* aInterval);
              NS_IMETHOD    SetInterval(PRInt32 aInterval);
        
              NS_IMETHOD    Remove(nsIDOMSilentDownloadTask* aTask);
              NS_IMETHOD    Add(nsIDOMSilentDownloadTask* aTask);
              NS_IMETHOD    Find(const nsString& aId, nsIDOMSilentDownloadTask** aReturn); 
              
              NS_IMETHOD    Startup();
              NS_IMETHOD    Shutdown();

    protected:         
              // nsITimerCallback Interface
              virtual void Notify(nsITimer *timer);
  
    private:
        
        
        nsresult StoreAllTasks(void);
        nsresult LoadAllTasks(void);

        void  *mScriptObject;
        
};




////////////////////////////////////////////////////////////////////////////////
// nsSilentDownloadManagerFactory:
////////////////////////////////////////////////////////////////////////////////

class nsSilentDownloadManagerFactory : public nsIFactory 
{
    public:
        
        nsSilentDownloadManagerFactory();
        virtual ~nsSilentDownloadManagerFactory();
        
        PRBool CanUnload(void);

        NS_DECL_ISUPPORTS

            NS_IMETHOD CreateInstance(nsISupports *aOuter,
                                      REFNSIID aIID,
                                      void **aResult);

            NS_IMETHOD LockFactory(PRBool aLock);

};


////////////////////////////////////////////////////////////////////////////////
// nsSilentDownloadTaskFactory:
////////////////////////////////////////////////////////////////////////////////

class nsSilentDownloadTaskFactory : public nsIFactory 
{
    public:
        
        nsSilentDownloadTaskFactory();
        virtual ~nsSilentDownloadTaskFactory();
        
        PRBool CanUnload(void);

        NS_DECL_ISUPPORTS

            NS_IMETHOD CreateInstance(nsISupports *aOuter,
                                      REFNSIID aIID,
                                      void **aResult);

            NS_IMETHOD LockFactory(PRBool aLock);

};

////////////////////////////////////////////////////////////////////////////////
// nsSilentDownloadNameSet:
////////////////////////////////////////////////////////////////////////////////

class nsSilentDownloadNameSet : public nsIScriptExternalNameSet 
{
    public:
        nsSilentDownloadNameSet();
        virtual ~nsSilentDownloadNameSet();

        NS_DECL_ISUPPORTS
            NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext);
            NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext);
};

#endif 
