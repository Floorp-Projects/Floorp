/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSComponent.h"
#include "nsNSSCallbacks.h"
#include "nsNSSIOLayer.h"
#include "nsCertVerificationThread.h"

#include "nsNetUtil.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryService.h"
#include "nsIStreamListener.h"
#include "nsIStringBundle.h"
#include "nsIDirectoryService.h"
#include "nsIDOMNode.h"
#include "nsCURILoader.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIProfileChangeStatus.h"
#include "nsNSSCertificate.h"
#include "nsNSSHelper.h"
#include "nsSmartCardMonitor.h"
#include "prlog.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMSmartCardEvent.h"
#include "nsIDOMCrypto.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "nsCRT.h"
#include "nsCRLInfo.h"
#include "nsCertOverrideService.h"

#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsIPrincipal.h"
#include "nsReadableUtils.h"
#include "nsIDateTimeFormat.h"
#include "prtypes.h"
#include "nsIEntropyCollector.h"
#include "nsIBufEntropyCollector.h"
#include "nsIServiceManager.h"
#include "nsIFile.h"
#include "nsITokenPasswordDialogs.h"
#include "nsICRLManager.h"
#include "nsNSSShutDown.h"
#include "nsSmartCardEvent.h"
#include "nsIKeyModule.h"

#include "nss.h"
#include "pk11func.h"
#include "ssl.h"
#include "sslproto.h"
#include "secmod.h"
#include "sechash.h"
#include "secmime.h"
#include "ocsp.h"
#include "cms.h"
#include "nssckbi.h"
#include "base64.h"
#include "secerr.h"
#include "sslerr.h"
#include "cert.h"

#include "nsXULAppAPI.h"

#ifdef XP_WIN
#include "nsILocalFileWin.h"
#endif

extern "C" {
#include "pkcs12.h"
#include "p12plcy.h"
}

using namespace mozilla;
using namespace mozilla::psm;

#ifdef PR_LOGGING
PRLogModuleInfo* gPIPNSSLog = nsnull;
#endif

#define NS_CRYPTO_HASH_BUFFER_SIZE 4096

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);
int nsNSSComponent::mInstanceCount = 0;
bool nsNSSComponent::globalConstFlagUsePKIXVerification = false;

// XXX tmp callback for slot password
extern char * PR_CALLBACK 
pk11PasswordPrompt(PK11SlotInfo *slot, PRBool retry, void *arg);

#define PIPNSS_STRBUNDLE_URL "chrome://pipnss/locale/pipnss.properties"
#define NSSERR_STRBUNDLE_URL "chrome://pipnss/locale/nsserrors.properties"

static PLHashNumber PR_CALLBACK certHashtable_keyHash(const void *key)
{
  if (!key)
    return 0;
  
  SECItem *certKey = (SECItem*)key;
  
  // lazy hash function, sum up all char values of SECItem
  
  PLHashNumber hash = 0;
  unsigned int i = 0;
  unsigned char *c = certKey->data;
  
  for (i = 0; i < certKey->len; ++i, ++c) {
    hash += *c;
  }
  
  return hash;
}

static PRIntn PR_CALLBACK certHashtable_keyCompare(const void *k1, const void *k2)
{
  // return type is a bool, answering the question "are the keys equal?"

  if (!k1 || !k2)
    return false;
  
  SECItem *certKey1 = (SECItem*)k1;
  SECItem *certKey2 = (SECItem*)k2;
  
  if (certKey1->len != certKey2->len) {
    return false;
  }
  
  unsigned int i = 0;
  unsigned char *c1 = certKey1->data;
  unsigned char *c2 = certKey2->data;
  
  for (i = 0; i < certKey1->len; ++i, ++c1, ++c2) {
    if (*c1 != *c2) {
      return false;
    }
  }
  
  return true;
}

static PRIntn PR_CALLBACK certHashtable_valueCompare(const void *v1, const void *v2)
{
  // two values are identical if their keys are identical
  
  if (!v1 || !v2)
    return false;
  
  CERTCertificate *cert1 = (CERTCertificate*)v1;
  CERTCertificate *cert2 = (CERTCertificate*)v2;
  
  return certHashtable_keyCompare(&cert1->certKey, &cert2->certKey);
}

static PRIntn PR_CALLBACK certHashtable_clearEntry(PLHashEntry *he, PRIntn /*index*/, void * /*userdata*/)
{
  if (he && he->value) {
    CERT_DestroyCertificate((CERTCertificate*)he->value);
  }
  
  return HT_ENUMERATE_NEXT;
}

class CRLDownloadEvent : public nsRunnable {
public:
  CRLDownloadEvent(const nsCSubstring &urlString, nsIStreamListener *listener)
    : mURLString(urlString)
    , mListener(listener)
  {}

  // Note that nsNSSComponent is a singleton object across all threads, 
  // and automatic downloads are always scheduled sequentially - that is, 
  // once one crl download is complete, the next one is scheduled
  NS_IMETHOD Run()
  {
    if (!mListener || mURLString.IsEmpty())
      return NS_OK;

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), mURLString);
    if (NS_SUCCEEDED(rv)){
      NS_OpenURI(mListener, nsnull, uri);
    }

    return NS_OK;
  }

private:
  nsCString mURLString;
  nsCOMPtr<nsIStreamListener> mListener;
};

//This class is used to run the callback code
//passed to the event handlers for smart card notification
class nsTokenEventRunnable : public nsIRunnable {
public:
  nsTokenEventRunnable(const nsAString &aType, const nsAString &aTokenName);
  virtual ~nsTokenEventRunnable();

  NS_IMETHOD Run ();
  NS_DECL_ISUPPORTS
private:
  nsString mType;
  nsString mTokenName;
};

// ISuuports implementation for nsTokenEventRunnable
NS_IMPL_THREADSAFE_ISUPPORTS1(nsTokenEventRunnable, nsIRunnable)

nsTokenEventRunnable::nsTokenEventRunnable(const nsAString &aType, 
   const nsAString &aTokenName): mType(aType), mTokenName(aTokenName) { }

nsTokenEventRunnable::~nsTokenEventRunnable() { }

//Implementation that runs the callback passed to 
//crypto.generateCRMFRequest as an event.
NS_IMETHODIMP
nsTokenEventRunnable::Run()
{ 
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  return nssComponent->DispatchEvent(mType, mTokenName);
}

bool nsPSMInitPanic::isPanic = false;

// We must ensure that the nsNSSComponent has been loaded before
// creating any other components.
bool EnsureNSSInitialized(EnsureNSSOperator op)
{
  if (nsPSMInitPanic::GetPanic())
    return false;

  if (GeckoProcessType_Default != XRE_GetProcessType())
  {
    if (op == nssEnsureOnChromeOnly)
    {
      // If the component needs PSM/NSS initialized only on the chrome process,
      // pretend we successfully initiated it but in reality we bypass it.
      // It's up to the programmer to check for process type in such components
      // and take care not to call anything that needs NSS/PSM initiated.
      return true;
    }

    NS_ERROR("Trying to initialize PSM/NSS in a non-chrome process!");
    return false;
  }

  static bool loading = false;
  static PRInt32 haveLoaded = 0;

  switch (op)
  {
    // In following 4 cases we are protected by monitor of XPCOM component
    // manager - we are inside of do_GetService call for nss component, so it is
    // safe to move with the flags here.
  case nssLoadingComponent:
    if (loading)
      return false; // We are reentered during nss component creation
    loading = true;
    return true;

  case nssInitSucceeded:
    NS_ASSERTION(loading, "Bad call to EnsureNSSInitialized(nssInitSucceeded)");
    loading = false;
    PR_AtomicSet(&haveLoaded, 1);
    return true;

  case nssInitFailed:
    NS_ASSERTION(loading, "Bad call to EnsureNSSInitialized(nssInitFailed)");
    loading = false;
    // no break

  case nssShutdown:
    PR_AtomicSet(&haveLoaded, 0);
    return false;

    // In this case we are called from a component to ensure nss initilization.
    // If the component has not yet been loaded and is not currently loading
    // call do_GetService for nss component to ensure it.
  case nssEnsure:
  case nssEnsureOnChromeOnly:
    // We are reentered during nss component creation or nss component is already up
    if (PR_AtomicAdd(&haveLoaded, 0) || loading)
      return true;

    {
    nsCOMPtr<nsINSSComponent> nssComponent
      = do_GetService(PSM_COMPONENT_CONTRACTID);

    // Nss component failed to initialize, inform the caller of that fact.
    // Flags are appropriately set by component constructor itself.
    if (!nssComponent)
      return false;

    bool isInitialized;
    nsresult rv = nssComponent->IsNSSInitialized(&isInitialized);
    return NS_SUCCEEDED(rv) && isInitialized;
    }

  default:
    NS_ASSERTION(false, "Bad operator to EnsureNSSInitialized");
    return false;
  }
}

nsNSSComponent::nsNSSComponent()
  :mutex("nsNSSComponent.mutex"),
   mNSSInitialized(false),
   mCrlTimerLock("nsNSSComponent.mCrlTimerLock"),
   mThreadList(nsnull),
   mCertVerificationThread(NULL)
{
#ifdef PR_LOGGING
  if (!gPIPNSSLog)
    gPIPNSSLog = PR_NewLogModule("pipnss");
#endif
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent::ctor\n"));
  mUpdateTimerInitialized = false;
  crlDownloadTimerOn = false;
  crlsScheduledForDownload = nsnull;
  mTimer = nsnull;
  mObserversRegistered = false;

  // In order to keep startup time lower, we delay loading and 
  // registering all identity data until first needed.
  memset(&mIdentityInfoCallOnce, 0, sizeof(PRCallOnceType));

  NS_ASSERTION( (0 == mInstanceCount), "nsNSSComponent is a singleton, but instantiated multiple times!");
  ++mInstanceCount;
  hashTableCerts = nsnull;
  mShutdownObjectList = nsNSSShutDownList::construct();
  mIsNetworkDown = false;
}

void 
nsNSSComponent::deleteBackgroundThreads()
{
  if (mCertVerificationThread)
  {
    mCertVerificationThread->requestExit();
    delete mCertVerificationThread;
    mCertVerificationThread = nsnull;
  }
}

void
nsNSSComponent::createBackgroundThreads()
{
  NS_ASSERTION(mCertVerificationThread == nsnull,
               "Cert verification thread already created.");

  mCertVerificationThread = new nsCertVerificationThread;
  nsresult rv = mCertVerificationThread->startThread(
    NS_LITERAL_CSTRING("Cert Verify"));

  if (NS_FAILED(rv)) {
    delete mCertVerificationThread;
    mCertVerificationThread = nsnull;
  }
}

nsNSSComponent::~nsNSSComponent()
{
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent::dtor\n"));

  deleteBackgroundThreads();

  if (mUpdateTimerInitialized) {
    {
      MutexAutoLock lock(mCrlTimerLock);
      if (crlDownloadTimerOn) {
        mTimer->Cancel();
      }
      crlDownloadTimerOn = false;
    }
    if(crlsScheduledForDownload != nsnull){
      crlsScheduledForDownload->Reset();
      delete crlsScheduledForDownload;
    }

    mUpdateTimerInitialized = false;
  }

  // All cleanup code requiring services needs to happen in xpcom_shutdown

  ShutdownNSS();
  nsSSLIOLayerHelpers::Cleanup();
  RememberCertErrorsTable::Cleanup();
  --mInstanceCount;
  delete mShutdownObjectList;

  // We are being freed, drop the haveLoaded flag to re-enable
  // potential nss initialization later.
  EnsureNSSInitialized(nssShutdown);

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent::dtor finished\n"));
}

NS_IMETHODIMP
nsNSSComponent::PostEvent(const nsAString &eventType, 
                                                  const nsAString &tokenName)
{
  nsCOMPtr<nsIRunnable> runnable = 
                               new nsTokenEventRunnable(eventType, tokenName);
  if (!runnable) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_DispatchToMainThread(runnable);
}


NS_IMETHODIMP
nsNSSComponent::DispatchEvent(const nsAString &eventType,
                                                 const nsAString &tokenName)
{
  // 'Dispatch' the event to all the windows. 'DispatchEventToWindow()' will
  // first check to see if a given window has requested crypto events.
  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> windowWatcher =
                            do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = windowWatcher->GetWindowEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool hasMoreWindows;

  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreWindows))
         && hasMoreWindows) {
    nsCOMPtr<nsISupports> supports;
    enumerator->GetNext(getter_AddRefs(supports));
    nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(supports));
    if (domWin) {
      nsresult rv2 = DispatchEventToWindow(domWin, eventType, tokenName);
      if (NS_FAILED(rv2)) {
        // return the last failure, don't let a single failure prevent
        // continued delivery of events.
        rv = rv2;
      }
    }
  }
  return rv;
}

