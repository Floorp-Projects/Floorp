/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1
#endif

#include "nsNSSComponent.h"

#include "CertVerifier.h"
#include "nsCertVerificationThread.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICertOverrideService.h"
#include "mozilla/Preferences.h"

#ifndef MOZ_DISABLE_CRYPTOLEGACY
#include "nsIDOMNode.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDocument.h"
#include "nsIDOMSmartCardEvent.h"
#include "nsSmartCardMonitor.h"
#include "nsIDOMCryptoLegacy.h"
#include "nsIPrincipal.h"
#else
#include "nsIDOMCrypto.h"
#endif

#include "nsCRT.h"
#include "nsNTLMAuthModule.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsCertificatePrincipal.h"
#include "nsIBufEntropyCollector.h"
#include "nsITokenPasswordDialogs.h"
#include "nsServiceManagerUtils.h"
#include "nsNSSShutDown.h"
#include "GeneratedEvents.h"
#include "SharedSSLState.h"

#include "nss.h"
#include "ssl.h"
#include "sslproto.h"
#include "secmod.h"
#include "secmime.h"
#include "ocsp.h"
#include "secerr.h"
#include "sslerr.h"

#include "nsXULAppAPI.h"

#ifdef XP_WIN
#include "nsILocalFileWin.h"
#endif

#include "p12plcy.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::psm;

#ifdef MOZ_LOGGING
PRLogModuleInfo* gPIPNSSLog = nullptr;
#endif

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

int nsNSSComponent::mInstanceCount = 0;

#ifndef NSS_NO_LIBPKIX
bool nsNSSComponent::globalConstFlagUsePKIXVerification = false;
#endif

// XXX tmp callback for slot password
extern char* pk11PasswordPrompt(PK11SlotInfo *slot, PRBool retry, void *arg);

#define PIPNSS_STRBUNDLE_URL "chrome://pipnss/locale/pipnss.properties"
#define NSSERR_STRBUNDLE_URL "chrome://pipnss/locale/nsserrors.properties"

#ifndef MOZ_DISABLE_CRYPTOLEGACY
//This class is used to run the callback code
//passed to the event handlers for smart card notification
class nsTokenEventRunnable : public nsIRunnable {
public:
  nsTokenEventRunnable(const nsAString &aType, const nsAString &aTokenName);
  virtual ~nsTokenEventRunnable();

  NS_IMETHOD Run ();
  NS_DECL_THREADSAFE_ISUPPORTS
private:
  nsString mType;
  nsString mTokenName;
};

// ISuuports implementation for nsTokenEventRunnable
NS_IMPL_ISUPPORTS1(nsTokenEventRunnable, nsIRunnable)

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
#endif // MOZ_DISABLE_CRYPTOLEGACY

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
  static int32_t haveLoaded = 0;

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
#ifndef MOZ_DISABLE_CRYPTOLEGACY
   mThreadList(nullptr),
#endif
   mCertVerificationThread(nullptr)
{
#ifdef PR_LOGGING
  if (!gPIPNSSLog)
    gPIPNSSLog = PR_NewLogModule("pipnss");
#endif
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent::ctor\n"));
  mObserversRegistered = false;

#ifndef NSS_NO_LIBPKIX
  // In order to keep startup time lower, we delay loading and 
  // registering all identity data until first needed.
  memset(&mIdentityInfoCallOnce, 0, sizeof(PRCallOnceType));
#endif

  NS_ASSERTION( (0 == mInstanceCount), "nsNSSComponent is a singleton, but instantiated multiple times!");
  ++mInstanceCount;
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
    mCertVerificationThread = nullptr;
  }
}

void
nsNSSComponent::createBackgroundThreads()
{
  NS_ASSERTION(!mCertVerificationThread,
               "Cert verification thread already created.");

  mCertVerificationThread = new nsCertVerificationThread;
  nsresult rv = mCertVerificationThread->startThread(
    NS_LITERAL_CSTRING("Cert Verify"));

  if (NS_FAILED(rv)) {
    delete mCertVerificationThread;
    mCertVerificationThread = nullptr;
  }
}

