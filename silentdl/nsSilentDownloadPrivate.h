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
// DLL Entry Points:
////////////////////////////////////////////////////////////////////////////////
extern "C" NS_EXPORT nsresult
NSGetFactory(const nsCID &aClass, nsISupports* serviceMgr, nsIFactory **aFactory);

extern "C" NS_EXPORT PRBool
NSCanUnload(void);

extern "C" NS_EXPORT nsresult
NSRegisterSelf(const char *path);

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(const char *path);

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
        ~nsSilentDownloadListener();

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
           ~nsSilentDownloadTask();
                         

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
        ~nsSilentDownloadManager();
               
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
        ~nsSilentDownloadManagerFactory();
        
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
        ~nsSilentDownloadTaskFactory();
        
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
        ~nsSilentDownloadNameSet();

        NS_DECL_ISUPPORTS
            NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext);
            NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext);
};

#endif 