nsresult
nsNSSComponent::DispatchEventToWindow(nsIDOMWindow *domWin,
                      const nsAString &eventType, const nsAString &tokenName)
{
  // first walk the children and dispatch their events 
  {
    nsresult rv;
    nsCOMPtr<nsIDOMWindowCollection> frames;
    rv = domWin->GetFrames(getter_AddRefs(frames));
    if (NS_FAILED(rv)) {
      return rv;
    }

    PRUint32 length;
    frames->GetLength(&length);
    PRUint32 i;
    for (i = 0; i < length; i++) {
      nsCOMPtr<nsIDOMWindow> childWin;
      frames->Item(i, getter_AddRefs(childWin));
      DispatchEventToWindow(childWin, eventType, tokenName);
    }
  }

  // check if we've enabled smart card events on this window
  // NOTE: it's not an error to say that we aren't going to dispatch
  // the event.
  {
    nsCOMPtr<nsIDOMWindow> domWindow = domWin;
    if (!domWindow) {
      return NS_OK; // nope, it's not an internal window
    }

    nsCOMPtr<nsIDOMCrypto> crypto;
    domWindow->GetCrypto(getter_AddRefs(crypto));
    if (!crypto) {
      return NS_OK; // nope, it doesn't have a crypto property
    }

    bool boolrv;
    crypto->GetEnableSmartCardEvents(&boolrv);
    if (!boolrv) {
      return NS_OK; // nope, it's not enabled.
    }
  }

  // dispatch the event ...

  nsresult rv;
  // find the document
  nsCOMPtr<nsIDOMDocument> doc;
  rv = domWin->GetDocument(getter_AddRefs(doc));
  if (doc == nsnull) {
    return NS_FAILED(rv) ? rv : NS_ERROR_FAILURE;
  }

  // create the event
  nsCOMPtr<nsIDOMEvent> event;
  rv = doc->CreateEvent(NS_LITERAL_STRING("Events"), 
                        getter_AddRefs(event));
  if (NS_FAILED(rv)) {
    return rv;
  }

  event->InitEvent(eventType, false, true);

  // create the Smart Card Event;
  nsCOMPtr<nsIDOMSmartCardEvent> smartCardEvent = 
                                          new nsSmartCardEvent(tokenName);
  // init the smart card event, fail here if we can't complete the 
  // initialization.
  if (!smartCardEvent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = smartCardEvent->Init(event);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Send it 
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(doc, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool boolrv;
  rv = target->DispatchEvent(smartCardEvent, &boolrv);
  return rv;
}


NS_IMETHODIMP
nsNSSComponent::PIPBundleFormatStringFromName(const char *name,
                                              const PRUnichar **params,
                                              PRUint32 numParams,
                                              nsAString &outString)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mPIPNSSBundle && name) {
    nsXPIDLString result;
    rv = mPIPNSSBundle->FormatStringFromName(NS_ConvertASCIItoUTF16(name).get(),
                                             params, numParams,
                                             getter_Copies(result));
    if (NS_SUCCEEDED(rv)) {
      outString = result;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsNSSComponent::GetPIPNSSBundleString(const char *name,
                                      nsAString &outString)
{
  nsresult rv = NS_ERROR_FAILURE;

  outString.SetLength(0);
  if (mPIPNSSBundle && name) {
    nsXPIDLString result;
    rv = mPIPNSSBundle->GetStringFromName(NS_ConvertASCIItoUTF16(name).get(),
                                          getter_Copies(result));
    if (NS_SUCCEEDED(rv)) {
      outString = result;
      rv = NS_OK;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsNSSComponent::NSSBundleFormatStringFromName(const char *name,
                                              const PRUnichar **params,
                                              PRUint32 numParams,
                                              nsAString &outString)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mNSSErrorsBundle && name) {
    nsXPIDLString result;
    rv = mNSSErrorsBundle->FormatStringFromName(NS_ConvertASCIItoUTF16(name).get(),
                                                params, numParams,
                                                getter_Copies(result));
    if (NS_SUCCEEDED(rv)) {
      outString = result;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsNSSComponent::GetNSSBundleString(const char *name,
                                   nsAString &outString)
{
  nsresult rv = NS_ERROR_FAILURE;

  outString.SetLength(0);
  if (mNSSErrorsBundle && name) {
    nsXPIDLString result;
    rv = mNSSErrorsBundle->GetStringFromName(NS_ConvertASCIItoUTF16(name).get(),
                                             getter_Copies(result));
    if (NS_SUCCEEDED(rv)) {
      outString = result;
      rv = NS_OK;
    }
  }

  return rv;
}

void
nsNSSComponent::LaunchSmartCardThreads()
{
  nsNSSShutDownPreventionLock locker;
  {
    SECMODModuleList *list;
    SECMODListLock *lock = SECMOD_GetDefaultModuleListLock();
    if (!lock) {
        PR_LOG(gPIPNSSLog, PR_LOG_ERROR,
               ("Couldn't get the module list lock, can't launch smart card threads\n"));
        return;
    }
    SECMOD_GetReadLock(lock);
    list = SECMOD_GetDefaultModuleList();

    while (list) {
      SECMODModule *module = list->module;
      LaunchSmartCardThread(module);
      list = list->next;
    }
    SECMOD_ReleaseReadLock(lock);
  }
}

NS_IMETHODIMP
nsNSSComponent::LaunchSmartCardThread(SECMODModule *module)
{
  SmartCardMonitoringThread *newThread;
  if (SECMOD_HasRemovableSlots(module)) {
    if (mThreadList == nsnull) {
      mThreadList = new SmartCardThreadList();
      if (!mThreadList) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    newThread = new SmartCardMonitoringThread(module);
    if (!newThread) {
	return NS_ERROR_OUT_OF_MEMORY;
    }
    // newThread is adopted by the add.
    return mThreadList->Add(newThread);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::ShutdownSmartCardThread(SECMODModule *module)
{
  if (!mThreadList) {
    return NS_OK;
  }
  mThreadList->Remove(module);
  return NS_OK;
}

void
nsNSSComponent::ShutdownSmartCardThreads()
{
  delete mThreadList;
  mThreadList = nsnull;
}

static char *
nss_addEscape(const char *string, char quote)
{
    char *newString = 0;
    int escapes = 0, size = 0;
    const char *src;
    char *dest;

    for (src=string; *src ; src++) {
        if ((*src == quote) || (*src == '\\')) {
          escapes++;
        }
        size++;
    }

    newString = (char*)PORT_ZAlloc(escapes+size+1);
    if (newString == NULL) {
        return NULL;
    }

    for (src=string, dest=newString; *src; src++,dest++) {
        if ((*src == quote) || (*src == '\\')) {
            *dest++ = '\\';
        }
        *dest = *src;
    }

    return newString;
}

void
nsNSSComponent::InstallLoadableRoots()
{
  nsNSSShutDownPreventionLock locker;
  SECMODModule *RootsModule = nsnull;

  // In the past we used SECMOD_AddNewModule to load our module containing
  // root CA certificates. This caused problems, refer to bug 176501.
  // On startup, we fix our database and clean any stored module reference,
  // and will use SECMOD_LoadUserModule to temporarily load it
  // for the session. (This approach requires to clean up 
  // using SECMOD_UnloadUserModule at the end of the session.)

  {
    // Find module containing root certs

    SECMODModuleList *list;
    SECMODListLock *lock = SECMOD_GetDefaultModuleListLock();
    if (!lock) {
        PR_LOG(gPIPNSSLog, PR_LOG_ERROR,
               ("Couldn't get the module list lock, can't install loadable roots\n"));
        return;
    }
    SECMOD_GetReadLock(lock);
    list = SECMOD_GetDefaultModuleList();

    while (!RootsModule && list) {
      SECMODModule *module = list->module;

      for (int i=0; i < module->slotCount; i++) {
        PK11SlotInfo *slot = module->slots[i];
        if (PK11_IsPresent(slot)) {
          if (PK11_HasRootCerts(slot)) {
            RootsModule = SECMOD_ReferenceModule(module);
            break;
          }
        }
      }

      list = list->next;
    }
    SECMOD_ReleaseReadLock(lock);
  }

  if (RootsModule) {
    PRInt32 modType;
    SECMOD_DeleteModule(RootsModule->commonName, &modType);
    SECMOD_DestroyModule(RootsModule);
    RootsModule = nsnull;
  }

  // Find the best Roots module for our purposes.
  // Prefer the application's installation directory,
  // but also ensure the library is at least the version we expect.

  nsresult rv;
  nsAutoString modName;
  rv = GetPIPNSSBundleString("RootCertModuleName", modName);
  if (NS_FAILED(rv)) return;

  nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  if (!directoryService)
    return;

  static const char nss_lib[] = "nss3";
  const char *possible_ckbi_locations[] = {
    nss_lib, // This special value means: search for ckbi in the directory
             // where nss3 is.
    NS_XPCOM_CURRENT_PROCESS_DIR,
    NS_GRE_DIR,
    0 // This special value means: 
      //   search for ckbi in the directories on the shared
      //   library/DLL search path
  };

  for (size_t il = 0; il < sizeof(possible_ckbi_locations)/sizeof(const char*); ++il) {
    nsCOMPtr<nsIFile> mozFile;
    char *fullLibraryPath = nsnull;

    if (!possible_ckbi_locations[il])
    {
      fullLibraryPath = PR_GetLibraryName(nsnull, "nssckbi");
    }
    else
    {
      if (possible_ckbi_locations[il] == nss_lib) {
        // Get the location of the nss3 library.
        char *nss_path = PR_GetLibraryFilePathname(DLL_PREFIX "nss3" DLL_SUFFIX,
                                                   (PRFuncPtr) NSS_Initialize);
        if (!nss_path) {
          continue;
        }
        // Get the directory containing the nss3 library.
        nsCOMPtr<nsIFile> nssLib(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv)) {
          rv = nssLib->InitWithNativePath(nsDependentCString(nss_path));
        }
        PR_Free(nss_path);
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIFile> file;
          if (NS_SUCCEEDED(nssLib->GetParent(getter_AddRefs(file)))) {
            mozFile = do_QueryInterface(file);
          }
        }
      } else {
        directoryService->Get( possible_ckbi_locations[il],
                               NS_GET_IID(nsIFile), 
                               getter_AddRefs(mozFile));
      }
  
      if (!mozFile) {
        continue;
      }

      nsCAutoString processDir;
      mozFile->GetNativePath(processDir);
      fullLibraryPath = PR_GetLibraryName(processDir.get(), "nssckbi");
    }

    if (!fullLibraryPath) {
      continue;
    }

    char *escaped_fullLibraryPath = nss_addEscape(fullLibraryPath, '\"');
    if (!escaped_fullLibraryPath) {
      PR_FreeLibraryName(fullLibraryPath); // allocated by NSPR
      continue;
    }

    /* If a module exists with the same name, delete it. */
    NS_ConvertUTF16toUTF8 modNameUTF8(modName);
    int modType;
    SECMOD_DeleteModule(const_cast<char*>(modNameUTF8.get()), &modType);

    nsCString pkcs11moduleSpec;
    pkcs11moduleSpec.Append(NS_LITERAL_CSTRING("name=\""));
    pkcs11moduleSpec.Append(modNameUTF8.get());
    pkcs11moduleSpec.Append(NS_LITERAL_CSTRING("\" library=\""));
    pkcs11moduleSpec.Append(escaped_fullLibraryPath);
    pkcs11moduleSpec.Append(NS_LITERAL_CSTRING("\""));

    PR_FreeLibraryName(fullLibraryPath); // allocated by NSPR
    PORT_Free(escaped_fullLibraryPath);

    RootsModule =
      SECMOD_LoadUserModule(const_cast<char*>(pkcs11moduleSpec.get()), 
                            nsnull, // no parent 
                            false); // do not recurse

    if (RootsModule) {
      bool found = (RootsModule->loaded);

      SECMOD_DestroyModule(RootsModule);
      RootsModule = nsnull;

      if (found) {
        break;
      }
    }
  }
}

void 
nsNSSComponent::UnloadLoadableRoots()
{
  nsresult rv;
  nsAutoString modName;
  rv = GetPIPNSSBundleString("RootCertModuleName", modName);
  if (NS_FAILED(rv)) return;

  NS_ConvertUTF16toUTF8 modNameUTF8(modName);
  SECMODModule *RootsModule = SECMOD_FindModule(modNameUTF8.get());

  if (RootsModule) {
    SECMOD_UnloadUserModule(RootsModule);
    SECMOD_DestroyModule(RootsModule);
  }
}

nsresult
nsNSSComponent::ConfigureInternalPKCS11Token()
{
  nsNSSShutDownPreventionLock locker;
  nsAutoString manufacturerID;
  nsAutoString libraryDescription;
  nsAutoString tokenDescription;
  nsAutoString privateTokenDescription;
  nsAutoString slotDescription;
  nsAutoString privateSlotDescription;
  nsAutoString fips140TokenDescription;
  nsAutoString fips140SlotDescription;

  nsresult rv;
  rv = GetPIPNSSBundleString("ManufacturerID", manufacturerID);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("LibraryDescription", libraryDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("TokenDescription", tokenDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("PrivateTokenDescription", privateTokenDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("SlotDescription", slotDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("PrivateSlotDescription", privateSlotDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("Fips140TokenDescription", fips140TokenDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("Fips140SlotDescription", fips140SlotDescription);
  if (NS_FAILED(rv)) return rv;

  PK11_ConfigurePKCS11(NS_ConvertUTF16toUTF8(manufacturerID).get(),
                       NS_ConvertUTF16toUTF8(libraryDescription).get(),
                       NS_ConvertUTF16toUTF8(tokenDescription).get(),
                       NS_ConvertUTF16toUTF8(privateTokenDescription).get(),
                       NS_ConvertUTF16toUTF8(slotDescription).get(),
                       NS_ConvertUTF16toUTF8(privateSlotDescription).get(),
                       NS_ConvertUTF16toUTF8(fips140TokenDescription).get(),
                       NS_ConvertUTF16toUTF8(fips140SlotDescription).get(),
                       0, 0);
  return NS_OK;
}

nsresult
nsNSSComponent::InitializePIPNSSBundle()
{
  // Called during init only, no mutex required.

  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  if (NS_FAILED(rv) || !bundleService) 
    return NS_ERROR_FAILURE;
  
  bundleService->CreateBundle(PIPNSS_STRBUNDLE_URL,
                              getter_AddRefs(mPIPNSSBundle));
  if (!mPIPNSSBundle)
    rv = NS_ERROR_FAILURE;

  bundleService->CreateBundle(NSSERR_STRBUNDLE_URL,
                              getter_AddRefs(mNSSErrorsBundle));
  if (!mNSSErrorsBundle)
    rv = NS_ERROR_FAILURE;

  return rv;
}

nsresult
nsNSSComponent::RegisterPSMContentListener()
{
  // Called during init only, no mutex required.

  nsresult rv = NS_OK;
  if (!mPSMContentListener) {
    nsCOMPtr<nsIURILoader> dispatcher(do_GetService(NS_URI_LOADER_CONTRACTID));
    if (dispatcher) {
      mPSMContentListener = do_CreateInstance(NS_PSMCONTENTLISTEN_CONTRACTID);
      rv = dispatcher->RegisterContentListener(mPSMContentListener);
    }
  }
  return rv;
}

/* Table of pref names and SSL cipher ID */
typedef struct {
  const char* pref;
  long id;
} CipherPref;

static CipherPref CipherPrefs[] = {
 /* SSL3/TLS cipher suites*/
 {"security.ssl3.rsa_rc4_128_md5", SSL_RSA_WITH_RC4_128_MD5}, // 128-bit RC4 encryption with RSA and an MD5 MAC
 {"security.ssl3.rsa_rc4_128_sha", SSL_RSA_WITH_RC4_128_SHA}, // 128-bit RC4 encryption with RSA and a SHA1 MAC
 {"security.ssl3.rsa_fips_des_ede3_sha", SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with RSA and a SHA1 MAC (FIPS)
 {"security.ssl3.rsa_des_ede3_sha", SSL_RSA_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with RSA and a SHA1 MAC
 {"security.ssl3.rsa_fips_des_sha", SSL_RSA_FIPS_WITH_DES_CBC_SHA}, // 56-bit DES encryption with RSA and a SHA1 MAC (FIPS)
 {"security.ssl3.rsa_des_sha", SSL_RSA_WITH_DES_CBC_SHA}, // 56-bit DES encryption with RSA and a SHA1 MAC
 {"security.ssl3.rsa_1024_rc4_56_sha", TLS_RSA_EXPORT1024_WITH_RC4_56_SHA}, // 56-bit RC4 encryption with RSA and a SHA1 MAC (export)
 {"security.ssl3.rsa_1024_des_cbc_sha", TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA}, // 56-bit DES encryption with RSA and a SHA1 MAC (export)
 {"security.ssl3.rsa_rc4_40_md5", SSL_RSA_EXPORT_WITH_RC4_40_MD5}, // 40-bit RC4 encryption with RSA and an MD5 MAC (export)
 {"security.ssl3.rsa_rc2_40_md5", SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5}, // 40-bit RC2 encryption with RSA and an MD5 MAC (export)
 /* Extra SSL3/TLS cipher suites */
 {"security.ssl3.dhe_rsa_camellia_256_sha", TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA}, // 256-bit Camellia encryption with RSA, DHE, and a SHA1 MAC
 {"security.ssl3.dhe_dss_camellia_256_sha", TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA}, // 256-bit Camellia encryption with DSA, DHE, and a SHA1 MAC
 {"security.ssl3.rsa_camellia_256_sha", TLS_RSA_WITH_CAMELLIA_256_CBC_SHA}, // 256-bit Camellia encryption with RSA and a SHA1 MAC
 {"security.ssl3.dhe_rsa_aes_256_sha", TLS_DHE_RSA_WITH_AES_256_CBC_SHA}, // 256-bit AES encryption with RSA, DHE, and a SHA1 MAC
 {"security.ssl3.dhe_dss_aes_256_sha", TLS_DHE_DSS_WITH_AES_256_CBC_SHA}, // 256-bit AES encryption with DSA, DHE, and a SHA1 MAC
 {"security.ssl3.rsa_aes_256_sha", TLS_RSA_WITH_AES_256_CBC_SHA}, // 256-bit AES encryption with RSA and a SHA1 MAC
   /* TLS_DHE_DSS_WITH_RC4_128_SHA // 128-bit RC4 encryption with DSA, DHE, and a SHA1 MAC
      If this cipher gets included at a later time, it should get added at this position */
 {"security.ssl3.ecdhe_ecdsa_aes_256_sha", TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA}, // 256-bit AES encryption with ECDHE-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdhe_ecdsa_aes_128_sha", TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with ECDHE-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdhe_ecdsa_des_ede3_sha", TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with ECDHE-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdhe_ecdsa_rc4_128_sha", TLS_ECDHE_ECDSA_WITH_RC4_128_SHA}, // 128-bit RC4 encryption with ECDHE-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdhe_ecdsa_null_sha", TLS_ECDHE_ECDSA_WITH_NULL_SHA}, // No encryption with ECDHE-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdhe_rsa_aes_256_sha", TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA}, // 256-bit AES encryption with ECDHE-RSA and a SHA1 MAC
 {"security.ssl3.ecdhe_rsa_aes_128_sha", TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with ECDHE-RSA and a SHA1 MAC
 {"security.ssl3.ecdhe_rsa_des_ede3_sha", TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with ECDHE-RSA and a SHA1 MAC
 {"security.ssl3.ecdhe_rsa_rc4_128_sha", TLS_ECDHE_RSA_WITH_RC4_128_SHA}, // 128-bit RC4 encryption with ECDHE-RSA and a SHA1 MAC
 {"security.ssl3.ecdhe_rsa_null_sha", TLS_ECDHE_RSA_WITH_NULL_SHA}, // No encryption with ECDHE-RSA and a SHA1 MAC
 {"security.ssl3.ecdh_ecdsa_aes_256_sha", TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA}, // 256-bit AES encryption with ECDH-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdh_ecdsa_aes_128_sha", TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with ECDH-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdh_ecdsa_des_ede3_sha", TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with ECDH-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdh_ecdsa_rc4_128_sha", TLS_ECDH_ECDSA_WITH_RC4_128_SHA}, // 128-bit RC4 encryption with ECDH-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdh_ecdsa_null_sha", TLS_ECDH_ECDSA_WITH_NULL_SHA}, // No encryption with ECDH-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdh_rsa_aes_256_sha", TLS_ECDH_RSA_WITH_AES_256_CBC_SHA}, // 256-bit AES encryption with ECDH-RSA and a SHA1 MAC
 {"security.ssl3.ecdh_rsa_aes_128_sha", TLS_ECDH_RSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with ECDH-RSA and a SHA1 MAC
 {"security.ssl3.ecdh_rsa_des_ede3_sha", TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with ECDH-RSA and a SHA1 MAC
 {"security.ssl3.ecdh_rsa_rc4_128_sha", TLS_ECDH_RSA_WITH_RC4_128_SHA}, // 128-bit RC4 encryption with ECDH-RSA and a SHA1 MAC
 {"security.ssl3.ecdh_rsa_null_sha", TLS_ECDH_RSA_WITH_NULL_SHA}, // No encryption with ECDH-RSA and a SHA1 MAC
 {"security.ssl3.dhe_rsa_camellia_128_sha", TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA}, // 128-bit Camellia encryption with RSA, DHE, and a SHA1 MAC
 {"security.ssl3.dhe_dss_camellia_128_sha", TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA}, // 128-bit Camellia encryption with DSA, DHE, and a SHA1 MAC
 {"security.ssl3.rsa_camellia_128_sha", TLS_RSA_WITH_CAMELLIA_128_CBC_SHA}, // 128-bit Camellia encryption with RSA and a SHA1 MAC
 {"security.ssl3.dhe_rsa_aes_128_sha", TLS_DHE_RSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with RSA, DHE, and a SHA1 MAC
 {"security.ssl3.dhe_dss_aes_128_sha", TLS_DHE_DSS_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with DSA, DHE, and a SHA1 MAC
 {"security.ssl3.rsa_aes_128_sha", TLS_RSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with RSA and a SHA1 MAC
 {"security.ssl3.dhe_rsa_des_ede3_sha", SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with RSA, DHE, and a SHA1 MAC
 {"security.ssl3.dhe_dss_des_ede3_sha", SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with DSA, DHE, and a SHA1 MAC
 {"security.ssl3.dhe_rsa_des_sha", SSL_DHE_RSA_WITH_DES_CBC_SHA}, // 56-bit DES encryption with RSA, DHE, and a SHA1 MAC
 {"security.ssl3.dhe_dss_des_sha", SSL_DHE_DSS_WITH_DES_CBC_SHA}, // 56-bit DES encryption with DSA, DHE, and a SHA1 MAC
 {"security.ssl3.rsa_null_sha", SSL_RSA_WITH_NULL_SHA}, // No encryption with RSA authentication and a SHA1 MAC
 {"security.ssl3.rsa_null_md5", SSL_RSA_WITH_NULL_MD5}, // No encryption with RSA authentication and an MD5 MAC
 {"security.ssl3.rsa_seed_sha", TLS_RSA_WITH_SEED_CBC_SHA}, // SEED encryption with RSA and a SHA1 MAC
 {NULL, 0} /* end marker */
};

static void
setNonPkixOcspEnabled(PRInt32 ocspEnabled, nsIPrefBranch * pref)
{
  switch (ocspEnabled) {
  case 0:
    CERT_DisableOCSPChecking(CERT_GetDefaultCertDB());
    CERT_DisableOCSPDefaultResponder(CERT_GetDefaultCertDB());
    break;
  case 1:
    CERT_EnableOCSPChecking(CERT_GetDefaultCertDB());
    CERT_DisableOCSPDefaultResponder(CERT_GetDefaultCertDB());
    break;
  case 2:
    {
      char *signingCA = nsnull;
      char *url = nsnull;

      // Get the signing CA and service url //
      pref->GetCharPref("security.OCSP.signingCA", &signingCA);
      pref->GetCharPref("security.OCSP.URL", &url);

      // Set OCSP up
      CERT_EnableOCSPChecking(CERT_GetDefaultCertDB());
      CERT_SetOCSPDefaultResponder(CERT_GetDefaultCertDB(), url, signingCA);
      CERT_EnableOCSPDefaultResponder(CERT_GetDefaultCertDB());

      nsMemory::Free(signingCA);
      nsMemory::Free(url);
    }
    break;
  }
}

#define CRL_DOWNLOAD_DEFAULT false
#define OCSP_ENABLED_DEFAULT 1
#define OCSP_REQUIRED_DEFAULT 0
#define FRESH_REVOCATION_REQUIRED_DEFAULT false
#define MISSING_CERT_DOWNLOAD_DEFAULT false
#define FIRST_REVO_METHOD_DEFAULT "ocsp"
#define USE_NSS_LIBPKIX_DEFAULT false

// Caller must hold a lock on nsNSSComponent::mutex when calling this function
void nsNSSComponent::setValidationOptions(nsIPrefBranch * pref)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv;

  bool crlDownloading;
  rv = pref->GetBoolPref("security.CRL_download.enabled", &crlDownloading);
  if (NS_FAILED(rv))
    crlDownloading = CRL_DOWNLOAD_DEFAULT;
  
  PRInt32 ocspEnabled;
  rv = pref->GetIntPref("security.OCSP.enabled", &ocspEnabled);
  // 0 = disabled, 1 = enabled, 
  // 2 = enabled with given default responder
  if (NS_FAILED(rv))
    ocspEnabled = OCSP_ENABLED_DEFAULT;

  bool ocspRequired;
  rv = pref->GetBoolPref("security.OCSP.require", &ocspRequired);
  if (NS_FAILED(rv))
    ocspRequired = OCSP_REQUIRED_DEFAULT;

  bool anyFreshRequired;
  rv = pref->GetBoolPref("security.fresh_revocation_info.require", &anyFreshRequired);
  if (NS_FAILED(rv))
    anyFreshRequired = FRESH_REVOCATION_REQUIRED_DEFAULT;
  
  bool aiaDownloadEnabled;
  rv = pref->GetBoolPref("security.missing_cert_download.enabled", &aiaDownloadEnabled);
  if (NS_FAILED(rv))
    aiaDownloadEnabled = MISSING_CERT_DOWNLOAD_DEFAULT;

  nsCString firstNetworkRevo;
  rv = pref->GetCharPref("security.first_network_revocation_method", getter_Copies(firstNetworkRevo));
  if (NS_FAILED(rv))
    firstNetworkRevo = FIRST_REVO_METHOD_DEFAULT;
  
  setNonPkixOcspEnabled(ocspEnabled, pref);
  
  CERT_SetOCSPFailureMode( ocspRequired ?
                           ocspMode_FailureIsVerificationFailure
                           : ocspMode_FailureIsNotAVerificationFailure);

  nsRefPtr<nsCERTValInParamWrapper> newCVIN = new nsCERTValInParamWrapper;
  if (NS_SUCCEEDED(newCVIN->Construct(
      aiaDownloadEnabled ? 
        nsCERTValInParamWrapper::missing_cert_download_on : nsCERTValInParamWrapper::missing_cert_download_off,
      crlDownloading ?
        nsCERTValInParamWrapper::crl_download_allowed : nsCERTValInParamWrapper::crl_local_only,
      ocspEnabled ? 
        nsCERTValInParamWrapper::ocsp_on : nsCERTValInParamWrapper::ocsp_off,
      ocspRequired ? 
        nsCERTValInParamWrapper::ocsp_strict : nsCERTValInParamWrapper::ocsp_relaxed,
      anyFreshRequired ?
        nsCERTValInParamWrapper::any_revo_strict : nsCERTValInParamWrapper::any_revo_relaxed,
      firstNetworkRevo.get()))) {
    // Swap to new defaults, and will cause the old defaults to be released,
    // as soon as any concurrent use of the old default objects has finished.
    mDefaultCERTValInParam = newCVIN;
  }

  /*
    * The new defaults might change the validity of already established SSL sessions,
    * let's not reuse them.
    */
  SSL_ClearSessionCache();
}

NS_IMETHODIMP
nsNSSComponent::SkipOcsp()
{
  nsNSSShutDownPreventionLock locker;
  CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();

  SECStatus rv = CERT_DisableOCSPChecking(certdb);
  return (rv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSComponent::SkipOcspOff()
{
  nsNSSShutDownPreventionLock locker;
  PRInt32 ocspEnabled;
  if (NS_FAILED(mPrefBranch->GetIntPref("security.OCSP.enabled", &ocspEnabled)))
    ocspEnabled = OCSP_ENABLED_DEFAULT;
  // 0 = disabled, 1 = enabled, 
  // 2 = enabled with given default responder
  
  setNonPkixOcspEnabled(ocspEnabled, mPrefBranch);

  if (ocspEnabled)
    SSL_ClearSessionCache();

  return NS_OK;
}

nsresult
nsNSSComponent::PostCRLImportEvent(const nsCSubstring &urlString,
                                   nsIStreamListener *listener)
{
  //Create the event
  nsCOMPtr<nsIRunnable> event = new CRLDownloadEvent(urlString, listener);
  if (!event)
    return NS_ERROR_OUT_OF_MEMORY;

  //Get a handle to the ui thread
  return NS_DispatchToMainThread(event);
}

nsresult
nsNSSComponent::DownloadCRLDirectly(nsAutoString url, nsAutoString key)
{
  //This api is meant to support direct interactive update of crl from the crl manager
  //or other such ui.
  nsCOMPtr<nsIStreamListener> listener =
      new PSMContentDownloader(PSMContentDownloader::PKCS7_CRL);
  
  NS_ConvertUTF16toUTF8 url8(url);
  return PostCRLImportEvent(url8, listener);
}

nsresult nsNSSComponent::DownloadCrlSilently()
{
  //Add this attempt to the hashtable
  nsStringKey hashKey(mCrlUpdateKey.get());
  crlsScheduledForDownload->Put(&hashKey,(void *)nsnull);
    
  //Set up the download handler
  nsRefPtr<PSMContentDownloader> psmDownloader =
      new PSMContentDownloader(PSMContentDownloader::PKCS7_CRL);
  psmDownloader->setSilentDownload(true);
  psmDownloader->setCrlAutodownloadKey(mCrlUpdateKey);
  
  //Now get the url string
  NS_ConvertUTF16toUTF8 url8(mDownloadURL);
  return PostCRLImportEvent(url8, psmDownloader);
}

nsresult nsNSSComponent::getParamsForNextCrlToDownload(nsAutoString *url, PRTime *time, nsAutoString *key)
{
  const char *updateEnabledPref = CRL_AUTOUPDATE_ENABLED_PREF;
  const char *updateTimePref = CRL_AUTOUPDATE_TIME_PREF;
  const char *updateURLPref = CRL_AUTOUPDATE_URL_PREF;
  char **allCrlsToBeUpdated;
  PRUint32 noOfCrls;
  PRTime nearestUpdateTime = 0;
  nsAutoString crlKey;
  char *tempUrl;
  nsresult rv;
  
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID,&rv);
  if(NS_FAILED(rv)){
    return rv;
  }

  rv = pref->GetChildList(updateEnabledPref, &noOfCrls, &allCrlsToBeUpdated);
  if ( (NS_FAILED(rv)) || (noOfCrls==0) ){
    return NS_ERROR_FAILURE;
  }

  for(PRUint32 i=0;i<noOfCrls;i++) {
    //First check if update pref is enabled for this crl
    bool autoUpdateEnabled = false;
    rv = pref->GetBoolPref(*(allCrlsToBeUpdated+i), &autoUpdateEnabled);
    if (NS_FAILED(rv) || !autoUpdateEnabled) {
      continue;
    }

    nsAutoString tempCrlKey;

    //Now, generate the crl key. Same key would be used as hashkey as well
    nsCAutoString enabledPrefCString(*(allCrlsToBeUpdated+i));
    enabledPrefCString.ReplaceSubstring(updateEnabledPref,".");
    tempCrlKey.AssignWithConversion(enabledPrefCString.get());
      
    //Check if this crl has already been scheduled. Its presence in the hashtable
    //implies that it has been scheduled already this client session, and
    //is either in the process of being downloaded, or its download failed
    //for some reason. In the second case, we will not retry in the current client session
    nsStringKey hashKey(tempCrlKey.get());
    if(crlsScheduledForDownload->Exists(&hashKey)){
      continue;
    }

    char *tempTimeString;
    PRTime tempTime;
    nsCAutoString timingPrefCString(updateTimePref);
    LossyAppendUTF16toASCII(tempCrlKey, timingPrefCString);
    // No PRTime/Int64 type in prefs; stored as string; parsed here as PRInt64
    rv = pref->GetCharPref(timingPrefCString.get(), &tempTimeString);
    if (NS_FAILED(rv)){
      // Assume corrupted. Force download. Pref should be reset after download.
      tempTime = PR_Now();
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
             ("get %s failed: forcing download\n", timingPrefCString.get()));
    } else {
      tempTime = (PRTime)nsCRT::atoll(tempTimeString);
      nsMemory::Free(tempTimeString);
      // nsCRT::atoll parses the first token in the string; three possibilities
      //  -1- Alpha char: returns 0; change to PR_Now() and force update.
      //  -2- Number (between epoch and PR_Now(), e.g. 0 - 1332280017 for
      //      Tue Mar 20, 2012, 2:46pm approx): includes formatted date 
      //      values (previous method of storing update date, e.g year, month 
      //      or day, 2012, 1-31, 1-12 etc). Less than PR_Now() forces 
      //      autoupdate.
      //  -3- Number (larger than PR_Now()): no forced autoupdate
      // Note: corrupt values within range of -2- will have an implicit 
      // unflagged recovery. Corrupt values in range of -3- will be unflagged
      // and unrecovered by this code.
      if (tempTime == 0)
        tempTime = PR_Now();
#ifdef PR_LOGGING
      PRExplodedTime explodedTime;
      PR_ExplodeTime(tempTime, PR_GMTParameters, &explodedTime);
      // Note: tm_month starts from 0 = Jan, hence +1
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
             ("%s tempTime(%lli) "
              "(m/d/y h:m:s = %02d/%02d/%d %02d:%02d:%02d GMT\n",
              timingPrefCString.get(), tempTime,
              explodedTime.tm_month+1, explodedTime.tm_mday,
              explodedTime.tm_year, explodedTime.tm_hour,
              explodedTime.tm_min, explodedTime.tm_sec));
#endif
    }

    if(nearestUpdateTime == 0 || tempTime < nearestUpdateTime){
      nsCAutoString urlPrefCString(updateURLPref);
      LossyAppendUTF16toASCII(tempCrlKey, urlPrefCString);
      rv = pref->GetCharPref(urlPrefCString.get(), &tempUrl);
      if (NS_FAILED(rv) || (!tempUrl)){
        continue;
      }
      nearestUpdateTime = tempTime;
      crlKey = tempCrlKey;
    }
  }

  if(noOfCrls > 0)
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(noOfCrls, allCrlsToBeUpdated);

  if(nearestUpdateTime > 0){
    *time = nearestUpdateTime;
    url->AssignWithConversion((const char *)tempUrl);
    nsMemory::Free(tempUrl);
    *key = crlKey;
    rv = NS_OK;
  } else{
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
nsNSSComponent::Notify(nsITimer *timer)
{
  //Timer has fired. So set the flag accordingly
  {
    MutexAutoLock lock(mCrlTimerLock);
    crlDownloadTimerOn = false;
  }

  //First, handle this download
  DownloadCrlSilently();

  //Dont Worry if successful or not
  //Set the next timer
  DefineNextTimer();
  return NS_OK;
}

nsresult
nsNSSComponent::RemoveCrlFromList(nsAutoString key)
{
  nsStringKey hashKey(key.get());
  if(crlsScheduledForDownload->Exists(&hashKey)){
    crlsScheduledForDownload->Remove(&hashKey);
  }
  return NS_OK;
}

nsresult
nsNSSComponent::DefineNextTimer()
{
  PRTime nextFiring;
  PRTime now = PR_Now();
  PRUint64 diff;
  PRUint32 interval;
  PRUint32 primaryDelay = CRL_AUTOUPDATE_DEFAULT_DELAY;
  nsresult rv;

  if(!mTimer){
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if(NS_FAILED(rv))
      return rv;
  }

  //If some timer is already running, cancel it. Thus, the request that came last,
  //wins. This would ensure that in no way we end up setting two different timers
  //This part should be synchronized because this function might be called from separate
  //threads

  MutexAutoLock lock(mCrlTimerLock);

  if (crlDownloadTimerOn) {
    mTimer->Cancel();
  }

  rv = getParamsForNextCrlToDownload(&mDownloadURL, &nextFiring, &mCrlUpdateKey);
  //If there are no more crls to be updated any time in future
  if(NS_FAILED(rv)){
    // Return - no error - just implies nothing to schedule
    return NS_OK;
  }
     
  //Define the firing interval, from NOW
  if ( now < nextFiring) {
    LL_SUB(diff,nextFiring,now);
    LL_L2UI(interval, diff);
    //Now, we are doing 32 operations - so, don't need LL_ functions...
    interval = interval/PR_USEC_PER_MSEC;
  }else {
    interval = primaryDelay;
  }
  
  mTimer->InitWithCallback(static_cast<nsITimerCallback*>(this), 
                           interval,
                           nsITimer::TYPE_ONE_SHOT);
  crlDownloadTimerOn = true;

  return NS_OK;
}

//Note that the StopCRLUpdateTimer and InitializeCRLUpdateTimer functions should never be called
//simultaneously from diff threads - they are NOT threadsafe. But, since there is no chance of 
//that happening, there is not much benefit it trying to make it so at this point
nsresult
nsNSSComponent::StopCRLUpdateTimer()
{
  
  //If it is at all running. 
  if (mUpdateTimerInitialized) {
    if(crlsScheduledForDownload != nsnull){
      crlsScheduledForDownload->Reset();
      delete crlsScheduledForDownload;
      crlsScheduledForDownload = nsnull;
    }
    {
      MutexAutoLock lock(mCrlTimerLock);
      if (crlDownloadTimerOn) {
        mTimer->Cancel();
      }
      crlDownloadTimerOn = false;
    }
    mUpdateTimerInitialized = false;
  }

  return NS_OK;
}

nsresult
nsNSSComponent::InitializeCRLUpdateTimer()
{
  nsresult rv;
    
  //First check if this is already initialized. Then we stop it.
  if (!mUpdateTimerInitialized) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if(NS_FAILED(rv)){
      return rv;
    }
    crlsScheduledForDownload = new nsHashtable(16, true);
    DefineNextTimer();
    mUpdateTimerInitialized = true;  
  } 

  return NS_OK;
}

#ifdef XP_MACOSX
void
nsNSSComponent::TryCFM2MachOMigration(nsIFile *cfmPath, nsIFile *machoPath)
{
  // We will modify the parameters.
  //
  // If neither cert7.db, cert8.db, key3.db, are available, 
  // copy from filenames that were used in the old days
  // test for key3.db first, since a new profile might only contain cert8.db, 
  // but not cert7.db - this optimizes number of tests

  NS_NAMED_LITERAL_CSTRING(cstr_key3db, "key3.db");
  NS_NAMED_LITERAL_CSTRING(cstr_cert7db, "cert7.db");
  NS_NAMED_LITERAL_CSTRING(cstr_cert8db, "cert8.db");
  NS_NAMED_LITERAL_CSTRING(cstr_keydatabase3, "Key Database3");
  NS_NAMED_LITERAL_CSTRING(cstr_certificate7, "Certificates7");
  NS_NAMED_LITERAL_CSTRING(cstr_certificate8, "Certificates8");

  bool bExists;
  nsresult rv;

  nsCOMPtr<nsIFile> macho_key3db;
  rv = machoPath->Clone(getter_AddRefs(macho_key3db));
  if (NS_FAILED(rv)) {
    return;
  }

  macho_key3db->AppendNative(cstr_key3db);
  rv = macho_key3db->Exists(&bExists);
  if (NS_FAILED(rv) || bExists) {
    return;
  }

  nsCOMPtr<nsIFile> macho_cert7db;
  rv = machoPath->Clone(getter_AddRefs(macho_cert7db));
  if (NS_FAILED(rv)) {
    return;
  }

  macho_cert7db->AppendNative(cstr_cert7db);
  rv = macho_cert7db->Exists(&bExists);
  if (NS_FAILED(rv) || bExists) {
    return;
  }

  nsCOMPtr<nsIFile> macho_cert8db;
  rv = machoPath->Clone(getter_AddRefs(macho_cert8db));
  if (NS_FAILED(rv)) {
    return;
  }

  macho_cert8db->AppendNative(cstr_cert8db);
  rv = macho_cert7db->Exists(&bExists);
  if (NS_FAILED(rv) || bExists) {
    return;
  }

  // None of the new files exist. Try to copy any available old files.

  nsCOMPtr<nsIFile> cfm_key3;
  rv = cfmPath->Clone(getter_AddRefs(cfm_key3));
  if (NS_FAILED(rv)) {
    return;
  }

  cfm_key3->AppendNative(cstr_keydatabase3);
  rv = cfm_key3->Exists(&bExists);
  if (NS_FAILED(rv)) {
    return;
  }

  if (bExists) {
    cfm_key3->CopyToFollowingLinksNative(machoPath, cstr_key3db);
  }

  nsCOMPtr<nsIFile> cfm_cert7;
  rv = cfmPath->Clone(getter_AddRefs(cfm_cert7));
  if (NS_FAILED(rv)) {
    return;
  }

  cfm_cert7->AppendNative(cstr_certificate7);
  rv = cfm_cert7->Exists(&bExists);
  if (NS_FAILED(rv)) {
    return;
  }

  if (bExists) {
    cfm_cert7->CopyToFollowingLinksNative(machoPath, cstr_cert7db);
  }

  nsCOMPtr<nsIFile> cfm_cert8;
  rv = cfmPath->Clone(getter_AddRefs(cfm_cert8));
  if (NS_FAILED(rv)) {
    return;
  }

  cfm_cert8->AppendNative(cstr_certificate8);
  rv = cfm_cert8->Exists(&bExists);
  if (NS_FAILED(rv)) {
    return;
  }

  if (bExists) {
    cfm_cert8->CopyToFollowingLinksNative(machoPath, cstr_cert8db);
  }
}
#endif

static void configureMD5(bool enabled)
{
  if (enabled) { // set flags
    NSS_SetAlgorithmPolicy(SEC_OID_MD5, 
        NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE, 0);
    NSS_SetAlgorithmPolicy(SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
        NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE, 0);
    NSS_SetAlgorithmPolicy(SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC,
        NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE, 0);
  }
  else { // clear flags
    NSS_SetAlgorithmPolicy(SEC_OID_MD5,
        0, NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE);
    NSS_SetAlgorithmPolicy(SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
        0, NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE);
    NSS_SetAlgorithmPolicy(SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC,
        0, NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE);
  }
}

nsresult
nsNSSComponent::InitializeNSS(bool showWarningBox)
{
  // Can be called both during init and profile change.
  // Needs mutex protection.

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent::InitializeNSS\n"));

  MOZ_STATIC_ASSERT(nsINSSErrorsService::NSS_SEC_ERROR_BASE == SEC_ERROR_BASE &&
                    nsINSSErrorsService::NSS_SEC_ERROR_LIMIT == SEC_ERROR_LIMIT &&
                    nsINSSErrorsService::NSS_SSL_ERROR_BASE == SSL_ERROR_BASE &&
                    nsINSSErrorsService::NSS_SSL_ERROR_LIMIT == SSL_ERROR_LIMIT,
                    "You must update the values in nsINSSErrorsService.idl");

  // variables used for flow control within this function

  enum { problem_none, problem_no_rw, problem_no_security_at_all }
    which_nss_problem = problem_none;

  {
    MutexAutoLock lock(mutex);

    // Init phase 1, prepare own variables used for NSS

    if (mNSSInitialized) {
      PR_ASSERT(!"Trying to initialize NSS twice"); // We should never try to 
                                                    // initialize NSS more than
                                                    // once in a process.
      return NS_ERROR_FAILURE;
    }
    
    nsresult rv;
    nsCAutoString profileStr;
    nsCOMPtr<nsIFile> profilePath;

    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(profilePath));
    if (NS_FAILED(rv)) {
      PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("Unable to get profile directory\n"));
      ConfigureInternalPKCS11Token();
      SECStatus init_rv = NSS_NoDB_Init(NULL);
      if (init_rv != SECSuccess) {
        nsPSMInitPanic::SetPanic();
        return NS_ERROR_NOT_AVAILABLE;
      }
    }
    else
    {

  // XP_MAC == CFM
  // XP_MACOSX == MachO

  #if defined(XP_MAC) && defined(XP_MACOSX)
  #error "This code assumes XP_MAC and XP_MACOSX will never be defined at the same time"
  #endif

  #if defined(XP_MAC) || defined(XP_MACOSX)
    // On Mac CFM we place all NSS DBs in the Security
    // Folder in the profile directory.
    nsCOMPtr<nsIFile> cfmSecurityPath;
    cfmSecurityPath = profilePath; // alias for easier code reading
    cfmSecurityPath->AppendNative(NS_LITERAL_CSTRING("Security"));
  #endif

  #if defined(XP_MAC)
    // on CFM, cfmSecurityPath and profilePath point to the same oject
    profilePath->Create(nsIFile::DIRECTORY_TYPE, 0); //This is for Mac, don't worry about
                                                     //permissions.
  #elif defined(XP_MACOSX)
    // On MachO, we need to access both directories,
    // and therefore need separate nsIFile instances.
    // Keep cfmSecurityPath instance, obtain new instance for MachO profilePath.
    rv = cfmSecurityPath->GetParent(getter_AddRefs(profilePath));
    if (NS_FAILED(rv)) {
      nsPSMInitPanic::SetPanic();
      return rv;
    }
  #endif

    const char *dbdir_override = getenv("MOZPSM_NSSDBDIR_OVERRIDE");
    if (dbdir_override && strlen(dbdir_override)) {
      profileStr = dbdir_override;
    }
    else {
  #if defined(XP_WIN)
      // Native path will drop Unicode characters that cannot be mapped to system's
      // codepage, using short (canonical) path as workaround.
      nsCOMPtr<nsILocalFileWin> profilePathWin(do_QueryInterface(profilePath, &rv));
      if (profilePathWin)
        rv = profilePathWin->GetNativeCanonicalPath(profileStr);
  #else
      rv = profilePath->GetNativePath(profileStr);
  #endif
      if (NS_FAILED(rv)) {
        nsPSMInitPanic::SetPanic();
        return rv;
      }
    }

    hashTableCerts = PL_NewHashTable( 0, certHashtable_keyHash, certHashtable_keyCompare,
      certHashtable_valueCompare, 0, 0 );

  #if defined(XP_MACOSX)
    // function may modify the parameters
    // ignore return code from conversion, we continue anyway
    TryCFM2MachOMigration(cfmSecurityPath, profilePath);
  #endif

    rv = mPrefBranch->GetBoolPref("security.use_libpkix_verification", &globalConstFlagUsePKIXVerification);
    if (NS_FAILED(rv))
      globalConstFlagUsePKIXVerification = USE_NSS_LIBPKIX_DEFAULT;

    bool supress_warning_preference = false;
    rv = mPrefBranch->GetBoolPref("security.suppress_nss_rw_impossible_warning", &supress_warning_preference);

    if (NS_FAILED(rv)) {
      supress_warning_preference = false;
    }

    // init phase 2, init calls to NSS library

    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS Initialization beginning\n"));

    // The call to ConfigureInternalPKCS11Token needs to be done before NSS is initialized, 
    // but affects only static data.
    // If we could assume i18n will not change between profiles, one call per application
    // run were sufficient. As I can't predict what happens in the future, let's repeat
    // this call for every re-init of NSS.

    ConfigureInternalPKCS11Token();

    // The NSS_INIT_NOROOTINIT flag turns off the loading of the root certs
    // module by NSS_Initialize because we will load it in InstallLoadableRoots
    // later.  It also allows us to work around a bug in the system NSS in
    // Ubuntu 8.04, which loads any nonexistent "<configdir>/libnssckbi.so" as
    // "/usr/lib/nss/libnssckbi.so".
    PRUint32 init_flags = NSS_INIT_NOROOTINIT | NSS_INIT_OPTIMIZESPACE;
    SECStatus init_rv = ::NSS_Initialize(profileStr.get(), "", "",
                                         SECMOD_DB, init_flags);

    if (init_rv != SECSuccess) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("can not init NSS r/w in %s\n", profileStr.get()));

      if (supress_warning_preference) {
        which_nss_problem = problem_none;
      }
      else {
        which_nss_problem = problem_no_rw;
      }

      // try to init r/o
      init_flags |= NSS_INIT_READONLY;
      init_rv = ::NSS_Initialize(profileStr.get(), "", "",
                                 SECMOD_DB, init_flags);

      if (init_rv != SECSuccess) {
        PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("can not init in r/o either\n"));
        which_nss_problem = problem_no_security_at_all;

        init_rv = NSS_NoDB_Init(profileStr.get());
        if (init_rv != SECSuccess) {
          nsPSMInitPanic::SetPanic();
          return NS_ERROR_NOT_AVAILABLE;
        }
      }
    } // have profile dir
    } // lock

    // init phase 3, only if phase 2 was successful

    if (problem_no_security_at_all != which_nss_problem) {

      mNSSInitialized = true;

      ::NSS_SetDomesticPolicy();
      //  SSL_EnableCipher(SSL_RSA_WITH_NULL_MD5, SSL_ALLOWED);
      //  SSL_EnableCipher(SSL_RSA_WITH_NULL_SHA, SSL_ALLOWED);

      PK11_SetPasswordFunc(PK11PasswordPrompt);

      // Register an observer so we can inform NSS when these prefs change
      mPrefBranch->AddObserver("security.", this, false);

      SSL_OptionSetDefault(SSL_ENABLE_SSL2, false);
      SSL_OptionSetDefault(SSL_V2_COMPATIBLE_HELLO, false);
      bool enabled;
      mPrefBranch->GetBoolPref("security.enable_ssl3", &enabled);
      SSL_OptionSetDefault(SSL_ENABLE_SSL3, enabled);
      mPrefBranch->GetBoolPref("security.enable_tls", &enabled);
      SSL_OptionSetDefault(SSL_ENABLE_TLS, enabled);
      mPrefBranch->GetBoolPref("security.enable_md5_signatures", &enabled);
      configureMD5(enabled);

      // Configure TLS session tickets
      mPrefBranch->GetBoolPref("security.enable_tls_session_tickets", &enabled);
      SSL_OptionSetDefault(SSL_ENABLE_SESSION_TICKETS, enabled);

      mPrefBranch->GetBoolPref("security.ssl.require_safe_negotiation", &enabled);
      SSL_OptionSetDefault(SSL_REQUIRE_SAFE_NEGOTIATION, enabled);

      mPrefBranch->GetBoolPref(
        "security.ssl.allow_unrestricted_renego_everywhere__temporarily_available_pref", 
        &enabled);
      SSL_OptionSetDefault(SSL_ENABLE_RENEGOTIATION, 
        enabled ? SSL_RENEGOTIATE_UNRESTRICTED : SSL_RENEGOTIATE_REQUIRES_XTN);

#ifdef SSL_ENABLE_FALSE_START // Requires NSS 3.12.8
      mPrefBranch->GetBoolPref("security.ssl.enable_false_start", &enabled);
      SSL_OptionSetDefault(SSL_ENABLE_FALSE_START, enabled);
#endif

      // Disable any ciphers that NSS might have enabled by default
      for (PRUint16 i = 0; i < SSL_NumImplementedCiphers; ++i)
      {
        PRUint16 cipher_id = SSL_ImplementedCiphers[i];
        SSL_CipherPrefSetDefault(cipher_id, false);
      }

      // Now only set SSL/TLS ciphers we knew about at compile time
      for (CipherPref* cp = CipherPrefs; cp->pref; ++cp) {
        rv = mPrefBranch->GetBoolPref(cp->pref, &enabled);
        if (NS_FAILED(rv))
          enabled = false;

        SSL_CipherPrefSetDefault(cp->id, enabled);
      }

      // Enable ciphers for PKCS#12
      SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
      SEC_PKCS12EnableCipher(PKCS12_RC4_128, 1);
      SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
      SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 1);
      SEC_PKCS12EnableCipher(PKCS12_DES_56, 1);
      SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 1);
      SEC_PKCS12SetPreferredCipher(PKCS12_DES_EDE3_168, 1);
      PORT_SetUCS2_ASCIIConversionFunction(pip_ucs2_ascii_conversion_fn);

      // dynamic options from prefs
      setValidationOptions(mPrefBranch);

      // static validation options for usagesarray - do not hit the network
      mDefaultCERTValInParamLocalOnly = new nsCERTValInParamWrapper;
      rv = mDefaultCERTValInParamLocalOnly->Construct(
          nsCERTValInParamWrapper::missing_cert_download_off,
          nsCERTValInParamWrapper::crl_local_only,
          nsCERTValInParamWrapper::ocsp_off,
          nsCERTValInParamWrapper::ocsp_relaxed,
          nsCERTValInParamWrapper::any_revo_relaxed,
          FIRST_REVO_METHOD_DEFAULT);
      if (NS_FAILED(rv)) {
        nsPSMInitPanic::SetPanic();
        return rv;
      }
      
      RegisterMyOCSPAIAInfoCallback();

      mHttpForNSS.initTable();
      mHttpForNSS.registerHttpClient();

      InstallLoadableRoots();

      LaunchSmartCardThreads();

      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS Initialization done\n"));
    }
  }

  if (problem_none != which_nss_problem) {
    nsPSMInitPanic::SetPanic();

    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS problem, trying to bring up GUI error message\n"));

    // We might want to use different messages, depending on what failed.
    // For now, let's use the same message.
    if (showWarningBox) {
      ShowAlertFromStringBundle("NSSInitProblemX");
    }
  }

  return NS_OK;
}

nsresult
nsNSSComponent::ShutdownNSS()
{
  // Can be called both during init and profile change,
  // needs mutex protection.
  
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent::ShutdownNSS\n"));

  MutexAutoLock lock(mutex);
  nsresult rv = NS_OK;

  if (hashTableCerts) {
    PL_HashTableEnumerateEntries(hashTableCerts, certHashtable_clearEntry, 0);
    PL_HashTableDestroy(hashTableCerts);
    hashTableCerts = nsnull;
  }

  if (mNSSInitialized) {
    mNSSInitialized = false;

    PK11_SetPasswordFunc((PK11PasswordFunc)nsnull);
    mHttpForNSS.unregisterHttpClient();
    UnregisterMyOCSPAIAInfoCallback();

    if (mPrefBranch) {
      mPrefBranch->RemoveObserver("security.", this);
    }

    ShutdownSmartCardThreads();
    SSL_ClearSessionCache();
    if (mClientAuthRememberService) {
      mClientAuthRememberService->ClearRememberedDecisions();
    }
    UnloadLoadableRoots();
    CleanupIdentityInfo();
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("evaporating psm resources\n"));
    mShutdownObjectList->evaporateAllNSSResources();
    EnsureNSSInitialized(nssShutdown);
    if (SECSuccess != ::NSS_Shutdown()) {
      PR_LOG(gPIPNSSLog, PR_LOG_ALWAYS, ("NSS SHUTDOWN FAILURE\n"));
      rv = NS_ERROR_FAILURE;
    }
    else {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS shutdown =====>> OK <<=====\n"));
    }
  }

  return rv;
}
 
NS_IMETHODIMP
nsNSSComponent::Init()
{
  // No mutex protection.
  // Assume Init happens before any concurrency on "this" can start.

  nsresult rv = NS_OK;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Beginning NSS initialization\n"));

  if (!mShutdownObjectList)
  {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS init, out of memory in constructor\n"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = InitializePIPNSSBundle();
  if (NS_FAILED(rv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("Unable to create pipnss bundle.\n"));
    return rv;
  }      

  // Access our string bundles now, this prevents assertions from I/O
  // - nsStandardURL not thread-safe
  // - wrong thread: 'NS_IsMainThread()' in nsIOService.cpp
  // when loading error strings on the SSL threads.
  {
    NS_NAMED_LITERAL_STRING(dummy_name, "dummy");
    nsXPIDLString result;
    mPIPNSSBundle->GetStringFromName(dummy_name.get(),
                                     getter_Copies(result));
    mNSSErrorsBundle->GetStringFromName(dummy_name.get(),
                                        getter_Copies(result));
  }

  if (!mPrefBranch) {
    mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
    NS_ASSERTION(mPrefBranch, "Unable to get pref service");
  }

  // Do that before NSS init, to make sure we won't get unloaded.
  RegisterObservers();

  rv = InitializeNSS(true); // ok to show a warning box on failure
  if (NS_FAILED(rv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("Unable to Initialize NSS.\n"));

    DeregisterObservers();
    mPIPNSSBundle = nsnull;
    return rv;
  }

  RememberCertErrorsTable::Init();
  nsSSLIOLayerHelpers::Init();
  char *unrestricted_hosts=nsnull;
  mPrefBranch->GetCharPref("security.ssl.renego_unrestricted_hosts", &unrestricted_hosts);
  if (unrestricted_hosts) {
    nsSSLIOLayerHelpers::setRenegoUnrestrictedSites(nsDependentCString(unrestricted_hosts));
    nsMemory::Free(unrestricted_hosts);
    unrestricted_hosts=nsnull;
  }

  bool enabled = false;
  mPrefBranch->GetBoolPref("security.ssl.treat_unsafe_negotiation_as_broken", &enabled);
  nsSSLIOLayerHelpers::setTreatUnsafeNegotiationAsBroken(enabled);

  PRInt32 warnLevel = 1;
  mPrefBranch->GetIntPref("security.ssl.warn_missing_rfc5746", &warnLevel);
  nsSSLIOLayerHelpers::setWarnLevelMissingRFC5746(warnLevel);
  
  mClientAuthRememberService = new nsClientAuthRememberService;
  if (mClientAuthRememberService)
    mClientAuthRememberService->Init();

  createBackgroundThreads();
  if (!mCertVerificationThread)
  {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS init, could not create threads\n"));

    DeregisterObservers();
    mPIPNSSBundle = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  InitializeCRLUpdateTimer();
  RegisterPSMContentListener();

  nsCOMPtr<nsIEntropyCollector> ec
      = do_GetService(NS_ENTROPYCOLLECTOR_CONTRACTID);

  nsCOMPtr<nsIBufEntropyCollector> bec;

  if (ec) {
    bec = do_QueryInterface(ec);
  }

  NS_ASSERTION(bec, "No buffering entropy collector.  "
                    "This means no entropy will be collected.");
  if (bec) {
    bec->ForwardTo(this);
  }

  return rv;
}

/* nsISupports Implementation for the class */
NS_IMPL_THREADSAFE_ISUPPORTS6(nsNSSComponent,
                              nsISignatureVerifier,
                              nsIEntropyCollector,
                              nsINSSComponent,
                              nsIObserver,
                              nsISupportsWeakReference,
                              nsITimerCallback)


/* Callback functions for decoder. For now, use empty/default functions. */
static void ContentCallback(void *arg, 
                                           const char *buf,
                                           unsigned long len)
{
}

static PK11SymKey * GetDecryptKeyCallback(void *arg, 
                                                 SECAlgorithmID *algid)
{
  return nsnull;
}

static PRBool DecryptionAllowedCallback(SECAlgorithmID *algid,  
                                               PK11SymKey *bulkkey)
{
  return SECMIME_DecryptionAllowed(algid, bulkkey);
}

static void * GetPasswordKeyCallback(void *arg, void *handle)
{
  return NULL;
}

NS_IMETHODIMP
nsNSSComponent::VerifySignature(const char* aRSABuf, PRUint32 aRSABufLen,
                                const char* aPlaintext, PRUint32 aPlaintextLen,
                                PRInt32* aErrorCode,
                                nsIPrincipal** aPrincipal)
{
  if (!aPrincipal || !aErrorCode) {
    return NS_ERROR_NULL_POINTER;
  }

  *aErrorCode = 0;
  *aPrincipal = nsnull;

  nsNSSShutDownPreventionLock locker;
  SEC_PKCS7ContentInfo * p7_info = nsnull; 
  unsigned char hash[SHA1_LENGTH]; 

  SECItem item;
  item.type = siEncodedCertBuffer;
  item.data = (unsigned char*)aRSABuf;
  item.len = aRSABufLen;
  p7_info = SEC_PKCS7DecodeItem(&item,
                                ContentCallback, nsnull,
                                GetPasswordKeyCallback, nsnull,
                                GetDecryptKeyCallback, nsnull,
                                DecryptionAllowedCallback);

  if (!p7_info) {
    return NS_ERROR_FAILURE;
  }

  // Make sure we call SEC_PKCS7DestroyContentInfo after this point;
  // otherwise we leak data in p7_info
  
  //-- If a plaintext was provided, hash it.
  SECItem digest;
  digest.data = nsnull;
  digest.len = 0;

  if (aPlaintext) {
    HASHContext* hash_ctxt;
    PRUint32 hashLen = 0;

    hash_ctxt = HASH_Create(HASH_AlgSHA1);
    HASH_Begin(hash_ctxt);
    HASH_Update(hash_ctxt,(const unsigned char*)aPlaintext, aPlaintextLen);
    HASH_End(hash_ctxt, hash, &hashLen, SHA1_LENGTH); 
    HASH_Destroy(hash_ctxt);

    digest.data = hash;
    digest.len = SHA1_LENGTH;
  }

  //-- Verify signature
  bool rv = SEC_PKCS7VerifyDetachedSignature(p7_info, certUsageObjectSigner,
                                               &digest, HASH_AlgSHA1, false);
  if (!rv) {
    *aErrorCode = PR_GetError();
  }

  // Get the signing cert //
  CERTCertificate *cert = p7_info->content.signedData->signerInfos[0]->cert;
  nsresult rv2 = NS_OK;
  if (cert) {
    // Use |do { } while (0);| as a "more C++-ish" thing than goto;
    // this way we don't have to worry about goto across variable
    // declarations.  We have no loops in this code, so it's OK.
    do {
      nsCOMPtr<nsIX509Cert> pCert = nsNSSCertificate::Create(cert);
      if (!pCert) {
        rv2 = NS_ERROR_OUT_OF_MEMORY;
        break;
      }

      if (!mScriptSecurityManager) {
        MutexAutoLock lock(mutex);
        // re-test the condition to prevent double initialization
        if (!mScriptSecurityManager) {
          mScriptSecurityManager = 
            do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv2);
          if (NS_FAILED(rv2)) {
            break;
          }
        }
      }

      //-- Create a certificate principal with id and organization data
      nsAutoString fingerprint;
      rv2 = pCert->GetSha1Fingerprint(fingerprint);
      if (NS_FAILED(rv2)) {
        break;
      }
      nsAutoString orgName;
      rv2 = pCert->GetOrganization(orgName);
      if (NS_FAILED(rv2)) {
        break;
      }
      nsAutoString subjectName;
      rv2 = pCert->GetSubjectName(subjectName);
      if (NS_FAILED(rv2)) {
        break;
      }
    
      nsCOMPtr<nsIPrincipal> certPrincipal;
      rv2 = mScriptSecurityManager->
        GetCertificatePrincipal(NS_ConvertUTF16toUTF8(fingerprint),
                                NS_ConvertUTF16toUTF8(subjectName),
                                NS_ConvertUTF16toUTF8(orgName),
                                pCert, nsnull, getter_AddRefs(certPrincipal));
      if (NS_FAILED(rv2) || !certPrincipal) {
        break;
      }
      
      certPrincipal.swap(*aPrincipal);
    } while (0);
  }

  SEC_PKCS7DestroyContentInfo(p7_info);

  return rv2;
}

NS_IMETHODIMP
nsNSSComponent::RandomUpdate(void *entropy, PRInt32 bufLen)
{
  nsNSSShutDownPreventionLock locker;

  // Asynchronous event happening often,
  // must not interfere with initialization or profile switch.
  
  MutexAutoLock lock(mutex);

  if (!mNSSInitialized)
      return NS_ERROR_NOT_INITIALIZED;

  PK11_RandomUpdate(entropy, bufLen);
  return NS_OK;
}

#define PROFILE_CHANGE_NET_TEARDOWN_TOPIC "profile-change-net-teardown"
#define PROFILE_CHANGE_NET_RESTORE_TOPIC "profile-change-net-restore"
#define PROFILE_APPROVE_CHANGE_TOPIC "profile-approve-change"
#define PROFILE_CHANGE_TEARDOWN_TOPIC "profile-change-teardown"
#define PROFILE_CHANGE_TEARDOWN_VETO_TOPIC "profile-change-teardown-veto"
#define PROFILE_BEFORE_CHANGE_TOPIC "profile-before-change"
#define PROFILE_DO_CHANGE_TOPIC "profile-do-change"

NS_IMETHODIMP
nsNSSComponent::Observe(nsISupports *aSubject, const char *aTopic, 
                        const PRUnichar *someData)
{
  if (nsCRT::strcmp(aTopic, PROFILE_APPROVE_CHANGE_TOPIC) == 0) {
    DoProfileApproveChange(aSubject);
  }
  else if (nsCRT::strcmp(aTopic, PROFILE_CHANGE_TEARDOWN_TOPIC) == 0) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("in PSM code, receiving change-teardown\n"));
    DoProfileChangeTeardown(aSubject);
  }
  else if (nsCRT::strcmp(aTopic, PROFILE_CHANGE_TEARDOWN_VETO_TOPIC) == 0) {
    mShutdownObjectList->allowUI();
  }
  else if (nsCRT::strcmp(aTopic, PROFILE_BEFORE_CHANGE_TOPIC) == 0) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("receiving profile change topic\n"));
    DoProfileBeforeChange(aSubject);
  }
  else if (nsCRT::strcmp(aTopic, PROFILE_DO_CHANGE_TOPIC) == 0) {
    if (someData && NS_LITERAL_STRING("startup").Equals(someData)) {
      // The application is initializing against a known profile directory for
      // the first time during process execution.
      // However, earlier code execution might have already triggered NSS init.
      // We must ensure that NSS gets shut down prior to any attempt to init
      // it again. We use the same cleanup functionality used when switching
      // profiles. The order of function calls must correspond to the order
      // of notifications sent by Profile Manager (nsProfile).
      DoProfileApproveChange(aSubject);
      DoProfileChangeNetTeardown();
      DoProfileChangeTeardown(aSubject);
      DoProfileBeforeChange(aSubject);
      DoProfileChangeNetRestore();
    }
  
    bool needsInit = true;

    {
      MutexAutoLock lock(mutex);

      if (mNSSInitialized) {
        // We have already initialized NSS before the profile came up,
        // no need to do it again
        needsInit = false;
      }
    }
    
    if (needsInit) {
      if (NS_FAILED(InitializeNSS(false))) { // do not show a warning box on failure
        PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("Unable to Initialize NSS after profile switch.\n"));
        nsCOMPtr<nsIProfileChangeStatus> status = do_QueryInterface(aSubject);
        if (status) {
          status->ChangeFailed();
        }
      }
    }

    InitializeCRLUpdateTimer();
  }
  else if (nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {

    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent: XPCom shutdown observed\n"));

    // Cleanup code that requires services, it's too late in destructor.

    if (mPSMContentListener) {
      nsCOMPtr<nsIURILoader> dispatcher(do_GetService(NS_URI_LOADER_CONTRACTID));
      if (dispatcher) {
        dispatcher->UnRegisterContentListener(mPSMContentListener);
      }
      mPSMContentListener = nsnull;
    }

    nsCOMPtr<nsIEntropyCollector> ec
        = do_GetService(NS_ENTROPYCOLLECTOR_CONTRACTID);

    if (ec) {
      nsCOMPtr<nsIBufEntropyCollector> bec
        = do_QueryInterface(ec);
      if (bec) {
        bec->DontForward();
      }
    }
  }
  else if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) { 
    nsNSSShutDownPreventionLock locker;
    bool clearSessionCache = false;
    bool enabled;
    NS_ConvertUTF16toUTF8  prefName(someData);

    if (prefName.Equals("security.enable_ssl3")) {
      mPrefBranch->GetBoolPref("security.enable_ssl3", &enabled);
      SSL_OptionSetDefault(SSL_ENABLE_SSL3, enabled);
      clearSessionCache = true;
    } else if (prefName.Equals("security.enable_tls")) {
      mPrefBranch->GetBoolPref("security.enable_tls", &enabled);
      SSL_OptionSetDefault(SSL_ENABLE_TLS, enabled);
      clearSessionCache = true;
    } else if (prefName.Equals("security.enable_md5_signatures")) {
      mPrefBranch->GetBoolPref("security.enable_md5_signatures", &enabled);
      configureMD5(enabled);
      clearSessionCache = true;
    } else if (prefName.Equals("security.enable_tls_session_tickets")) {
      mPrefBranch->GetBoolPref("security.enable_tls_session_tickets", &enabled);
      SSL_OptionSetDefault(SSL_ENABLE_SESSION_TICKETS, enabled);
    } else if (prefName.Equals("security.ssl.require_safe_negotiation")) {
      mPrefBranch->GetBoolPref("security.ssl.require_safe_negotiation", &enabled);
      SSL_OptionSetDefault(SSL_REQUIRE_SAFE_NEGOTIATION, enabled);
    } else if (prefName.Equals("security.ssl.allow_unrestricted_renego_everywhere__temporarily_available_pref")) {
      mPrefBranch->GetBoolPref("security.ssl.allow_unrestricted_renego_everywhere__temporarily_available_pref", &enabled);
      SSL_OptionSetDefault(SSL_ENABLE_RENEGOTIATION, 
        enabled ? SSL_RENEGOTIATE_UNRESTRICTED : SSL_RENEGOTIATE_REQUIRES_XTN);
    } else if (prefName.Equals("security.ssl.renego_unrestricted_hosts")) {
      char *unrestricted_hosts=nsnull;
      mPrefBranch->GetCharPref("security.ssl.renego_unrestricted_hosts", &unrestricted_hosts);
      if (unrestricted_hosts) {
        nsSSLIOLayerHelpers::setRenegoUnrestrictedSites(nsDependentCString(unrestricted_hosts));
        nsMemory::Free(unrestricted_hosts);
      }
    } else if (prefName.Equals("security.ssl.treat_unsafe_negotiation_as_broken")) {
      mPrefBranch->GetBoolPref("security.ssl.treat_unsafe_negotiation_as_broken", &enabled);
      nsSSLIOLayerHelpers::setTreatUnsafeNegotiationAsBroken(enabled);
    } else if (prefName.Equals("security.ssl.warn_missing_rfc5746")) {
      PRInt32 warnLevel = 1;
      mPrefBranch->GetIntPref("security.ssl.warn_missing_rfc5746", &warnLevel);
      nsSSLIOLayerHelpers::setWarnLevelMissingRFC5746(warnLevel);
#ifdef SSL_ENABLE_FALSE_START // Requires NSS 3.12.8
    } else if (prefName.Equals("security.ssl.enable_false_start")) {
      mPrefBranch->GetBoolPref("security.ssl.enable_false_start", &enabled);
      SSL_OptionSetDefault(SSL_ENABLE_FALSE_START, enabled);
#endif
    } else if (prefName.Equals("security.OCSP.enabled")
               || prefName.Equals("security.CRL_download.enabled")
               || prefName.Equals("security.fresh_revocation_info.require")
               || prefName.Equals("security.missing_cert_download.enabled")
               || prefName.Equals("security.first_network_revocation_method")
               || prefName.Equals("security.OCSP.require")) {
      MutexAutoLock lock(mutex);
      setValidationOptions(mPrefBranch);
    } else {
      /* Look through the cipher table and set according to pref setting */
      for (CipherPref* cp = CipherPrefs; cp->pref; ++cp) {
        if (prefName.Equals(cp->pref)) {
          mPrefBranch->GetBoolPref(cp->pref, &enabled);
          SSL_CipherPrefSetDefault(cp->id, enabled);
          clearSessionCache = true;
          break;
        }
      }
    }
    if (clearSessionCache)
      SSL_ClearSessionCache();
  }
  else if (nsCRT::strcmp(aTopic, PROFILE_CHANGE_NET_TEARDOWN_TOPIC) == 0) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("receiving network teardown topic\n"));
    DoProfileChangeNetTeardown();
  }
  else if (nsCRT::strcmp(aTopic, PROFILE_CHANGE_NET_RESTORE_TOPIC) == 0) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("receiving network restore topic\n"));
    DoProfileChangeNetRestore();
  }

  return NS_OK;
}

/*static*/ nsresult
nsNSSComponent::GetNewPrompter(nsIPrompt ** result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = nsnull;

  if (!NS_IsMainThread()) {
    NS_ERROR("nsSDRContext::GetNewPrompter called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = wwatch->GetNewPrompter(0, result);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

/*static*/ nsresult
nsNSSComponent::ShowAlertWithConstructedString(const nsString & message)
{
  nsCOMPtr<nsIPrompt> prompter;
  nsresult rv = GetNewPrompter(getter_AddRefs(prompter));
  if (prompter) {
    nsPSMUITracker tracker;
    if (tracker.isUIForbidden()) {
      NS_WARNING("Suppressing alert because PSM UI is forbidden");
      rv = NS_ERROR_UNEXPECTED;
    } else {
      rv = prompter->Alert(nsnull, message.get());
    }
  }
  return rv;
}

NS_IMETHODIMP
nsNSSComponent::ShowAlertFromStringBundle(const char * messageID)
{
  nsString message;
  nsresult rv;

  rv = GetPIPNSSBundleString(messageID, message);
  if (NS_FAILED(rv)) {
    NS_ERROR("GetPIPNSSBundleString failed");
    return rv;
  }

  return ShowAlertWithConstructedString(message);
}

nsresult nsNSSComponent::LogoutAuthenticatedPK11()
{
  nsCOMPtr<nsICertOverrideService> icos =
    do_GetService("@mozilla.org/security/certoverride;1");
  if (icos) {
    icos->ClearValidityOverride(
            NS_LITERAL_CSTRING("all:temporary-certificates"),
            0);
  }

  if (mClientAuthRememberService) {
    mClientAuthRememberService->ClearRememberedDecisions();
  }

  return mShutdownObjectList->doPK11Logout();
}

nsresult
nsNSSComponent::RegisterObservers()
{
  // Happens once during init only, no mutex protection.

  nsCOMPtr<nsIObserverService> observerService(do_GetService("@mozilla.org/observer-service;1"));
  NS_ASSERTION(observerService, "could not get observer service");
  if (observerService) {
    mObserversRegistered = true;
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent: adding observers\n"));

    // We are a service.
    // Once we are loaded, don't allow being removed from memory.
    // This makes sense, as initializing NSS is expensive.

    // By using false for parameter ownsWeak in AddObserver,
    // we make sure that we won't get unloaded until the application shuts down.

    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

    observerService->AddObserver(this, PROFILE_APPROVE_CHANGE_TOPIC, false);
    observerService->AddObserver(this, PROFILE_CHANGE_TEARDOWN_TOPIC, false);
    observerService->AddObserver(this, PROFILE_CHANGE_TEARDOWN_VETO_TOPIC, false);
    observerService->AddObserver(this, PROFILE_BEFORE_CHANGE_TOPIC, false);
    observerService->AddObserver(this, PROFILE_DO_CHANGE_TOPIC, false);
    observerService->AddObserver(this, PROFILE_CHANGE_NET_TEARDOWN_TOPIC, false);
    observerService->AddObserver(this, PROFILE_CHANGE_NET_RESTORE_TOPIC, false);
  }
  return NS_OK;
}

nsresult
nsNSSComponent::DeregisterObservers()
{
  if (!mObserversRegistered)
    return NS_OK;

  nsCOMPtr<nsIObserverService> observerService(do_GetService("@mozilla.org/observer-service;1"));
  NS_ASSERTION(observerService, "could not get observer service");
  if (observerService) {
    mObserversRegistered = false;
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent: removing observers\n"));

    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

    observerService->RemoveObserver(this, PROFILE_APPROVE_CHANGE_TOPIC);
    observerService->RemoveObserver(this, PROFILE_CHANGE_TEARDOWN_TOPIC);
    observerService->RemoveObserver(this, PROFILE_CHANGE_TEARDOWN_VETO_TOPIC);
    observerService->RemoveObserver(this, PROFILE_BEFORE_CHANGE_TOPIC);
    observerService->RemoveObserver(this, PROFILE_DO_CHANGE_TOPIC);
    observerService->RemoveObserver(this, PROFILE_CHANGE_NET_TEARDOWN_TOPIC);
    observerService->RemoveObserver(this, PROFILE_CHANGE_NET_RESTORE_TOPIC);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::RememberCert(CERTCertificate *cert)
{
  nsNSSShutDownPreventionLock locker;

  // Must not interfere with init / shutdown / profile switch.

  MutexAutoLock lock(mutex);

  if (!hashTableCerts || !cert)
    return NS_OK;
  
  void *found = PL_HashTableLookup(hashTableCerts, (void*)&cert->certKey);
  
  if (found) {
    // we remember that cert already
    return NS_OK;
  }
  
  CERTCertificate *myDupCert = CERT_DupCertificate(cert);
  
  if (!myDupCert)
    return NS_ERROR_OUT_OF_MEMORY;
  
  if (!PL_HashTableAdd(hashTableCerts, (void*)&myDupCert->certKey, myDupCert)) {
    CERT_DestroyCertificate(myDupCert);
  }
  
  return NS_OK;
}

static const char PROFILE_SWITCH_CRYPTO_UI_ACTIVE[] =
                        "ProfileSwitchCryptoUIActive";
static const char PROFILE_SWITCH_SOCKETS_STILL_ACTIVE[] =
                        "ProfileSwitchSocketsStillActive";

void
nsNSSComponent::DoProfileApproveChange(nsISupports* aSubject)
{
  if (mShutdownObjectList->isUIActive()) {
    ShowAlertFromStringBundle(PROFILE_SWITCH_CRYPTO_UI_ACTIVE);
    nsCOMPtr<nsIProfileChangeStatus> status = do_QueryInterface(aSubject);
    if (status) {
      status->VetoChange();
    }
  }
}

void
nsNSSComponent::DoProfileChangeNetTeardown()
{
  if (mCertVerificationThread)
    mCertVerificationThread->requestExit();
  mIsNetworkDown = true;
}

void
nsNSSComponent::DoProfileChangeTeardown(nsISupports* aSubject)
{
  bool callVeto = false;

  if (!mShutdownObjectList->ifPossibleDisallowUI()) {
    callVeto = true;
    ShowAlertFromStringBundle(PROFILE_SWITCH_CRYPTO_UI_ACTIVE);
  }
  else if (mShutdownObjectList->areSSLSocketsActive()) {
    callVeto = true;
    ShowAlertFromStringBundle(PROFILE_SWITCH_SOCKETS_STILL_ACTIVE);
  }

  if (callVeto) {
    nsCOMPtr<nsIProfileChangeStatus> status = do_QueryInterface(aSubject);
    if (status) {
      status->VetoChange();
    }
  }
}

void
nsNSSComponent::DoProfileBeforeChange(nsISupports* aSubject)
{
  NS_ASSERTION(mIsNetworkDown, "nsNSSComponent relies on profile manager to wait for synchronous shutdown of all network activity");

  bool needsCleanup = true;

  {
    MutexAutoLock lock(mutex);

    if (!mNSSInitialized) {
      // Make sure we don't try to cleanup if we have already done so.
      // This makes sure we behave safely, in case we are notified
      // multiple times.
      needsCleanup = false;
    }
  }
    
  StopCRLUpdateTimer();

  if (needsCleanup) {
    if (NS_FAILED(ShutdownNSS())) {
      nsCOMPtr<nsIProfileChangeStatus> status = do_QueryInterface(aSubject);
      if (status) {
        status->ChangeFailed();
      }
    }
  }
  mShutdownObjectList->allowUI();
}

void
nsNSSComponent::DoProfileChangeNetRestore()
{
  /* XXX this doesn't work well, since nothing expects null pointers */
  deleteBackgroundThreads();
  createBackgroundThreads();
  mIsNetworkDown = false;
}

NS_IMETHODIMP
nsNSSComponent::GetClientAuthRememberService(nsClientAuthRememberService **cars)
{
  NS_ENSURE_ARG_POINTER(cars);
  NS_IF_ADDREF(*cars = mClientAuthRememberService);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::IsNSSInitialized(bool *initialized)
{
  MutexAutoLock lock(mutex);
  *initialized = mNSSInitialized;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::GetDefaultCERTValInParam(nsRefPtr<nsCERTValInParamWrapper> &out)
{
  MutexAutoLock lock(mutex);
  if (!mNSSInitialized)
      return NS_ERROR_NOT_INITIALIZED;
  out = mDefaultCERTValInParam;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::GetDefaultCERTValInParamLocalOnly(nsRefPtr<nsCERTValInParamWrapper> &out)
{
  MutexAutoLock lock(mutex);
  if (!mNSSInitialized)
      return NS_ERROR_NOT_INITIALIZED;
  out = mDefaultCERTValInParamLocalOnly;
  return NS_OK;
}

//---------------------------------------------
// Implementing nsICryptoHash
//---------------------------------------------

nsCryptoHash::nsCryptoHash()
  : mHashContext(nsnull)
  , mInitialized(false)
{
}

nsCryptoHash::~nsCryptoHash()
{
  nsNSSShutDownPreventionLock locker;

  if (isAlreadyShutDown())
    return;

  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsCryptoHash::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsCryptoHash::destructorSafeDestroyNSSReference()
{
  if (isAlreadyShutDown())
    return;

  if (mHashContext)
    HASH_Destroy(mHashContext);
  mHashContext = nsnull;
}

NS_IMPL_ISUPPORTS1(nsCryptoHash, nsICryptoHash)

NS_IMETHODIMP 
nsCryptoHash::Init(PRUint32 algorithm)
{
  nsNSSShutDownPreventionLock locker;

  HASH_HashType hashType = (HASH_HashType)algorithm;
  if (mHashContext)
  {
    if ((!mInitialized) && (HASH_GetType(mHashContext) == hashType))
    {
      mInitialized = true;
      HASH_Begin(mHashContext);
      return NS_OK;
    }

    // Destroy current hash context if the type was different
    // or Finish method wasn't called.
    HASH_Destroy(mHashContext);
    mInitialized = false;
  }

  mHashContext = HASH_Create(hashType);
  if (!mHashContext)
    return NS_ERROR_INVALID_ARG;

  HASH_Begin(mHashContext);
  mInitialized = true;
  return NS_OK; 
}

NS_IMETHODIMP
nsCryptoHash::InitWithString(const nsACString & aAlgorithm)
{
  if (aAlgorithm.LowerCaseEqualsLiteral("md2"))
    return Init(nsICryptoHash::MD2);

  if (aAlgorithm.LowerCaseEqualsLiteral("md5"))
    return Init(nsICryptoHash::MD5);

  if (aAlgorithm.LowerCaseEqualsLiteral("sha1"))
    return Init(nsICryptoHash::SHA1);

  if (aAlgorithm.LowerCaseEqualsLiteral("sha256"))
    return Init(nsICryptoHash::SHA256);

  if (aAlgorithm.LowerCaseEqualsLiteral("sha384"))
    return Init(nsICryptoHash::SHA384);

  if (aAlgorithm.LowerCaseEqualsLiteral("sha512"))
    return Init(nsICryptoHash::SHA512);

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsCryptoHash::Update(const PRUint8 *data, PRUint32 len)
{
  nsNSSShutDownPreventionLock locker;
  
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  HASH_Update(mHashContext, data, len);
  return NS_OK; 
}

NS_IMETHODIMP
nsCryptoHash::UpdateFromStream(nsIInputStream *data, PRUint32 len)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  if (!data)
    return NS_ERROR_INVALID_ARG;

  PRUint32 n;
  nsresult rv = data->Available(&n);
  if (NS_FAILED(rv))
    return rv;

  // if the user has passed PR_UINT32_MAX, then read
  // everything in the stream

  if (len == PR_UINT32_MAX)
    len = n;

  // So, if the stream has NO data available for the hash,
  // or if the data available is less then what the caller
  // requested, we can not fulfill the hash update.  In this
  // case, just return NS_ERROR_NOT_AVAILABLE indicating
  // that there is not enough data in the stream to satisify
  // the request.

  if (n == 0 || n < len)
    return NS_ERROR_NOT_AVAILABLE;
  
  char buffer[NS_CRYPTO_HASH_BUFFER_SIZE];
  PRUint32 read, readLimit;
  
  while(NS_SUCCEEDED(rv) && len>0)
  {
    readLimit = NS_MIN(PRUint32(NS_CRYPTO_HASH_BUFFER_SIZE), len);
    
    rv = data->Read(buffer, readLimit, &read);
    
    if (NS_SUCCEEDED(rv))
      rv = Update((const PRUint8*)buffer, read);
    
    len -= read;
  }
  
  return rv;
}

NS_IMETHODIMP
nsCryptoHash::Finish(bool ascii, nsACString & _retval)
{
  nsNSSShutDownPreventionLock locker;
  
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;
  
  PRUint32 hashLen = 0;
  unsigned char buffer[HASH_LENGTH_MAX];
  unsigned char* pbuffer = buffer;

  HASH_End(mHashContext, pbuffer, &hashLen, HASH_LENGTH_MAX);

  mInitialized = false;

  if (ascii)
  {
    char *asciiData = BTOA_DataToAscii(buffer, hashLen);
    NS_ENSURE_TRUE(asciiData, NS_ERROR_OUT_OF_MEMORY);

    _retval.Assign(asciiData);
    PORT_Free(asciiData);
  }
  else
  {
    _retval.Assign((const char*)buffer, hashLen);
  }

  return NS_OK;
}

//---------------------------------------------
// Implementing nsICryptoHMAC
//---------------------------------------------

NS_IMPL_ISUPPORTS1(nsCryptoHMAC, nsICryptoHMAC)

nsCryptoHMAC::nsCryptoHMAC()
{
  mHMACContext = nsnull;
}

nsCryptoHMAC::~nsCryptoHMAC()
{
  nsNSSShutDownPreventionLock locker;

  if (isAlreadyShutDown())
    return;

  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsCryptoHMAC::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsCryptoHMAC::destructorSafeDestroyNSSReference()
{
  if (isAlreadyShutDown())
    return;

  if (mHMACContext)
    PK11_DestroyContext(mHMACContext, true);
  mHMACContext = nsnull;
}

/* void init (in unsigned long aAlgorithm, in nsIKeyObject aKeyObject); */
NS_IMETHODIMP nsCryptoHMAC::Init(PRUint32 aAlgorithm, nsIKeyObject *aKeyObject)
{
  nsNSSShutDownPreventionLock locker;

  if (mHMACContext)
  {
    PK11_DestroyContext(mHMACContext, true);
    mHMACContext = nsnull;
  }

  CK_MECHANISM_TYPE HMACMechType;
  switch (aAlgorithm)
  {
  case nsCryptoHMAC::MD2:
    HMACMechType = CKM_MD2_HMAC; break;
  case nsCryptoHMAC::MD5:
    HMACMechType = CKM_MD5_HMAC; break;
  case nsCryptoHMAC::SHA1:
    HMACMechType = CKM_SHA_1_HMAC; break;
  case nsCryptoHMAC::SHA256:
    HMACMechType = CKM_SHA256_HMAC; break;
  case nsCryptoHMAC::SHA384:
    HMACMechType = CKM_SHA384_HMAC; break;
  case nsCryptoHMAC::SHA512:
    HMACMechType = CKM_SHA512_HMAC; break;
  default:
    return NS_ERROR_INVALID_ARG;
  }

  NS_ENSURE_ARG_POINTER(aKeyObject);

  nsresult rv;

  PRInt16 keyType;
  rv = aKeyObject->GetType(&keyType);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(keyType == nsIKeyObject::SYM_KEY, NS_ERROR_INVALID_ARG);

  PK11SymKey* key;
  // GetKeyObj doesn't addref the key
  rv = aKeyObject->GetKeyObj((void**)&key);
  NS_ENSURE_SUCCESS(rv, rv);

  SECItem rawData;
  rawData.data = 0;
  rawData.len = 0;
  mHMACContext = PK11_CreateContextBySymKey(
      HMACMechType, CKA_SIGN, key, &rawData);
  NS_ENSURE_TRUE(mHMACContext, NS_ERROR_FAILURE);

  SECStatus ss = PK11_DigestBegin(mHMACContext);
  NS_ENSURE_TRUE(ss == SECSuccess, NS_ERROR_FAILURE);

  return NS_OK;
}

/* void update ([array, size_is (aLen), const] in octet aData, in unsigned long aLen); */
NS_IMETHODIMP nsCryptoHMAC::Update(const PRUint8 *aData, PRUint32 aLen)
{
  nsNSSShutDownPreventionLock locker;

  if (!mHMACContext)
    return NS_ERROR_NOT_INITIALIZED;

  if (!aData)
    return NS_ERROR_INVALID_ARG;

  SECStatus ss = PK11_DigestOp(mHMACContext, aData, aLen);
  NS_ENSURE_TRUE(ss == SECSuccess, NS_ERROR_FAILURE);
  
  return NS_OK;
}

/* void updateFromStream (in nsIInputStream aStream, in unsigned long aLen); */
NS_IMETHODIMP nsCryptoHMAC::UpdateFromStream(nsIInputStream *aStream, PRUint32 aLen)
{
  if (!mHMACContext)
    return NS_ERROR_NOT_INITIALIZED;

  if (!aStream)
    return NS_ERROR_INVALID_ARG;

  PRUint32 n;
  nsresult rv = aStream->Available(&n);
  if (NS_FAILED(rv))
    return rv;

  // if the user has passed PR_UINT32_MAX, then read
  // everything in the stream

  if (aLen == PR_UINT32_MAX)
    aLen = n;

  // So, if the stream has NO data available for the hash,
  // or if the data available is less then what the caller
  // requested, we can not fulfill the HMAC update.  In this
  // case, just return NS_ERROR_NOT_AVAILABLE indicating
  // that there is not enough data in the stream to satisify
  // the request.

  if (n == 0 || n < aLen)
    return NS_ERROR_NOT_AVAILABLE;
  
  char buffer[NS_CRYPTO_HASH_BUFFER_SIZE];
  PRUint32 read, readLimit;
  
  while(NS_SUCCEEDED(rv) && aLen > 0)
  {
    readLimit = NS_MIN(PRUint32(NS_CRYPTO_HASH_BUFFER_SIZE), aLen);
    
    rv = aStream->Read(buffer, readLimit, &read);
    if (read == 0)
      return NS_BASE_STREAM_CLOSED;
    
    if (NS_SUCCEEDED(rv))
      rv = Update((const PRUint8*)buffer, read);
    
    aLen -= read;
  }
  
  return rv;
}

/* ACString finish (in bool aASCII); */
NS_IMETHODIMP nsCryptoHMAC::Finish(bool aASCII, nsACString & _retval)
{
  nsNSSShutDownPreventionLock locker;

  if (!mHMACContext)
    return NS_ERROR_NOT_INITIALIZED;
  
  PRUint32 hashLen = 0;
  unsigned char buffer[HASH_LENGTH_MAX];
  unsigned char* pbuffer = buffer;

  PK11_DigestFinal(mHMACContext, pbuffer, &hashLen, HASH_LENGTH_MAX);
  if (aASCII)
  {
    char *asciiData = BTOA_DataToAscii(buffer, hashLen);
    NS_ENSURE_TRUE(asciiData, NS_ERROR_OUT_OF_MEMORY);

    _retval.Assign(asciiData);
    PORT_Free(asciiData);
  }
  else
  {
    _retval.Assign((const char*)buffer, hashLen);
  }

  return NS_OK;
}

/* void reset (); */
NS_IMETHODIMP nsCryptoHMAC::Reset()
{
  nsNSSShutDownPreventionLock locker;

  SECStatus ss = PK11_DigestBegin(mHMACContext);
  NS_ENSURE_TRUE(ss == SECSuccess, NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(PipUIContext, nsIInterfaceRequestor)

PipUIContext::PipUIContext()
{
}

PipUIContext::~PipUIContext()
{
}

/* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP PipUIContext::GetInterface(const nsIID & uuid, void * *result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = nsnull;

  if (!NS_IsMainThread()) {
    NS_ERROR("PipUIContext::GetInterface called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (!uuid.Equals(NS_GET_IID(nsIPrompt)))
    return NS_ERROR_NO_INTERFACE;

  nsIPrompt * prompt = nsnull;
  nsresult rv = nsNSSComponent::GetNewPrompter(&prompt);
  *result = prompt;
  return rv;
}

nsresult 
getNSSDialogs(void **_result, REFNSIID aIID, const char *contract)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("getNSSDialogs called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsresult rv;

  nsCOMPtr<nsISupports> svc = do_GetService(contract, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  rv = svc->QueryInterface(aIID, _result);

  return rv;
}

nsresult
setPassword(PK11SlotInfo *slot, nsIInterfaceRequestor *ctx)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv = NS_OK;
  
  if (PK11_NeedUserInit(slot)) {
    nsITokenPasswordDialogs *dialogs;
    bool canceled;
    NS_ConvertUTF8toUTF16 tokenName(PK11_GetTokenName(slot));

    rv = getNSSDialogs((void**)&dialogs,
                       NS_GET_IID(nsITokenPasswordDialogs),
                       NS_TOKENPASSWORDSDIALOG_CONTRACTID);

    if (NS_FAILED(rv)) goto loser;

    {
      nsPSMUITracker tracker;
      if (tracker.isUIForbidden()) {
        rv = NS_ERROR_NOT_AVAILABLE;
      }
      else {
        rv = dialogs->SetPassword(ctx,
                                  tokenName.get(),
                                  &canceled);
      }
    }
    NS_RELEASE(dialogs);
    if (NS_FAILED(rv)) goto loser;

    if (canceled) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }
  }
 loser:
  return rv;
}


PSMContentDownloader::PSMContentDownloader(PRUint32 type)
  : mByteData(nsnull),
    mType(type),
    mDoSilentDownload(false)
{
}

PSMContentDownloader::~PSMContentDownloader()
{
  if (mByteData)
    nsMemory::Free(mByteData);
}

NS_IMPL_ISUPPORTS2(PSMContentDownloader, nsIStreamListener, nsIRequestObserver)

const PRInt32 kDefaultCertAllocLength = 2048;

NS_IMETHODIMP
PSMContentDownloader::OnStartRequest(nsIRequest* request, nsISupports* context)
{
  nsresult rv;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CertDownloader::OnStartRequest\n"));
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
  if (!channel) return NS_ERROR_FAILURE;

  // Get the URI //
  channel->GetURI(getter_AddRefs(mURI));

  PRInt32 contentLength;
  rv = channel->GetContentLength(&contentLength);
  if (NS_FAILED(rv) || contentLength <= 0)
    contentLength = kDefaultCertAllocLength;
  
  mBufferOffset = 0;
  mBufferSize = 0;
  mByteData = (char*) nsMemory::Alloc(contentLength);
  if (!mByteData)
    return NS_ERROR_OUT_OF_MEMORY;
  
  mBufferSize = contentLength;
  return NS_OK;
}

NS_IMETHODIMP
PSMContentDownloader::OnDataAvailable(nsIRequest* request,
                                nsISupports* context,
                                nsIInputStream *aIStream,
                                PRUint32 aSourceOffset,
                                PRUint32 aLength)
{
  if (!mByteData)
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRUint32 amt;
  nsresult err;
  //Do a check to see if we need to allocate more memory.
  if ((mBufferOffset + (PRInt32)aLength) > mBufferSize) {
      size_t newSize = (mBufferOffset + aLength) *2; // grow some more than needed
      char *newBuffer;
      newBuffer = (char*)nsMemory::Realloc(mByteData, newSize);
      if (newBuffer == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mByteData = newBuffer;
      mBufferSize = newSize;
  }
  
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CertDownloader::OnDataAvailable\n"));
  do {
    err = aIStream->Read(mByteData+mBufferOffset,
                         aLength, &amt);
    if (NS_FAILED(err)) return err;
    if (amt == 0) break;
    
    aLength -= amt;
    mBufferOffset += amt;
    
  } while (aLength > 0);
  
  return NS_OK;
}

NS_IMETHODIMP
PSMContentDownloader::OnStopRequest(nsIRequest* request,
                              nsISupports* context,
                              nsresult aStatus)
{
  nsNSSShutDownPreventionLock locker;
  //Check if the download succeeded - it might have failed due to
  //network issues, etc.
  if (NS_FAILED(aStatus)){
    handleContentDownloadError(aStatus);
    return aStatus;
  }

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CertDownloader::OnStopRequest\n"));

  nsCOMPtr<nsIX509CertDB> certdb;
  nsCOMPtr<nsICRLManager> crlManager;

  nsresult rv;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();

  switch (mType) {
  case PSMContentDownloader::X509_CA_CERT:
  case PSMContentDownloader::X509_USER_CERT:
  case PSMContentDownloader::X509_EMAIL_CERT:
    certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
    break;

  case PSMContentDownloader::PKCS7_CRL:
    crlManager = do_GetService(NS_CRLMANAGER_CONTRACTID);

  default:
    break;
  }

  switch (mType) {
  case PSMContentDownloader::X509_CA_CERT:
    return certdb->ImportCertificates((PRUint8*)mByteData, mBufferOffset, mType, ctx); 
  case PSMContentDownloader::X509_USER_CERT:
    return certdb->ImportUserCertificate((PRUint8*)mByteData, mBufferOffset, ctx);
  case PSMContentDownloader::X509_EMAIL_CERT:
    return certdb->ImportEmailCertificate((PRUint8*)mByteData, mBufferOffset, ctx); 
  case PSMContentDownloader::PKCS7_CRL:
    return crlManager->ImportCrl((PRUint8*)mByteData, mBufferOffset, mURI, SEC_CRL_TYPE, mDoSilentDownload, mCrlAutoDownloadKey.get());
  default:
    rv = NS_ERROR_FAILURE;
    break;
  }
  
  return rv;
}


nsresult
PSMContentDownloader::handleContentDownloadError(nsresult errCode)
{
  nsString tmpMessage;
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if(NS_FAILED(rv)){
    return rv;
  }
      
  //Handling errors for crl download only, for now.
  switch (mType){
  case PSMContentDownloader::PKCS7_CRL:

    //TO DO: Handle network errors in details
    //XXXXXXXXXXXXXXXXXX
    nssComponent->GetPIPNSSBundleString("CrlImportFailureNetworkProblem", tmpMessage);
      
    if (mDoSilentDownload) {
      //This is the case for automatic download. Update failure history
      nsCAutoString updateErrCntPrefStr(CRL_AUTOUPDATE_ERRCNT_PREF);
      nsCAutoString updateErrDetailPrefStr(CRL_AUTOUPDATE_ERRDETAIL_PREF);
      nsCString errMsg;
      PRInt32 errCnt;

      nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID,&rv);
      if(NS_FAILED(rv)){
        return rv;
      }
      
      LossyAppendUTF16toASCII(mCrlAutoDownloadKey, updateErrCntPrefStr);
      LossyAppendUTF16toASCII(mCrlAutoDownloadKey, updateErrDetailPrefStr);
      errMsg.AssignWithConversion(tmpMessage.get());
      
      rv = pref->GetIntPref(updateErrCntPrefStr.get(),&errCnt);
      if( (NS_FAILED(rv)) || (errCnt == 0) ){
        pref->SetIntPref(updateErrCntPrefStr.get(),1);
      }else{
        pref->SetIntPref(updateErrCntPrefStr.get(),errCnt+1);
      }
      pref->SetCharPref(updateErrDetailPrefStr.get(),errMsg.get());
      nsCOMPtr<nsIPrefService> prefSvc(do_QueryInterface(pref));
      prefSvc->SavePrefFile(nsnull);
    }else{
      nsString message;
      nssComponent->GetPIPNSSBundleString("CrlImportFailure1x", message);
      message.Append(NS_LITERAL_STRING("\n").get());
      message.Append(tmpMessage);
      nssComponent->GetPIPNSSBundleString("CrlImportFailure2", tmpMessage);
      message.Append(NS_LITERAL_STRING("\n").get());
      message.Append(tmpMessage);
      nsNSSComponent::ShowAlertWithConstructedString(message);
    }
    break;
  default:
    break;
  }

  return NS_OK;

}

void 
PSMContentDownloader::setSilentDownload(bool flag)
{
  mDoSilentDownload = flag;
}

void
PSMContentDownloader::setCrlAutodownloadKey(nsAutoString key)
{
  mCrlAutoDownloadKey = key;
}


/* other mime types that we should handle sometime:
   
   application/x-pkcs7-crl
   application/x-pkcs7-mime
   application/pkcs7-signature
   application/pre-encrypted
   
*/

PRUint32
getPSMContentType(const char * aContentType)
{ 
  // Don't forget to update RegisterPSMContentListeners in nsNSSModule.cpp 
  // for every supported content type.
  
  if (!nsCRT::strcasecmp(aContentType, "application/x-x509-ca-cert"))
    return PSMContentDownloader::X509_CA_CERT;
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-server-cert"))
    return PSMContentDownloader::X509_SERVER_CERT;
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-user-cert"))
    return PSMContentDownloader::X509_USER_CERT;
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-email-cert"))
    return PSMContentDownloader::X509_EMAIL_CERT;
  else if (!nsCRT::strcasecmp(aContentType, "application/x-pkcs7-crl"))
    return PSMContentDownloader::PKCS7_CRL;
  else if (!nsCRT::strcasecmp(aContentType, "application/x-x509-crl"))
    return PSMContentDownloader::PKCS7_CRL;
  else if (!nsCRT::strcasecmp(aContentType, "application/pkix-crl"))
    return PSMContentDownloader::PKCS7_CRL;
  return PSMContentDownloader::UNKNOWN_TYPE;
}


NS_IMPL_ISUPPORTS2(PSMContentListener,
                   nsIURIContentListener,
                   nsISupportsWeakReference) 

PSMContentListener::PSMContentListener()
{
  mLoadCookie = nsnull;
  mParentContentListener = nsnull;
}

PSMContentListener::~PSMContentListener()
{
}

nsresult
PSMContentListener::init()
{
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::OnStartURIOpen(nsIURI *aURI, bool *aAbortOpen)
{
  //if we don't want to handle the URI, return true in
  //*aAbortOpen
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::IsPreferred(const char * aContentType,
                                 char ** aDesiredContentType,
                                 bool * aCanHandleContent)
{
  return CanHandleContent(aContentType, true,
                          aDesiredContentType, aCanHandleContent);
}

NS_IMETHODIMP
PSMContentListener::CanHandleContent(const char * aContentType,
                                      bool aIsContentPreferred,
                                      char ** aDesiredContentType,
                                      bool * aCanHandleContent)
{
  PRUint32 type;
  type = getPSMContentType(aContentType);
  if (type == PSMContentDownloader::UNKNOWN_TYPE) {
    *aCanHandleContent = false;
  } else {
    *aCanHandleContent = true;
  }
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::DoContent(const char * aContentType,
                               bool aIsContentPreferred,
                               nsIRequest * aRequest,
                               nsIStreamListener ** aContentHandler,
                               bool * aAbortProcess)
{
  PSMContentDownloader *downLoader;
  PRUint32 type;
  type = getPSMContentType(aContentType);
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("PSMContentListener::DoContent\n"));
  if (type != PSMContentDownloader::UNKNOWN_TYPE) {
    downLoader = new PSMContentDownloader(type);
    if (downLoader) {
      downLoader->QueryInterface(NS_GET_IID(nsIStreamListener), 
                                            (void **)aContentHandler);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PSMContentListener::GetLoadCookie(nsISupports * *aLoadCookie)
{
  *aLoadCookie = mLoadCookie;
  NS_IF_ADDREF(*aLoadCookie);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::SetLoadCookie(nsISupports * aLoadCookie)
{
  mLoadCookie = aLoadCookie;
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::GetParentContentListener(nsIURIContentListener ** aContentListener)
{
  *aContentListener = mParentContentListener;
  NS_IF_ADDREF(*aContentListener);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::SetParentContentListener(nsIURIContentListener * aContentListener)
{
  mParentContentListener = aContentListener;
  return NS_OK;
}