nsNSSComponent::~nsNSSComponent()
{
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent::dtor\n"));

  deleteBackgroundThreads();

  // All cleanup code requiring services needs to happen in xpcom_shutdown

  ShutdownNSS();
  SharedSSLState::GlobalCleanup();
  RememberCertErrorsTable::Cleanup();
  --mInstanceCount;
  delete mShutdownObjectList;

  // We are being freed, drop the haveLoaded flag to re-enable
  // potential nss initialization later.
  EnsureNSSInitialized(nssShutdown);

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent::dtor finished\n"));
}

#ifndef MOZ_DISABLE_CRYPTOLEGACY
NS_IMETHODIMP
nsNSSComponent::PostEvent(const nsAString &eventType, 
                                                  const nsAString &tokenName)
{
  nsCOMPtr<nsIRunnable> runnable = 
                               new nsTokenEventRunnable(eventType, tokenName);

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
                                      const nsAString &eventType,
                                      const nsAString &tokenName)
{
  if (!domWin) {
    return NS_OK;
  }

  // first walk the children and dispatch their events 
  nsresult rv;
  nsCOMPtr<nsIDOMWindowCollection> frames;
  rv = domWin->GetFrames(getter_AddRefs(frames));
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t length;
  frames->GetLength(&length);
  uint32_t i;
  for (i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMWindow> childWin;
    frames->Item(i, getter_AddRefs(childWin));
    DispatchEventToWindow(childWin, eventType, tokenName);
  }

  // check if we've enabled smart card events on this window
  // NOTE: it's not an error to say that we aren't going to dispatch
  // the event.
  nsCOMPtr<nsIDOMCrypto> crypto;
  domWin->GetCrypto(getter_AddRefs(crypto));
  if (!crypto) {
    return NS_OK; // nope, it doesn't have a crypto property
  }

  bool boolrv;
  crypto->GetEnableSmartCardEvents(&boolrv);
  if (!boolrv) {
    return NS_OK; // nope, it's not enabled.
  }

  // dispatch the event ...

  // find the document
  nsCOMPtr<nsIDOMDocument> doc;
  rv = domWin->GetDocument(getter_AddRefs(doc));
  if (!doc) {
    return NS_FAILED(rv) ? rv : NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> d = do_QueryInterface(doc);

  // create the event
  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMSmartCardEvent(getter_AddRefs(event), d, nullptr, nullptr);
  nsCOMPtr<nsIDOMSmartCardEvent> smartCardEvent = do_QueryInterface(event);
  rv = smartCardEvent->InitSmartCardEvent(eventType, false, true, tokenName);
  NS_ENSURE_SUCCESS(rv, rv);
  smartCardEvent->SetTrusted(true);

  // Send it 
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(doc, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return target->DispatchEvent(smartCardEvent, &boolrv);
}
#endif // MOZ_DISABLE_CRYPTOLEGACY

NS_IMETHODIMP
nsNSSComponent::PIPBundleFormatStringFromName(const char *name,
                                              const PRUnichar **params,
                                              uint32_t numParams,
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
                                              uint32_t numParams,
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

#ifndef MOZ_DISABLE_CRYPTOLEGACY
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
    if (!mThreadList) {
      mThreadList = new SmartCardThreadList();
    }
    newThread = new SmartCardMonitoringThread(module);
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
  mThreadList = nullptr;
}
#endif // MOZ_DISABLE_CRYPTOLEGACY

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
    if (!newString) {
        return nullptr;
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
  SECMODModule *RootsModule = nullptr;

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
    int32_t modType;
    SECMOD_DeleteModule(RootsModule->commonName, &modType);
    SECMOD_DestroyModule(RootsModule);
    RootsModule = nullptr;
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
    char *fullLibraryPath = nullptr;

    if (!possible_ckbi_locations[il])
    {
      fullLibraryPath = PR_GetLibraryName(nullptr, "nssckbi");
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

      nsAutoCString processDir;
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
                            nullptr, // no parent 
                            false); // do not recurse

    if (RootsModule) {
      bool found = (RootsModule->loaded);

      SECMOD_DestroyModule(RootsModule);
      RootsModule = nullptr;

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
 {"security.ssl3.ecdhe_rsa_aes_256_sha", TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA}, // 256-bit AES encryption with ECDHE-RSA and a SHA1 MAC
 {"security.ssl3.ecdhe_rsa_aes_128_sha", TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with ECDHE-RSA and a SHA1 MAC
 {"security.ssl3.ecdhe_rsa_des_ede3_sha", TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with ECDHE-RSA and a SHA1 MAC
 {"security.ssl3.ecdhe_rsa_rc4_128_sha", TLS_ECDHE_RSA_WITH_RC4_128_SHA}, // 128-bit RC4 encryption with ECDHE-RSA and a SHA1 MAC
 {"security.ssl3.ecdh_ecdsa_aes_256_sha", TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA}, // 256-bit AES encryption with ECDH-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdh_ecdsa_aes_128_sha", TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with ECDH-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdh_ecdsa_des_ede3_sha", TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with ECDH-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdh_ecdsa_rc4_128_sha", TLS_ECDH_ECDSA_WITH_RC4_128_SHA}, // 128-bit RC4 encryption with ECDH-ECDSA and a SHA1 MAC
 {"security.ssl3.ecdh_rsa_aes_256_sha", TLS_ECDH_RSA_WITH_AES_256_CBC_SHA}, // 256-bit AES encryption with ECDH-RSA and a SHA1 MAC
 {"security.ssl3.ecdh_rsa_aes_128_sha", TLS_ECDH_RSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with ECDH-RSA and a SHA1 MAC
 {"security.ssl3.ecdh_rsa_des_ede3_sha", TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with ECDH-RSA and a SHA1 MAC
 {"security.ssl3.ecdh_rsa_rc4_128_sha", TLS_ECDH_RSA_WITH_RC4_128_SHA}, // 128-bit RC4 encryption with ECDH-RSA and a SHA1 MAC
 {"security.ssl3.dhe_rsa_camellia_128_sha", TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA}, // 128-bit Camellia encryption with RSA, DHE, and a SHA1 MAC
 {"security.ssl3.dhe_dss_camellia_128_sha", TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA}, // 128-bit Camellia encryption with DSA, DHE, and a SHA1 MAC
 {"security.ssl3.rsa_camellia_128_sha", TLS_RSA_WITH_CAMELLIA_128_CBC_SHA}, // 128-bit Camellia encryption with RSA and a SHA1 MAC
 {"security.ssl3.dhe_rsa_aes_128_sha", TLS_DHE_RSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with RSA, DHE, and a SHA1 MAC
 {"security.ssl3.dhe_dss_aes_128_sha", TLS_DHE_DSS_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with DSA, DHE, and a SHA1 MAC
 {"security.ssl3.rsa_aes_128_sha", TLS_RSA_WITH_AES_128_CBC_SHA}, // 128-bit AES encryption with RSA and a SHA1 MAC
 {"security.ssl3.dhe_rsa_des_ede3_sha", SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with RSA, DHE, and a SHA1 MAC
 {"security.ssl3.dhe_dss_des_ede3_sha", SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA}, // 168-bit Triple DES with DSA, DHE, and a SHA1 MAC
 {"security.ssl3.rsa_seed_sha", TLS_RSA_WITH_SEED_CBC_SHA}, // SEED encryption with RSA and a SHA1 MAC
 {"security.ssl3.ecdhe_ecdsa_aes_128_gcm_sha256", TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256}, // 128-bit AES-GCM encryption with ECDHE-ECDSA
 {"security.ssl3.ecdhe_rsa_aes_128_gcm_sha256", TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256}, // 128-bit AES-GCM encryption with ECDHE-RSA
 {nullptr, 0} /* end marker */
};

static void
setNonPkixOcspEnabled(int32_t ocspEnabled)
{
  // Note: this preference is numeric vs boolean because previously we
  // supported more than two options.
  if (!ocspEnabled) {
    CERT_DisableOCSPChecking(CERT_GetDefaultCertDB());
    CERT_DisableOCSPDefaultResponder(CERT_GetDefaultCertDB());
  } else {
    CERT_EnableOCSPChecking(CERT_GetDefaultCertDB());
    CERT_DisableOCSPDefaultResponder(CERT_GetDefaultCertDB());
  }
}

#define CRL_DOWNLOAD_DEFAULT false
#define OCSP_ENABLED_DEFAULT 1
#define OCSP_REQUIRED_DEFAULT false
#define FRESH_REVOCATION_REQUIRED_DEFAULT false
#define MISSING_CERT_DOWNLOAD_DEFAULT false
#define FIRST_REVO_METHOD_DEFAULT "ocsp"
#define USE_NSS_LIBPKIX_DEFAULT false
#define OCSP_STAPLING_ENABLED_DEFAULT true

// Caller must hold a lock on nsNSSComponent::mutex when calling this function
void nsNSSComponent::setValidationOptions()
{
  nsNSSShutDownPreventionLock locker;

  bool crlDownloading = Preferences::GetBool("security.CRL_download.enabled",
                                             CRL_DOWNLOAD_DEFAULT);
  // 0 = disabled, 1 = enabled
  int32_t ocspEnabled = Preferences::GetInt("security.OCSP.enabled",
                                            OCSP_ENABLED_DEFAULT);

  bool ocspRequired = Preferences::GetBool("security.OCSP.require",
                                           OCSP_REQUIRED_DEFAULT);
  bool anyFreshRequired = Preferences::GetBool("security.fresh_revocation_info.require",
                                               FRESH_REVOCATION_REQUIRED_DEFAULT);
  bool aiaDownloadEnabled = Preferences::GetBool("security.missing_cert_download.enabled",
                                                 MISSING_CERT_DOWNLOAD_DEFAULT);

  nsCString firstNetworkRevo =
    Preferences::GetCString("security.first_network_revocation_method");
  if (firstNetworkRevo.IsEmpty()) {
    firstNetworkRevo = FIRST_REVO_METHOD_DEFAULT;
  }

  bool ocspStaplingEnabled = Preferences::GetBool("security.ssl.enable_ocsp_stapling",
                                                  OCSP_STAPLING_ENABLED_DEFAULT);
  if (!ocspEnabled) {
    ocspStaplingEnabled = false;
  }
  PublicSSLState()->SetOCSPStaplingEnabled(ocspStaplingEnabled);
  PrivateSSLState()->SetOCSPStaplingEnabled(ocspStaplingEnabled);

  setNonPkixOcspEnabled(ocspEnabled);

  CERT_SetOCSPFailureMode( ocspRequired ?
                           ocspMode_FailureIsVerificationFailure
                           : ocspMode_FailureIsNotAVerificationFailure);

  mDefaultCertVerifier = new CertVerifier(
      aiaDownloadEnabled ? 
        CertVerifier::missing_cert_download_on : CertVerifier::missing_cert_download_off,
      crlDownloading ?
        CertVerifier::crl_download_allowed : CertVerifier::crl_local_only,
      ocspEnabled ? 
        CertVerifier::ocsp_on : CertVerifier::ocsp_off,
      ocspRequired ? 
        CertVerifier::ocsp_strict : CertVerifier::ocsp_relaxed,
      anyFreshRequired ?
        CertVerifier::any_revo_strict : CertVerifier::any_revo_relaxed,
      firstNetworkRevo.get());

  /*
    * The new defaults might change the validity of already established SSL sessions,
    * let's not reuse them.
    */
  SSL_ClearSessionCache();
}

// Enable the TLS versions given in the prefs, defaulting to SSL 3.0 and
// TLS 1.0 when the prefs aren't set or when they are set to invalid values.
nsresult
nsNSSComponent::setEnabledTLSVersions()
{
  // keep these values in sync with security-prefs.js and firefox.js
  static const int32_t PSM_DEFAULT_MIN_TLS_VERSION = 0;
  static const int32_t PSM_DEFAULT_MAX_TLS_VERSION = 1;

  int32_t minVersion = Preferences::GetInt("security.tls.version.min",
                                           PSM_DEFAULT_MIN_TLS_VERSION);
  int32_t maxVersion = Preferences::GetInt("security.tls.version.max",
                                           PSM_DEFAULT_MAX_TLS_VERSION);

  // 0 means SSL 3.0, 1 means TLS 1.0, 2 means TLS 1.1, etc.
  minVersion += SSL_LIBRARY_VERSION_3_0;
  maxVersion += SSL_LIBRARY_VERSION_3_0;

  SSLVersionRange range = { (uint16_t) minVersion, (uint16_t) maxVersion };

  if (minVersion != (int32_t) range.min || // prevent truncation
      maxVersion != (int32_t) range.max || // prevent truncation
      SSL_VersionRangeSetDefault(ssl_variant_stream, &range) != SECSuccess) {
    range.min = SSL_LIBRARY_VERSION_3_0 + PSM_DEFAULT_MIN_TLS_VERSION;
    range.max = SSL_LIBRARY_VERSION_3_0 + PSM_DEFAULT_MAX_TLS_VERSION;
    if (SSL_VersionRangeSetDefault(ssl_variant_stream, &range)
          != SECSuccess) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
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
  // 0 = disabled, 1 = enabled
  int32_t ocspEnabled = Preferences::GetInt("security.OCSP.enabled",
                                            OCSP_ENABLED_DEFAULT);

  setNonPkixOcspEnabled(ocspEnabled);

  if (ocspEnabled)
    SSL_ClearSessionCache();

  return NS_OK;
}

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

static const bool SUPPRESS_WARNING_PREF_DEFAULT = false;
static const bool MD5_ENABLED_DEFAULT = false;
static const bool TLS_SESSION_TICKETS_ENABLED_DEFAULT = true;
static const bool REQUIRE_SAFE_NEGOTIATION_DEFAULT = false;
static const bool ALLOW_UNRESTRICTED_RENEGO_DEFAULT = false;
static const bool FALSE_START_ENABLED_DEFAULT = true;
static const bool CIPHER_ENABLED_DEFAULT = false;

nsresult
nsNSSComponent::InitializeNSS(bool showWarningBox)
{
  // Can be called both during init and profile change.
  // Needs mutex protection.

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent::InitializeNSS\n"));

  static_assert(nsINSSErrorsService::NSS_SEC_ERROR_BASE == SEC_ERROR_BASE &&
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
    nsAutoCString profileStr;
    nsCOMPtr<nsIFile> profilePath;

    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(profilePath));
    if (NS_FAILED(rv)) {
      PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("Unable to get profile directory\n"));
      ConfigureInternalPKCS11Token();
      SECStatus init_rv = NSS_NoDB_Init(nullptr);
      if (init_rv != SECSuccess) {
        nsPSMInitPanic::SetPanic();
        return NS_ERROR_NOT_AVAILABLE;
      }
    }
    else
    {
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

#ifndef NSS_NO_LIBPKIX
    globalConstFlagUsePKIXVerification =
      Preferences::GetBool("security.use_libpkix_verification", USE_NSS_LIBPKIX_DEFAULT);
#endif

    bool suppressWarningPref =
      Preferences::GetBool("security.suppress_nss_rw_impossible_warning",
                           SUPPRESS_WARNING_PREF_DEFAULT);

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
    uint32_t init_flags = NSS_INIT_NOROOTINIT | NSS_INIT_OPTIMIZESPACE;
    SECStatus init_rv = ::NSS_Initialize(profileStr.get(), "", "",
                                         SECMOD_DB, init_flags);

    if (init_rv != SECSuccess) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("can not init NSS r/w in %s\n", profileStr.get()));

      if (suppressWarningPref) {
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

      PK11_SetPasswordFunc(PK11PasswordPrompt);

      SharedSSLState::GlobalInit();

      // Register an observer so we can inform NSS when these prefs change
      Preferences::AddStrongObserver(this, "security.");

      SSL_OptionSetDefault(SSL_ENABLE_SSL2, false);
      SSL_OptionSetDefault(SSL_V2_COMPATIBLE_HELLO, false);

      rv = setEnabledTLSVersions();
      if (NS_FAILED(rv)) {
        nsPSMInitPanic::SetPanic();
        return NS_ERROR_UNEXPECTED;
      }

      bool md5Enabled = Preferences::GetBool("security.enable_md5_signatures",
                                             MD5_ENABLED_DEFAULT);
      configureMD5(md5Enabled);

      // Configure TLS session tickets
      bool tlsSessionTicketsEnabled =
        Preferences::GetBool("security.enable_tls_session_tickets",
                             TLS_SESSION_TICKETS_ENABLED_DEFAULT);
      SSL_OptionSetDefault(SSL_ENABLE_SESSION_TICKETS, tlsSessionTicketsEnabled);

      bool requireSafeNegotiation =
        Preferences::GetBool("security.ssl.require_safe_negotiation",
                             REQUIRE_SAFE_NEGOTIATION_DEFAULT);
      SSL_OptionSetDefault(SSL_REQUIRE_SAFE_NEGOTIATION, requireSafeNegotiation);

      bool allowUnrestrictedRenego =
        Preferences::GetBool("security.ssl.allow_unrestricted_renego_everywhere__temporarily_available_pref",
                             ALLOW_UNRESTRICTED_RENEGO_DEFAULT);
      SSL_OptionSetDefault(SSL_ENABLE_RENEGOTIATION,
                           allowUnrestrictedRenego ?
                             SSL_RENEGOTIATE_UNRESTRICTED :
                             SSL_RENEGOTIATE_REQUIRES_XTN);

//    Bug 920248: temporarily disable false start
//    bool falseStartEnabled = Preferences::GetBool("security.ssl.enable_false_start",
//                                                  FALSE_START_ENABLED_DEFAULT);
      SSL_OptionSetDefault(SSL_ENABLE_FALSE_START, false);

      // Disable any ciphers that NSS might have enabled by default
      for (uint16_t i = 0; i < SSL_NumImplementedCiphers; ++i)
      {
        uint16_t cipher_id = SSL_ImplementedCiphers[i];
        SSL_CipherPrefSetDefault(cipher_id, false);
      }

      bool cipherEnabled;
      // Now only set SSL/TLS ciphers we knew about at compile time
      for (CipherPref* cp = CipherPrefs; cp->pref; ++cp) {
        cipherEnabled = Preferences::GetBool(cp->pref, CIPHER_ENABLED_DEFAULT);
        SSL_CipherPrefSetDefault(cp->id, cipherEnabled);
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
      setValidationOptions();

      mHttpForNSS.initTable();
      mHttpForNSS.registerHttpClient();

      InstallLoadableRoots();

#ifndef MOZ_DISABLE_CRYPTOLEGACY
      LaunchSmartCardThreads();
#endif

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

void
nsNSSComponent::ShutdownNSS()
{
  // Can be called both during init and profile change,
  // needs mutex protection.
  
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent::ShutdownNSS\n"));

  MutexAutoLock lock(mutex);

  if (mNSSInitialized) {
    mNSSInitialized = false;

    PK11_SetPasswordFunc((PK11PasswordFunc)nullptr);
    mHttpForNSS.unregisterHttpClient();

    Preferences::RemoveObserver(this, "security.");

#ifndef MOZ_DISABLE_CRYPTOLEGACY
    ShutdownSmartCardThreads();
#endif
    SSL_ClearSessionCache();
    UnloadLoadableRoots();
#ifndef NSS_NO_LIBPKIX
    CleanupIdentityInfo();
#endif
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("evaporating psm resources\n"));
    mShutdownObjectList->evaporateAllNSSResources();
    EnsureNSSInitialized(nssShutdown);
    if (SECSuccess != ::NSS_Shutdown()) {
      PR_LOG(gPIPNSSLog, PR_LOG_ALWAYS, ("NSS SHUTDOWN FAILURE\n"));
    }
    else {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS shutdown =====>> OK <<=====\n"));
    }
  }
}

static const bool SEND_LM_DEFAULT = false;

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

  bool sendLM = Preferences::GetBool("network.ntlm.send-lm-response",
                                     SEND_LM_DEFAULT);
  nsNTLMAuthModule::SetSendLM(sendLM);

  // Do that before NSS init, to make sure we won't get unloaded.
  RegisterObservers();

  rv = InitializeNSS(true); // ok to show a warning box on failure
  if (NS_FAILED(rv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_ERROR, ("Unable to Initialize NSS.\n"));

    DeregisterObservers();
    mPIPNSSBundle = nullptr;
    return rv;
  }

  RememberCertErrorsTable::Init();
  
  createBackgroundThreads();
  if (!mCertVerificationThread)
  {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("NSS init, could not create threads\n"));

    DeregisterObservers();
    mPIPNSSBundle = nullptr;
    return NS_ERROR_OUT_OF_MEMORY;
  }

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
NS_IMPL_ISUPPORTS5(nsNSSComponent,
                   nsISignatureVerifier,
                   nsIEntropyCollector,
                   nsINSSComponent,
                   nsIObserver,
                   nsISupportsWeakReference)


/* Callback functions for decoder. For now, use empty/default functions. */
static void ContentCallback(void *arg, 
                                           const char *buf,
                                           unsigned long len)
{
}

static PK11SymKey * GetDecryptKeyCallback(void *arg, 
                                                 SECAlgorithmID *algid)
{
  return nullptr;
}

static PRBool DecryptionAllowedCallback(SECAlgorithmID *algid,  
                                               PK11SymKey *bulkkey)
{
  return SECMIME_DecryptionAllowed(algid, bulkkey);
}

static void * GetPasswordKeyCallback(void *arg, void *handle)
{
  return nullptr;
}

NS_IMETHODIMP
nsNSSComponent::VerifySignature(const char* aRSABuf, uint32_t aRSABufLen,
                                const char* aPlaintext, uint32_t aPlaintextLen,
                                int32_t* aErrorCode,
                                nsICertificatePrincipal** aPrincipal)
{
  if (!aPrincipal || !aErrorCode) {
    return NS_ERROR_NULL_POINTER;
  }

  *aErrorCode = 0;
  *aPrincipal = nullptr;

  nsNSSShutDownPreventionLock locker;
  ScopedSEC_PKCS7ContentInfo p7_info;
  unsigned char hash[SHA1_LENGTH]; 

  SECItem item;
  item.type = siEncodedCertBuffer;
  item.data = (unsigned char*)aRSABuf;
  item.len = aRSABufLen;
  p7_info = SEC_PKCS7DecodeItem(&item,
                                ContentCallback, nullptr,
                                GetPasswordKeyCallback, nullptr,
                                GetDecryptKeyCallback, nullptr,
                                DecryptionAllowedCallback);

  if (!p7_info) {
    return NS_ERROR_FAILURE;
  }

  // Make sure we call SEC_PKCS7DestroyContentInfo after this point;
  // otherwise we leak data in p7_info

  //-- If a plaintext was provided, hash it.
  SECItem digest;
  digest.data = nullptr;
  digest.len = 0;

  if (aPlaintext) {
    HASHContext* hash_ctxt;
    uint32_t hashLen = 0;

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
    
      nsCOMPtr<nsICertificatePrincipal> certPrincipal =
        new nsCertificatePrincipal(NS_ConvertUTF16toUTF8(fingerprint),
                                   NS_ConvertUTF16toUTF8(subjectName),
                                   NS_ConvertUTF16toUTF8(orgName),
                                   pCert);

      certPrincipal.swap(*aPrincipal);
    } while (0);
  }

  return rv2;
}

NS_IMETHODIMP
nsNSSComponent::RandomUpdate(void *entropy, int32_t bufLen)
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
#define PROFILE_CHANGE_TEARDOWN_TOPIC "profile-change-teardown"
#define PROFILE_BEFORE_CHANGE_TOPIC "profile-before-change"
#define PROFILE_DO_CHANGE_TOPIC "profile-do-change"

NS_IMETHODIMP
nsNSSComponent::Observe(nsISupports *aSubject, const char *aTopic, 
                        const PRUnichar *someData)
{
  if (nsCRT::strcmp(aTopic, PROFILE_CHANGE_TEARDOWN_TOPIC) == 0) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("in PSM code, receiving change-teardown\n"));
    DoProfileChangeTeardown(aSubject);
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
      }
    }
  }
  else if (nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {

    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsNSSComponent: XPCom shutdown observed\n"));

    // Cleanup code that requires services, it's too late in destructor.

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
    NS_ConvertUTF16toUTF8  prefName(someData);

    if (prefName.Equals("security.tls.version.min") ||
        prefName.Equals("security.tls.version.max")) {
      (void) setEnabledTLSVersions();
      clearSessionCache = true;
    } else if (prefName.Equals("security.enable_md5_signatures")) {
      bool md5Enabled = Preferences::GetBool("security.enable_md5_signatures",
                                             MD5_ENABLED_DEFAULT);
      configureMD5(md5Enabled);
      clearSessionCache = true;
    } else if (prefName.Equals("security.enable_tls_session_tickets")) {
      bool tlsSessionTicketsEnabled =
        Preferences::GetBool("security.enable_tls_session_tickets",
                             TLS_SESSION_TICKETS_ENABLED_DEFAULT);
      SSL_OptionSetDefault(SSL_ENABLE_SESSION_TICKETS, tlsSessionTicketsEnabled);
    } else if (prefName.Equals("security.ssl.require_safe_negotiation")) {
      bool requireSafeNegotiation =
        Preferences::GetBool("security.ssl.require_safe_negotiation",
                             REQUIRE_SAFE_NEGOTIATION_DEFAULT);
      SSL_OptionSetDefault(SSL_REQUIRE_SAFE_NEGOTIATION, requireSafeNegotiation);
    } else if (prefName.Equals("security.ssl.allow_unrestricted_renego_everywhere__temporarily_available_pref")) {
      bool allowUnrestrictedRenego =
        Preferences::GetBool("security.ssl.allow_unrestricted_renego_everywhere__temporarily_available_pref",
                             ALLOW_UNRESTRICTED_RENEGO_DEFAULT);
      SSL_OptionSetDefault(SSL_ENABLE_RENEGOTIATION,
                           allowUnrestrictedRenego ?
                             SSL_RENEGOTIATE_UNRESTRICTED :
                             SSL_RENEGOTIATE_REQUIRES_XTN);
    } else if (prefName.Equals("security.ssl.enable_false_start")) {
//    Bug 920248: temporarily disable false start
//    bool falseStartEnabled = Preferences::GetBool("security.ssl.enable_false_start",
//                                                  FALSE_START_ENABLED_DEFAULT);
      SSL_OptionSetDefault(SSL_ENABLE_FALSE_START, false);
    } else if (prefName.Equals("security.OCSP.enabled")
               || prefName.Equals("security.CRL_download.enabled")
               || prefName.Equals("security.fresh_revocation_info.require")
               || prefName.Equals("security.missing_cert_download.enabled")
               || prefName.Equals("security.first_network_revocation_method")
               || prefName.Equals("security.OCSP.require")
               || prefName.Equals("security.ssl.enable_ocsp_stapling")) {
      MutexAutoLock lock(mutex);
      setValidationOptions();
    } else if (prefName.Equals("network.ntlm.send-lm-response")) {
      bool sendLM = Preferences::GetBool("network.ntlm.send-lm-response",
                                         SEND_LM_DEFAULT);
      nsNTLMAuthModule::SetSendLM(sendLM);
    } else {
      /* Look through the cipher table and set according to pref setting */
      bool cipherEnabled;
      for (CipherPref* cp = CipherPrefs; cp->pref; ++cp) {
        if (prefName.Equals(cp->pref)) {
          cipherEnabled = Preferences::GetBool(cp->pref, CIPHER_ENABLED_DEFAULT);
          SSL_CipherPrefSetDefault(cp->id, cipherEnabled);
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
  *result = nullptr;

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
      rv = prompter->Alert(nullptr, message.get());
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

  nsClientAuthRememberService::ClearAllRememberedDecisions();

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

    observerService->AddObserver(this, PROFILE_CHANGE_TEARDOWN_TOPIC, false);
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

    observerService->RemoveObserver(this, PROFILE_CHANGE_TEARDOWN_TOPIC);
    observerService->RemoveObserver(this, PROFILE_BEFORE_CHANGE_TOPIC);
    observerService->RemoveObserver(this, PROFILE_DO_CHANGE_TOPIC);
    observerService->RemoveObserver(this, PROFILE_CHANGE_NET_TEARDOWN_TOPIC);
    observerService->RemoveObserver(this, PROFILE_CHANGE_NET_RESTORE_TOPIC);
  }
  return NS_OK;
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
  mShutdownObjectList->ifPossibleDisallowUI();
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
    
  if (needsCleanup) {
    ShutdownNSS();
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
nsNSSComponent::IsNSSInitialized(bool *initialized)
{
  MutexAutoLock lock(mutex);
  *initialized = mNSSInitialized;
  return NS_OK;
}

//#ifndef NSS_NO_LIBPKIX
NS_IMETHODIMP
nsNSSComponent::GetDefaultCertVerifier(RefPtr<CertVerifier> &out)
{
  MutexAutoLock lock(mutex);
  if (!mNSSInitialized)
      return NS_ERROR_NOT_INITIALIZED;
  out = mDefaultCertVerifier;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(PipUIContext, nsIInterfaceRequestor)

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
  *result = nullptr;

  if (!NS_IsMainThread()) {
    NS_ERROR("PipUIContext::GetInterface called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (!uuid.Equals(NS_GET_IID(nsIPrompt)))
    return NS_ERROR_NO_INTERFACE;

  nsIPrompt * prompt = nullptr;
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
