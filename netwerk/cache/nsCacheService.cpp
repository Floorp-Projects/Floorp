/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is nsCacheService.cpp, released
 * February 10, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan, 10-February-2001
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

#include "mozilla/Util.h"

#include "necko-config.h"

#include "nsCache.h"
#include "nsCacheService.h"
#include "nsCacheRequest.h"
#include "nsCacheEntry.h"
#include "nsCacheEntryDescriptor.h"
#include "nsCacheDevice.h"
#include "nsMemoryCacheDevice.h"
#include "nsICacheVisitor.h"
#include "nsDiskCacheDevice.h"
#include "nsDiskCacheDeviceSQL.h"

#include "nsIMemoryReporter.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsILocalFile.h"
#include "nsIOService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "nsVoidArray.h"
#include "nsDeleteDir.h"
#include "nsIPrivateBrowsingService.h"
#include "nsNetCID.h"
#include <math.h>  // for log()
#include "mozilla/Util.h" // for DebugOnly
#include "mozilla/Services.h"

#include "mozilla/FunctionTimer.h"

#include "mozilla/net/NeckoCommon.h"

using namespace mozilla;

/******************************************************************************
 * nsCacheProfilePrefObserver
 *****************************************************************************/
#define DISK_CACHE_ENABLE_PREF      "browser.cache.disk.enable"
#define DISK_CACHE_DIR_PREF         "browser.cache.disk.parent_directory"
#define DISK_CACHE_SMART_SIZE_FIRST_RUN_PREF\
    "browser.cache.disk.smart_size.first_run"
#define DISK_CACHE_SMART_SIZE_ENABLED_PREF \
    "browser.cache.disk.smart_size.enabled"
#define DISK_CACHE_SMART_SIZE_PREF "browser.cache.disk.smart_size_cached_value"
#define DISK_CACHE_CAPACITY_PREF    "browser.cache.disk.capacity"
#define DISK_CACHE_MAX_ENTRY_SIZE_PREF "browser.cache.disk.max_entry_size"
#define DISK_CACHE_CAPACITY         256000

#define OFFLINE_CACHE_ENABLE_PREF   "browser.cache.offline.enable"
#define OFFLINE_CACHE_DIR_PREF      "browser.cache.offline.parent_directory"
#define OFFLINE_CACHE_CAPACITY_PREF "browser.cache.offline.capacity"
#define OFFLINE_CACHE_CAPACITY      512000

#define MEMORY_CACHE_ENABLE_PREF    "browser.cache.memory.enable"
#define MEMORY_CACHE_CAPACITY_PREF  "browser.cache.memory.capacity"
#define MEMORY_CACHE_MAX_ENTRY_SIZE_PREF "browser.cache.memory.max_entry_size"

static const char * observerList[] = { 
    "profile-before-change",
    "profile-do-change",
    NS_XPCOM_SHUTDOWN_OBSERVER_ID,
    NS_PRIVATE_BROWSING_SWITCH_TOPIC
};
static const char * prefList[] = { 
    DISK_CACHE_ENABLE_PREF,
    DISK_CACHE_SMART_SIZE_ENABLED_PREF,
    DISK_CACHE_CAPACITY_PREF,
    DISK_CACHE_DIR_PREF,
    DISK_CACHE_MAX_ENTRY_SIZE_PREF,
    OFFLINE_CACHE_ENABLE_PREF,
    OFFLINE_CACHE_CAPACITY_PREF,
    OFFLINE_CACHE_DIR_PREF,
    MEMORY_CACHE_ENABLE_PREF,
    MEMORY_CACHE_CAPACITY_PREF,
    MEMORY_CACHE_MAX_ENTRY_SIZE_PREF
};

// Cache sizes, in KB
const PRInt32 DEFAULT_CACHE_SIZE = 250 * 1024;  // 250 MB
const PRInt32 MIN_CACHE_SIZE = 50 * 1024;       //  50 MB
const PRInt32 MAX_CACHE_SIZE = 1024 * 1024;     //   1 GB
// Default cache size was 50 MB for many years until FF 4:
const PRInt32 PRE_GECKO_2_0_DEFAULT_CACHE_SIZE = 50 * 1024;

class nsCacheProfilePrefObserver : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    nsCacheProfilePrefObserver()
        : mHaveProfile(PR_FALSE)
        , mDiskCacheEnabled(PR_FALSE)
        , mDiskCacheCapacity(0)
        , mDiskCacheMaxEntrySize(-1) // -1 means "no limit"
        , mOfflineCacheEnabled(PR_FALSE)
        , mOfflineCacheCapacity(0)
        , mMemoryCacheEnabled(PR_TRUE)
        , mMemoryCacheCapacity(-1)
        , mMemoryCacheMaxEntrySize(-1) // -1 means "no limit"
        , mInPrivateBrowsing(PR_FALSE)
    {
    }

    virtual ~nsCacheProfilePrefObserver() {}
    
    nsresult        Install();
    void            Remove();
    nsresult        ReadPrefs(nsIPrefBranch* branch);
    
    bool            DiskCacheEnabled();
    PRInt32         DiskCacheCapacity()         { return mDiskCacheCapacity; }
    void            SetDiskCacheCapacity(PRInt32);
    PRInt32         DiskCacheMaxEntrySize()     { return mDiskCacheMaxEntrySize; }
    nsILocalFile *  DiskCacheParentDirectory()  { return mDiskCacheParentDirectory; }

    bool            OfflineCacheEnabled();
    PRInt32         OfflineCacheCapacity()         { return mOfflineCacheCapacity; }
    nsILocalFile *  OfflineCacheParentDirectory()  { return mOfflineCacheParentDirectory; }
    
    bool            MemoryCacheEnabled();
    PRInt32         MemoryCacheCapacity();
    PRInt32         MemoryCacheMaxEntrySize()     { return mMemoryCacheMaxEntrySize; }

    static PRUint32 GetSmartCacheSize(const nsAString& cachePath);

private:
    bool                    PermittedToSmartSize(nsIPrefBranch*, bool firstRun);
    bool                    mHaveProfile;
    
    bool                    mDiskCacheEnabled;
    PRInt32                 mDiskCacheCapacity; // in kilobytes
    PRInt32                 mDiskCacheMaxEntrySize; // in kilobytes
    nsCOMPtr<nsILocalFile>  mDiskCacheParentDirectory;

    bool                    mOfflineCacheEnabled;
    PRInt32                 mOfflineCacheCapacity; // in kilobytes
    nsCOMPtr<nsILocalFile>  mOfflineCacheParentDirectory;
    
    bool                    mMemoryCacheEnabled;
    PRInt32                 mMemoryCacheCapacity; // in kilobytes
    PRInt32                 mMemoryCacheMaxEntrySize; // in kilobytes

    bool                    mInPrivateBrowsing;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheProfilePrefObserver, nsIObserver)

// Runnable sent to main thread after the cache IO thread calculates available
// disk space, so that there is no race in setting mDiskCacheCapacity.
class nsSetSmartSizeEvent: public nsRunnable 
{
public:
    nsSetSmartSizeEvent(bool firstRun, PRInt32 smartSize) 
        : mFirstRun(firstRun) , mSmartSize(smartSize) {}

    NS_IMETHOD Run() 
    {
        nsresult rv;
        NS_ASSERTION(NS_IsMainThread(), 
                     "Setting smart size data off the main thread");

        // Main thread may have already called nsCacheService::Shutdown
        if (!nsCacheService::gService || !nsCacheService::gService->mObserver)
            return NS_ERROR_NOT_AVAILABLE;
    
        bool smartSizeEnabled;
        nsCOMPtr<nsIPrefBranch2> branch = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (!branch) {
            NS_WARNING("Failed to get pref service!");
            return NS_ERROR_NOT_AVAILABLE;
        }
        // ensure smart sizing wasn't switched off while event was pending
        rv = branch->GetBoolPref(DISK_CACHE_SMART_SIZE_ENABLED_PREF,
                                 &smartSizeEnabled);
        if (NS_FAILED(rv)) 
            smartSizeEnabled = PR_FALSE;
        if (smartSizeEnabled) {
            nsCacheService::SetDiskCacheCapacity(mSmartSize);
            // also set on observer, in case mDiskDevice not init'd yet.
            nsCacheService::gService->mObserver->SetDiskCacheCapacity(mSmartSize);
            rv = branch->SetIntPref(DISK_CACHE_SMART_SIZE_PREF, mSmartSize);
            if (NS_FAILED(rv)) 
                NS_WARNING("Failed to set smart size pref");
        }
        return rv;
    }

private: 
    bool mFirstRun;
    PRInt32 mSmartSize;
};


// Runnable sent from main thread to cacheIO thread
class nsGetSmartSizeEvent: public nsRunnable
{
public:
    nsGetSmartSizeEvent(bool firstRun, const nsAString& cachePath) 
      : mFirstRun(firstRun)
      , mCachePath(cachePath)
      , mSmartSize(0) 
    {}
   
    // Calculates user's disk space available on a background thread and
    // dispatches this value back to the main thread.
    NS_IMETHOD Run()
    {
        mSmartSize = 
          nsCacheProfilePrefObserver::GetSmartCacheSize(mCachePath);
        nsCOMPtr<nsIRunnable> event = new nsSetSmartSizeEvent(mFirstRun,
                                                              mSmartSize);
        NS_DispatchToMainThread(event);
        return NS_OK;
    }

private: 
    bool mFirstRun;
    nsString mCachePath;
    PRInt32 mSmartSize;
};

class nsBlockOnCacheThreadEvent : public nsRunnable {
public:
    nsBlockOnCacheThreadEvent()
    {
    }
    NS_IMETHOD Run()
    {
        nsCacheServiceAutoLock autoLock;
#ifdef PR_LOGGING
        CACHE_LOG_DEBUG(("nsBlockOnCacheThreadEvent [%p]\n", this));
#endif
        nsCacheService::gService->mCondVar.Notify();
        return NS_OK;
    }
};


nsresult
nsCacheProfilePrefObserver::Install()
{
    // install profile-change observer
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (!observerService)
        return NS_ERROR_FAILURE;
    
    nsresult rv, rv2 = NS_OK;
    for (unsigned int i=0; i<ArrayLength(observerList); i++) {
        rv = observerService->AddObserver(this, observerList[i], PR_FALSE);
        if (NS_FAILED(rv)) 
            rv2 = rv;
    }
    
    // install preferences observer
    nsCOMPtr<nsIPrefBranch2> branch = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!branch) return NS_ERROR_FAILURE;

    for (unsigned int i=0; i<ArrayLength(prefList); i++) {
        rv = branch->AddObserver(prefList[i], this, PR_FALSE);
        if (NS_FAILED(rv))
            rv2 = rv;
    }

    // determine the initial status of the private browsing mode
    nsCOMPtr<nsIPrivateBrowsingService> pbs =
      do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
    if (pbs)
      pbs->GetPrivateBrowsingEnabled(&mInPrivateBrowsing);

    // Determine if we have a profile already
    //     Install() is called *after* the profile-after-change notification
    //     when there is only a single profile, or it is specified on the
    //     commandline at startup.
    //     In that case, we detect the presence of a profile by the existence
    //     of the NS_APP_USER_PROFILE_50_DIR directory.

    nsCOMPtr<nsIFile> directory;
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(directory));
    if (NS_SUCCEEDED(rv))
        mHaveProfile = PR_TRUE;

    rv = ReadPrefs(branch);
    NS_ENSURE_SUCCESS(rv, rv);

    return rv2;
}


void
nsCacheProfilePrefObserver::Remove()
{
    // remove Observer Service observers
    nsCOMPtr<nsIObserverService> obs =
        mozilla::services::GetObserverService();
    if (obs) {
        for (unsigned int i=0; i<ArrayLength(observerList); i++) {
            obs->RemoveObserver(this, observerList[i]);
        }
    }

    // remove Pref Service observers
    nsCOMPtr<nsIPrefBranch2> prefs =
        do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return;
    for (unsigned int i=0; i<ArrayLength(prefList); i++)
        prefs->RemoveObserver(prefList[i], this); // remove cache pref observers
}

void
nsCacheProfilePrefObserver::SetDiskCacheCapacity(PRInt32 capacity)
{
    mDiskCacheCapacity = NS_MAX(0, capacity);
}


NS_IMETHODIMP
nsCacheProfilePrefObserver::Observe(nsISupports *     subject,
                                    const char *      topic,
                                    const PRUnichar * data_unicode)
{
    nsresult rv;
    NS_ConvertUTF16toUTF8 data(data_unicode);
    CACHE_LOG_ALWAYS(("Observe [topic=%s data=%s]\n", topic, data.get()));

    if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, topic)) {
        // xpcom going away, shutdown cache service
        if (nsCacheService::GlobalInstance())
            nsCacheService::GlobalInstance()->Shutdown();
    
    } else if (!strcmp("profile-before-change", topic)) {
        // profile before change
        mHaveProfile = PR_FALSE;

        // XXX shutdown devices
        nsCacheService::OnProfileShutdown(!strcmp("shutdown-cleanse",
                                                  data.get()));
        
    } else if (!strcmp("profile-do-change", topic)) {
        // profile after change
        mHaveProfile = PR_TRUE;
        nsCOMPtr<nsIPrefBranch> branch = do_GetService(NS_PREFSERVICE_CONTRACTID);
        ReadPrefs(branch);
        nsCacheService::OnProfileChanged();
    
    } else if (!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, topic)) {

        // ignore pref changes until we're done switch profiles
        if (!mHaveProfile)  
            return NS_OK;

        nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(subject, &rv);
        if (NS_FAILED(rv))  
            return rv;

        // which preference changed?
        if (!strcmp(DISK_CACHE_ENABLE_PREF, data.get())) {

            if (!mInPrivateBrowsing) {
                rv = branch->GetBoolPref(DISK_CACHE_ENABLE_PREF,
                                         &mDiskCacheEnabled);
                if (NS_FAILED(rv))  
                    return rv;
                nsCacheService::SetDiskCacheEnabled(DiskCacheEnabled());
            }

        } else if (!strcmp(DISK_CACHE_CAPACITY_PREF, data.get())) {

            PRInt32 capacity = 0;
            rv = branch->GetIntPref(DISK_CACHE_CAPACITY_PREF, &capacity);
            if (NS_FAILED(rv))  
                return rv;
            mDiskCacheCapacity = NS_MAX(0, capacity);
            nsCacheService::SetDiskCacheCapacity(mDiskCacheCapacity);
       
        // Update the cache capacity when smart sizing is turned on/off 
        } else if (!strcmp(DISK_CACHE_SMART_SIZE_ENABLED_PREF, data.get())) {
            // Is the update because smartsizing was turned on, or off?
            bool smartSizeEnabled;
            rv = branch->GetBoolPref(DISK_CACHE_SMART_SIZE_ENABLED_PREF,
                                     &smartSizeEnabled);
            if (NS_FAILED(rv)) 
                return rv;
            PRInt32 newCapacity = 0;
            if (smartSizeEnabled) {
                // Dispatch event to update smart size: just keep using old
                // value if this fails at any point
                if (!mDiskCacheParentDirectory) 
                    return NS_ERROR_NOT_AVAILABLE; // disk cache disabled anyway
                nsAutoString cachePath;
                rv = mDiskCacheParentDirectory->GetPath(cachePath);
                if (NS_FAILED(rv)) 
                    return rv;
                // Smart sizing switched on: recalculate the capacity.
                nsCOMPtr<nsIRunnable> event = 
                    new nsGetSmartSizeEvent(false, cachePath);
                rv = nsCacheService::DispatchToCacheIOThread(event);
            } else {
                // Smart sizing switched off: use user specified size
                rv = branch->GetIntPref(DISK_CACHE_CAPACITY_PREF, &newCapacity);
                if (NS_FAILED(rv)) 
                    return rv;
                mDiskCacheCapacity = NS_MAX(0, newCapacity);
                nsCacheService::SetDiskCacheCapacity(mDiskCacheCapacity);
            }
        } else if (!strcmp(DISK_CACHE_MAX_ENTRY_SIZE_PREF, data.get())) {
            PRInt32 newMaxSize;
            rv = branch->GetIntPref(DISK_CACHE_MAX_ENTRY_SIZE_PREF,
                                    &newMaxSize);
            if (NS_FAILED(rv)) 
                return rv;

            mDiskCacheMaxEntrySize = NS_MAX(-1, newMaxSize);
            nsCacheService::SetDiskCacheMaxEntrySize(mDiskCacheMaxEntrySize);
          
#if 0            
        } else if (!strcmp(DISK_CACHE_DIR_PREF, data.get())) {
            // XXX We probaby don't want to respond to this pref except after
            // XXX profile changes.  Ideally, there should be somekind of user
            // XXX notification that the pref change won't take effect until
            // XXX the next time the profile changes (browser launch)
#endif            
        } else

        // which preference changed?
        if (!strcmp(OFFLINE_CACHE_ENABLE_PREF, data.get())) {

            if (!mInPrivateBrowsing) {
                rv = branch->GetBoolPref(OFFLINE_CACHE_ENABLE_PREF,
                                         &mOfflineCacheEnabled);
                if (NS_FAILED(rv))  return rv;
                nsCacheService::SetOfflineCacheEnabled(OfflineCacheEnabled());
            }

        } else if (!strcmp(OFFLINE_CACHE_CAPACITY_PREF, data.get())) {

            PRInt32 capacity = 0;
            rv = branch->GetIntPref(OFFLINE_CACHE_CAPACITY_PREF, &capacity);
            if (NS_FAILED(rv))  return rv;
            mOfflineCacheCapacity = NS_MAX(0, capacity);
            nsCacheService::SetOfflineCacheCapacity(mOfflineCacheCapacity);
#if 0
        } else if (!strcmp(OFFLINE_CACHE_DIR_PREF, data.get())) {
            // XXX We probaby don't want to respond to this pref except after
            // XXX profile changes.  Ideally, there should be some kind of user
            // XXX notification that the pref change won't take effect until
            // XXX the next time the profile changes (browser launch)
#endif
        } else

        if (!strcmp(MEMORY_CACHE_ENABLE_PREF, data.get())) {

            rv = branch->GetBoolPref(MEMORY_CACHE_ENABLE_PREF,
                                     &mMemoryCacheEnabled);
            if (NS_FAILED(rv))  
                return rv;
            nsCacheService::SetMemoryCache();
            
        } else if (!strcmp(MEMORY_CACHE_CAPACITY_PREF, data.get())) {

            mMemoryCacheCapacity = -1;
            (void) branch->GetIntPref(MEMORY_CACHE_CAPACITY_PREF,
                                      &mMemoryCacheCapacity);
            nsCacheService::SetMemoryCache();
        } else if (!strcmp(MEMORY_CACHE_MAX_ENTRY_SIZE_PREF, data.get())) {
            PRInt32 newMaxSize;
            rv = branch->GetIntPref(MEMORY_CACHE_MAX_ENTRY_SIZE_PREF,
                                     &newMaxSize);
            if (NS_FAILED(rv)) 
                return rv;
            
            mMemoryCacheMaxEntrySize = NS_MAX(-1, newMaxSize);
            nsCacheService::SetMemoryCacheMaxEntrySize(mMemoryCacheMaxEntrySize);
        }
    } else if (!strcmp(NS_PRIVATE_BROWSING_SWITCH_TOPIC, topic)) {
        if (!strcmp(NS_PRIVATE_BROWSING_ENTER, data.get())) {
            mInPrivateBrowsing = PR_TRUE;

            nsCacheService::OnEnterExitPrivateBrowsing();

            mDiskCacheEnabled = PR_FALSE;
            nsCacheService::SetDiskCacheEnabled(DiskCacheEnabled());

            mOfflineCacheEnabled = PR_FALSE;
            nsCacheService::SetOfflineCacheEnabled(OfflineCacheEnabled());
        } else if (!strcmp(NS_PRIVATE_BROWSING_LEAVE, data.get())) {
            mInPrivateBrowsing = PR_FALSE;

            nsCacheService::OnEnterExitPrivateBrowsing();

            nsCOMPtr<nsIPrefBranch> branch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
            if (NS_FAILED(rv))  
                return rv;

            mDiskCacheEnabled = PR_TRUE; // by default enabled
            (void) branch->GetBoolPref(DISK_CACHE_ENABLE_PREF,
                                       &mDiskCacheEnabled);
            nsCacheService::SetDiskCacheEnabled(DiskCacheEnabled());

            mOfflineCacheEnabled = PR_TRUE; // by default enabled
            (void) branch->GetBoolPref(OFFLINE_CACHE_ENABLE_PREF,
                                       &mOfflineCacheEnabled);
            nsCacheService::SetOfflineCacheEnabled(OfflineCacheEnabled());
        }
    }
    
    return NS_OK;
}

 /* Computes our best guess for the default size of the user's disk cache, 
  * based on the amount of space they have free on their hard drive. 
  * We use a tiered scheme: the more space available, 
  * the larger the disk cache will be. However, we do not want
  * to enable the disk cache to grow to an unbounded size, so the larger the
  * user's available space is, the smaller of a percentage we take. We set a
  * lower bound of 50MB and an upper bound of 1GB.  
  *
  *@param:  None.
  *@return: The size that the user's disk cache should default to, in kBytes.
  */
PRUint32
nsCacheProfilePrefObserver::GetSmartCacheSize(const nsAString& cachePath) 
{
  // Check for free space on device where cache directory lives
  nsresult rv;
  nsCOMPtr<nsILocalFile> 
      cacheDirectory (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv) || !cacheDirectory)
    return DEFAULT_CACHE_SIZE;
  rv = cacheDirectory->InitWithPath(cachePath);
  if (NS_FAILED(rv))
    return DEFAULT_CACHE_SIZE;
  PRInt64 bytesAvailable;
  rv = cacheDirectory->GetDiskSpaceAvailable(&bytesAvailable);
  if (NS_FAILED(rv))
    return DEFAULT_CACHE_SIZE;
  PRInt64 kBytesAvail = bytesAvailable / 1024;
  
  // 0 MB <= Available < 500 MB: Use between 50MB and 200MB
  if (kBytesAvail < DEFAULT_CACHE_SIZE * 2) 
    return NS_MAX<PRInt64>(MIN_CACHE_SIZE, kBytesAvail * 4 / 10);
  
  // 500MB <= Available < 2.5 GB: Use 250MB 
  if (kBytesAvail < static_cast<PRInt64>(DEFAULT_CACHE_SIZE) * 10) 
    return DEFAULT_CACHE_SIZE;

  // 2.5 GB <= Available < 5 GB: Use between 250MB and 500MB
  if (kBytesAvail < static_cast<PRInt64>(DEFAULT_CACHE_SIZE) * 20) 
    return kBytesAvail / 10;

  // 5 GB <= Available < 50 GB:  Use 625MB
  if (kBytesAvail < static_cast<PRInt64>(DEFAULT_CACHE_SIZE) * 200 ) 
    return DEFAULT_CACHE_SIZE * 5 / 2;

  // 50 GB <= Available < 75 GB: Use 800MB
  if (kBytesAvail < static_cast<PRInt64>(DEFAULT_CACHE_SIZE) * 300) 
    return DEFAULT_CACHE_SIZE / 5 * 16;  
  
  // Use 1 GB
  return MAX_CACHE_SIZE;
}

/* Determine if we are permitted to dynamically size the user's disk cache based
 * on their disk space available. We may do this so long as the pref 
 * smart_size.enabled is true.
 */
bool
nsCacheProfilePrefObserver::PermittedToSmartSize(nsIPrefBranch* branch, bool
                                                 firstRun)
{
    nsresult rv;
    if (firstRun) {
        // check if user has set cache size in the past
        bool userSet;
        rv = branch->PrefHasUserValue(DISK_CACHE_CAPACITY_PREF, &userSet);
        if (NS_FAILED(rv)) userSet = PR_TRUE;
        if (userSet) {
            PRInt32 oldCapacity;
            // If user explicitly set cache size to be smaller than old default
            // of 50 MB, then keep user's value. Otherwise use smart sizing.
            rv = branch->GetIntPref(DISK_CACHE_CAPACITY_PREF, &oldCapacity);
            if (oldCapacity < PRE_GECKO_2_0_DEFAULT_CACHE_SIZE) {
                branch->SetBoolPref(DISK_CACHE_SMART_SIZE_ENABLED_PREF, 
                                    PR_FALSE);
                return false;
            }
        }
        // Set manual setting to MAX cache size as starting val for any
        // adjustment by user: (bug 559942 comment 65)
        branch->SetIntPref(DISK_CACHE_CAPACITY_PREF, MAX_CACHE_SIZE);
    }
    bool smartSizeEnabled; 
    rv = branch->GetBoolPref(DISK_CACHE_SMART_SIZE_ENABLED_PREF,
                             &smartSizeEnabled);
    if (NS_FAILED(rv)) 
        return false;
    return !!smartSizeEnabled;
}


nsresult
nsCacheProfilePrefObserver::ReadPrefs(nsIPrefBranch* branch)
{
    nsresult rv = NS_OK;

    // read disk cache device prefs
    if (!mInPrivateBrowsing) {
        mDiskCacheEnabled = PR_TRUE;  // presume disk cache is enabled
        (void) branch->GetBoolPref(DISK_CACHE_ENABLE_PREF, &mDiskCacheEnabled);
    }

    mDiskCacheCapacity = DISK_CACHE_CAPACITY;
    (void)branch->GetIntPref(DISK_CACHE_CAPACITY_PREF, &mDiskCacheCapacity);
    mDiskCacheCapacity = NS_MAX(0, mDiskCacheCapacity);

    (void) branch->GetIntPref(DISK_CACHE_MAX_ENTRY_SIZE_PREF,
                              &mDiskCacheMaxEntrySize);
    mDiskCacheMaxEntrySize = NS_MAX(-1, mDiskCacheMaxEntrySize);
    
    (void) branch->GetComplexValue(DISK_CACHE_DIR_PREF,     // ignore error
                                   NS_GET_IID(nsILocalFile),
                                   getter_AddRefs(mDiskCacheParentDirectory));
    
    if (!mDiskCacheParentDirectory) {
        nsCOMPtr<nsIFile>  directory;

        // try to get the disk cache parent directory
        rv = NS_GetSpecialDirectory(NS_APP_CACHE_PARENT_DIR,
                                    getter_AddRefs(directory));
        if (NS_FAILED(rv)) {
            // try to get the profile directory (there may not be a profile yet)
            nsCOMPtr<nsIFile> profDir;
            NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                   getter_AddRefs(profDir));
            NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                                   getter_AddRefs(directory));
            if (!directory)
                directory = profDir;
            else if (profDir) {
                bool same;
                if (NS_SUCCEEDED(profDir->Equals(directory, &same)) && !same) {
                    // We no longer store the cache directory in the main
                    // profile directory, so we should cleanup the old one.
                    rv = profDir->AppendNative(NS_LITERAL_CSTRING("Cache"));
                    if (NS_SUCCEEDED(rv)) {
                        bool exists;
                        if (NS_SUCCEEDED(profDir->Exists(&exists)) && exists)
                            DeleteDir(profDir, PR_FALSE, PR_FALSE);
                    }
                }
            }
        }
        // use file cache in build tree only if asked, to avoid cache dir litter
        if (!directory && PR_GetEnv("NECKO_DEV_ENABLE_DISK_CACHE")) {
            rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                        getter_AddRefs(directory));
        }
        if (directory)
            mDiskCacheParentDirectory = do_QueryInterface(directory, &rv);
    }
    if (mDiskCacheParentDirectory) {
        bool firstSmartSizeRun;
        rv = branch->GetBoolPref(DISK_CACHE_SMART_SIZE_FIRST_RUN_PREF, 
                                 &firstSmartSizeRun); 
        if (NS_FAILED(rv)) 
            firstSmartSizeRun = PR_FALSE;
        if (PermittedToSmartSize(branch, firstSmartSizeRun)) {
            // Avoid evictions: use previous cache size until smart size event
            // updates mDiskCacheCapacity
            if (!firstSmartSizeRun) {
                PRInt32 oldSmartSize;
                rv = branch->GetIntPref(DISK_CACHE_SMART_SIZE_PREF,
                                        &oldSmartSize);
                mDiskCacheCapacity = oldSmartSize;
            } else {
                PRInt32 oldCapacity;
                rv = branch->GetIntPref(DISK_CACHE_CAPACITY_PREF, &oldCapacity);
                if (NS_SUCCEEDED(rv)) {
                    mDiskCacheCapacity = oldCapacity;
                } else {
                    mDiskCacheCapacity = DEFAULT_CACHE_SIZE;
                }
            }
            nsAutoString cachePath;
            rv = mDiskCacheParentDirectory->GetPath(cachePath);
            if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsIRunnable> event = 
                    new nsGetSmartSizeEvent(!!firstSmartSizeRun, cachePath);
                nsCacheService::DispatchToCacheIOThread(event);
            }
        }

        if (firstSmartSizeRun) {
            // It is no longer our first run
            rv = branch->SetBoolPref(DISK_CACHE_SMART_SIZE_FIRST_RUN_PREF, 
                                     PR_FALSE);
            if (NS_FAILED(rv)) 
                NS_WARNING("Failed setting first_run pref in ReadPrefs.");
        }
    }

    // read offline cache device prefs
    if (!mInPrivateBrowsing) {
        mOfflineCacheEnabled = PR_TRUE;  // presume offline cache is enabled
        (void) branch->GetBoolPref(OFFLINE_CACHE_ENABLE_PREF,
                                   &mOfflineCacheEnabled);
    }

    mOfflineCacheCapacity = OFFLINE_CACHE_CAPACITY;
    (void)branch->GetIntPref(OFFLINE_CACHE_CAPACITY_PREF,
                             &mOfflineCacheCapacity);
    mOfflineCacheCapacity = NS_MAX(0, mOfflineCacheCapacity);

    (void) branch->GetComplexValue(OFFLINE_CACHE_DIR_PREF,     // ignore error
                                   NS_GET_IID(nsILocalFile),
                                   getter_AddRefs(mOfflineCacheParentDirectory));

    if (!mOfflineCacheParentDirectory) {
        nsCOMPtr<nsIFile>  directory;

        // try to get the offline cache parent directory
        rv = NS_GetSpecialDirectory(NS_APP_CACHE_PARENT_DIR,
                                    getter_AddRefs(directory));
        if (NS_FAILED(rv)) {
            // try to get the profile directory (there may not be a profile yet)
            nsCOMPtr<nsIFile> profDir;
            NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                   getter_AddRefs(profDir));
            NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                                   getter_AddRefs(directory));
            if (!directory)
                directory = profDir;
        }
#if DEBUG
        if (!directory) {
            // use current process directory during development
            rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                        getter_AddRefs(directory));
        }
#endif
        if (directory)
            mOfflineCacheParentDirectory = do_QueryInterface(directory, &rv);
    }

    // read memory cache device prefs
    (void) branch->GetBoolPref(MEMORY_CACHE_ENABLE_PREF, &mMemoryCacheEnabled);

    mMemoryCacheCapacity = -1;
    (void) branch->GetIntPref(MEMORY_CACHE_CAPACITY_PREF,
                              &mMemoryCacheCapacity);

    (void) branch->GetIntPref(MEMORY_CACHE_MAX_ENTRY_SIZE_PREF,
                              &mMemoryCacheMaxEntrySize);
    mMemoryCacheMaxEntrySize = NS_MAX(-1, mMemoryCacheMaxEntrySize);
    return rv;
}

nsresult
nsCacheService::DispatchToCacheIOThread(nsIRunnable* event)
{
    if (!gService->mCacheIOThread) return NS_ERROR_NOT_AVAILABLE;
    return gService->mCacheIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

nsresult
nsCacheService::SyncWithCacheIOThread()
{
    gService->mLock.AssertCurrentThreadOwns();
    if (!gService->mCacheIOThread) return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<nsIRunnable> event = new nsBlockOnCacheThreadEvent();

    // dispatch event - it will notify the monitor when it's done
    nsresult rv =
        gService->mCacheIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed dispatching block-event");
        return NS_ERROR_UNEXPECTED;
    }

    // wait until notified, then return
    rv = gService->mCondVar.Wait();

    return rv;
}


bool
nsCacheProfilePrefObserver::DiskCacheEnabled()
{
    if ((mDiskCacheCapacity == 0) || (!mDiskCacheParentDirectory))  return PR_FALSE;
    return mDiskCacheEnabled;
}


bool
nsCacheProfilePrefObserver::OfflineCacheEnabled()
{
    if ((mOfflineCacheCapacity == 0) || (!mOfflineCacheParentDirectory))
        return PR_FALSE;

    return mOfflineCacheEnabled;
}


bool
nsCacheProfilePrefObserver::MemoryCacheEnabled()
{
    if (mMemoryCacheCapacity == 0)  return PR_FALSE;
    return mMemoryCacheEnabled;
}


/**
 * MemoryCacheCapacity
 *
 * If the browser.cache.memory.capacity preference is positive, we use that
 * value for the amount of memory available for the cache.
 *
 * If browser.cache.memory.capacity is zero, the memory cache is disabled.
 * 
 * If browser.cache.memory.capacity is negative or not present, we use a
 * formula that grows less than linearly with the amount of system memory, 
 * with an upper limit on the cache size. No matter how much physical RAM is
 * present, the default cache size would not exceed 32 MB. This maximum would
 * apply only to systems with more than 4 GB of RAM (e.g. terminal servers)
 *
 *   RAM   Cache
 *   ---   -----
 *   32 Mb   2 Mb
 *   64 Mb   4 Mb
 *  128 Mb   6 Mb
 *  256 Mb  10 Mb
 *  512 Mb  14 Mb
 * 1024 Mb  18 Mb
 * 2048 Mb  24 Mb
 * 4096 Mb  30 Mb
 *
 * The equation for this is (for cache size C and memory size K (kbytes)):
 *  x = log2(K) - 14
 *  C = x^2/3 + x + 2/3 + 0.1 (0.1 for rounding)
 *  if (C > 32) C = 32
 */

PRInt32
nsCacheProfilePrefObserver::MemoryCacheCapacity()
{
    PRInt32 capacity = mMemoryCacheCapacity;
    if (capacity >= 0) {
        CACHE_LOG_DEBUG(("Memory cache capacity forced to %d\n", capacity));
        return capacity;
    }

    static PRUint64 bytes = PR_GetPhysicalMemorySize();
    CACHE_LOG_DEBUG(("Physical Memory size is %llu\n", bytes));

    // If getting the physical memory failed, arbitrarily assume
    // 32 MB of RAM. We use a low default to have a reasonable
    // size on all the devices we support.
    if (bytes == 0)
        bytes = 32 * 1024 * 1024;

    // Conversion from unsigned int64 to double doesn't work on all platforms.
    // We need to truncate the value at LL_MAXINT to make sure we don't
    // overflow.
    if (LL_CMP(bytes, >, LL_MAXINT))
        bytes = LL_MAXINT;

    PRUint64 kbytes;
    LL_SHR(kbytes, bytes, 10);

    double kBytesD;
    LL_L2D(kBytesD, (PRInt64) kbytes);

    double x = log(kBytesD)/log(2.0) - 14;
    if (x > 0) {
        capacity = (PRInt32)(x * x / 3.0 + x + 2.0 / 3 + 0.1); // 0.1 for rounding
        if (capacity > 32)
            capacity = 32;
        capacity   *= 1024;
    } else {
        capacity    = 0;
    }

    return capacity;
}


/******************************************************************************
 * nsProcessRequestEvent
 *****************************************************************************/

class nsProcessRequestEvent : public nsRunnable {
public:
    nsProcessRequestEvent(nsCacheRequest *aRequest)
    {
        mRequest = aRequest;
    }

    NS_IMETHOD Run()
    {
        nsresult rv;

        NS_ASSERTION(mRequest->mListener,
                     "Sync OpenCacheEntry() posted to background thread!");

        nsCacheServiceAutoLock lock;
        rv = nsCacheService::gService->ProcessRequest(mRequest,
                                                      PR_FALSE,
                                                      nsnull);

        // Don't delete the request if it was queued
        if (rv != NS_ERROR_CACHE_WAIT_FOR_VALIDATION)
            delete mRequest;

        return NS_OK;
    }

protected:
    virtual ~nsProcessRequestEvent() {}

private:
    nsCacheRequest *mRequest;
};

/******************************************************************************
 * nsCacheService
 *****************************************************************************/
nsCacheService *   nsCacheService::gService = nsnull;

static nsCOMPtr<nsIMemoryReporter> MemoryCacheReporter = nsnull;

NS_THREADSAFE_MEMORY_REPORTER_IMPLEMENT(NetworkMemoryCache,
    "explicit/network-memory-cache",
    KIND_HEAP,
    UNITS_BYTES,
    nsCacheService::MemoryDeviceSize,
    "Memory used by the network memory cache.")

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheService, nsICacheService)

nsCacheService::nsCacheService()
    : mLock("nsCacheService.mLock"),
      mCondVar(mLock, "nsCacheService.mCondVar"),
      mInitialized(PR_FALSE),
      mEnableMemoryDevice(PR_TRUE),
      mEnableDiskDevice(PR_TRUE),
      mMemoryDevice(nsnull),
      mDiskDevice(nsnull),
      mOfflineDevice(nsnull),
      mTotalEntries(0),
      mCacheHits(0),
      mCacheMisses(0),
      mMaxKeyLength(0),
      mMaxDataSize(0),
      mMaxMetaSize(0),
      mDeactivateFailures(0),
      mDeactivatedUnboundEntries(0)
{
    NS_ASSERTION(gService==nsnull, "multiple nsCacheService instances!");
    gService = this;

    // create list of cache devices
    PR_INIT_CLIST(&mDoomedEntries);
}

nsCacheService::~nsCacheService()
{
    if (mInitialized) // Shutdown hasn't been called yet.
        (void) Shutdown();

    gService = nsnull;
}


nsresult
nsCacheService::Init()
{
    NS_TIME_FUNCTION;

    NS_ASSERTION(!mInitialized, "nsCacheService already initialized.");
    if (mInitialized)
        return NS_ERROR_ALREADY_INITIALIZED;

    if (mozilla::net::IsNeckoChild()) {
        return NS_ERROR_UNEXPECTED;
    }

    CACHE_LOG_INIT();

    nsresult rv = NS_NewThread(getter_AddRefs(mCacheIOThread));
    if (NS_FAILED(rv)) {
        NS_WARNING("Can't create cache IO thread");
    }

    // initialize hashtable for active cache entries
    rv = mActiveEntries.Init();
    if (NS_FAILED(rv)) return rv;
    
    // create profile/preference observer
    mObserver = new nsCacheProfilePrefObserver();
    if (!mObserver)  return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mObserver);
    
    mObserver->Install();
    mEnableDiskDevice    = mObserver->DiskCacheEnabled();
    mEnableOfflineDevice = mObserver->OfflineCacheEnabled();
    mEnableMemoryDevice  = mObserver->MemoryCacheEnabled();

    mInitialized = PR_TRUE;
    return NS_OK;
}


void
nsCacheService::Shutdown()
{
    nsCOMPtr<nsIThread> cacheIOThread;

    {
    nsCacheServiceAutoLock lock;
    NS_ASSERTION(mInitialized, 
                 "can't shutdown nsCacheService unless it has been initialized.");

    if (mInitialized) {

        mInitialized = PR_FALSE;

        mObserver->Remove();
        NS_RELEASE(mObserver);
        
        // Clear entries
        ClearDoomList();
        ClearActiveEntries();

        // Make sure to wait for any pending cache-operations before
        // proceeding with destructive actions (bug #620660)
        (void) SyncWithCacheIOThread();
        
        // unregister memory reporter, before deleting the memory device, just
        // to be safe
        NS_UnregisterMemoryReporter(MemoryCacheReporter);
        MemoryCacheReporter = nsnull;

        // deallocate memory and disk caches
        delete mMemoryDevice;
        mMemoryDevice = nsnull;

        delete mDiskDevice;
        mDiskDevice = nsnull;

        NS_IF_RELEASE(mOfflineDevice);

#ifdef PR_LOGGING
        LogCacheStatistics();
#endif

        mCacheIOThread.swap(cacheIOThread);
    }
    } // lock

    if (cacheIOThread)
        cacheIOThread->Shutdown();
}


nsresult
nsCacheService::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsresult  rv;

    if (aOuter != nsnull)
        return NS_ERROR_NO_AGGREGATION;

    nsCacheService * cacheService = new nsCacheService();
    if (cacheService == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(cacheService);
    rv = cacheService->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = cacheService->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(cacheService);
    return rv;
}


NS_IMETHODIMP
nsCacheService::CreateSession(const char *          clientID,
                              nsCacheStoragePolicy  storagePolicy, 
                              bool                  streamBased,
                              nsICacheSession     **result)
{
    *result = nsnull;

    if (this == nsnull)  return NS_ERROR_NOT_AVAILABLE;

    nsCacheSession * session = new nsCacheSession(clientID, storagePolicy, streamBased);
    if (!session)  return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result = session);

    return NS_OK;
}


nsresult
nsCacheService::EvictEntriesForSession(nsCacheSession * session)
{
    NS_ASSERTION(gService, "nsCacheService::gService is null.");
    return gService->EvictEntriesForClient(session->ClientID()->get(),
                                 session->StoragePolicy());
}

namespace {

class EvictionNotifierRunnable : public nsRunnable
{
public:
    EvictionNotifierRunnable(nsISupports* aSubject)
        : mSubject(aSubject)
    { }

    NS_DECL_NSIRUNNABLE

private:
    nsCOMPtr<nsISupports> mSubject;
};

NS_IMETHODIMP
EvictionNotifierRunnable::Run()
{
    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    if (obsSvc) {
        obsSvc->NotifyObservers(mSubject,
                                NS_CACHESERVICE_EMPTYCACHE_TOPIC_ID,
                                nsnull);
    }
    return NS_OK;
}

} // anonymous namespace

nsresult
nsCacheService::EvictEntriesForClient(const char *          clientID,
                                      nsCacheStoragePolicy  storagePolicy)
{
    nsRefPtr<EvictionNotifierRunnable> r = new EvictionNotifierRunnable(this);
    NS_DispatchToMainThread(r);

    nsCacheServiceAutoLock lock;
    nsresult res = NS_OK;

    if (storagePolicy == nsICache::STORE_ANYWHERE ||
        storagePolicy == nsICache::STORE_ON_DISK) {

        if (mEnableDiskDevice) {
            nsresult rv;
            if (!mDiskDevice)
                rv = CreateDiskDevice();
            if (mDiskDevice)
                rv = mDiskDevice->EvictEntries(clientID);
            if (NS_FAILED(rv)) res = rv;
        }
    }

    // Only clear the offline cache if it has been specifically asked for.
    if (storagePolicy == nsICache::STORE_OFFLINE) {
        if (mEnableOfflineDevice) {
            nsresult rv;
            if (!mOfflineDevice)
                rv = CreateOfflineDevice();
            if (mOfflineDevice)
                rv = mOfflineDevice->EvictEntries(clientID);
            if (NS_FAILED(rv)) res = rv;
        }
    }

    if (storagePolicy == nsICache::STORE_ANYWHERE ||
        storagePolicy == nsICache::STORE_IN_MEMORY) {

        // If there is no memory device, there is no need to evict it...
        if (mMemoryDevice) {
            nsresult rv;
            rv = mMemoryDevice->EvictEntries(clientID);
            if (NS_FAILED(rv)) res = rv;
        }
    }

    return res;
}


nsresult        
nsCacheService::IsStorageEnabledForPolicy(nsCacheStoragePolicy  storagePolicy,
                                          bool *              result)
{
    if (gService == nsnull) return NS_ERROR_NOT_AVAILABLE;
    nsCacheServiceAutoLock lock;

    *result = gService->IsStorageEnabledForPolicy_Locked(storagePolicy);
    return NS_OK;
}


bool          
nsCacheService::IsStorageEnabledForPolicy_Locked(nsCacheStoragePolicy  storagePolicy)
{
    if (gService->mEnableMemoryDevice &&
        (storagePolicy == nsICache::STORE_ANYWHERE ||
         storagePolicy == nsICache::STORE_IN_MEMORY)) {
        return PR_TRUE;
    }
    if (gService->mEnableDiskDevice &&
        (storagePolicy == nsICache::STORE_ANYWHERE ||
         storagePolicy == nsICache::STORE_ON_DISK  ||
         storagePolicy == nsICache::STORE_ON_DISK_AS_FILE)) {
        return PR_TRUE;
    }
    if (gService->mEnableOfflineDevice &&
        storagePolicy == nsICache::STORE_OFFLINE) {
        return PR_TRUE;
    }
    
    return PR_FALSE;
}

NS_IMETHODIMP nsCacheService::VisitEntries(nsICacheVisitor *visitor)
{
    NS_ENSURE_ARG_POINTER(visitor);

    nsCacheServiceAutoLock lock;

    if (!(mEnableDiskDevice || mEnableMemoryDevice))
        return NS_ERROR_NOT_AVAILABLE;

    // XXX record the fact that a visitation is in progress, 
    // XXX i.e. keep list of visitors in progress.
    
    nsresult rv = NS_OK;
    // If there is no memory device, there are then also no entries to visit...
    if (mMemoryDevice) {
        rv = mMemoryDevice->Visit(visitor);
        if (NS_FAILED(rv)) return rv;
    }

    if (mEnableDiskDevice) {
        if (!mDiskDevice) {
            rv = CreateDiskDevice();
            if (NS_FAILED(rv)) return rv;
        }
        rv = mDiskDevice->Visit(visitor);
        if (NS_FAILED(rv)) return rv;
    }

    if (mEnableOfflineDevice) {
        if (!mOfflineDevice) {
            rv = CreateOfflineDevice();
            if (NS_FAILED(rv)) return rv;
        }
        rv = mOfflineDevice->Visit(visitor);
        if (NS_FAILED(rv)) return rv;
    }

    // XXX notify any shutdown process that visitation is complete for THIS visitor.
    // XXX keep queue of visitors

    return NS_OK;
}


NS_IMETHODIMP nsCacheService::EvictEntries(nsCacheStoragePolicy storagePolicy)
{
    return  EvictEntriesForClient(nsnull, storagePolicy);
}

NS_IMETHODIMP nsCacheService::GetCacheIOTarget(nsIEventTarget * *aCacheIOTarget)
{
    nsCacheServiceAutoLock lock;

    if (!mCacheIOThread)
        return NS_ERROR_NOT_AVAILABLE;

    NS_ADDREF(*aCacheIOTarget = mCacheIOThread);
    return NS_OK;
}

/**
 * Internal Methods
 */
nsresult
nsCacheService::CreateDiskDevice()
{
    if (!mInitialized)      return NS_ERROR_NOT_AVAILABLE;
    if (!mEnableDiskDevice) return NS_ERROR_NOT_AVAILABLE;
    if (mDiskDevice)        return NS_OK;

    mDiskDevice = new nsDiskCacheDevice;
    if (!mDiskDevice)       return NS_ERROR_OUT_OF_MEMORY;

    // set the preferences
    mDiskDevice->SetCacheParentDirectory(mObserver->DiskCacheParentDirectory());
    mDiskDevice->SetCapacity(mObserver->DiskCacheCapacity());
    mDiskDevice->SetMaxEntrySize(mObserver->DiskCacheMaxEntrySize());
    
    nsresult rv = mDiskDevice->Init();
    if (NS_FAILED(rv)) {
#if DEBUG
        printf("###\n");
        printf("### mDiskDevice->Init() failed (0x%.8x)\n", rv);
        printf("###    - disabling disk cache for this session.\n");
        printf("###\n");
#endif        
        mEnableDiskDevice = PR_FALSE;
        delete mDiskDevice;
        mDiskDevice = nsnull;
    }
    return rv;
}

nsresult
nsCacheService::CreateOfflineDevice()
{
    CACHE_LOG_ALWAYS(("Creating offline device"));

    if (!mInitialized)         return NS_ERROR_NOT_AVAILABLE;
    if (!mEnableOfflineDevice) return NS_ERROR_NOT_AVAILABLE;
    if (mOfflineDevice)        return NS_OK;

    mOfflineDevice = new nsOfflineCacheDevice;
    if (!mOfflineDevice)       return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(mOfflineDevice);

    // set the preferences
    mOfflineDevice->SetCacheParentDirectory(
        mObserver->OfflineCacheParentDirectory());
    mOfflineDevice->SetCapacity(mObserver->OfflineCacheCapacity());

    nsresult rv = mOfflineDevice->Init();
    if (NS_FAILED(rv)) {
        CACHE_LOG_DEBUG(("mOfflineDevice->Init() failed (0x%.8x)\n", rv));
        CACHE_LOG_DEBUG(("    - disabling offline cache for this session.\n"));

        mEnableOfflineDevice = PR_FALSE;
        NS_RELEASE(mOfflineDevice);
    }
    return rv;
}

nsresult
nsCacheService::CreateMemoryDevice()
{
    if (!mInitialized)        return NS_ERROR_NOT_AVAILABLE;
    if (!mEnableMemoryDevice) return NS_ERROR_NOT_AVAILABLE;
    if (mMemoryDevice)        return NS_OK;

    mMemoryDevice = new nsMemoryCacheDevice;
    if (!mMemoryDevice)       return NS_ERROR_OUT_OF_MEMORY;
    
    // set preference
    PRInt32 capacity = mObserver->MemoryCacheCapacity();
    CACHE_LOG_DEBUG(("Creating memory device with capacity %d\n", capacity));
    mMemoryDevice->SetCapacity(capacity);
    mMemoryDevice->SetMaxEntrySize(mObserver->MemoryCacheMaxEntrySize());

    nsresult rv = mMemoryDevice->Init();
    if (NS_FAILED(rv)) {
        NS_WARNING("Initialization of Memory Cache failed.");
        delete mMemoryDevice;
        mMemoryDevice = nsnull;
    }

    MemoryCacheReporter =
        new NS_MEMORY_REPORTER_NAME(NetworkMemoryCache);
    NS_RegisterMemoryReporter(MemoryCacheReporter);

    return rv;
}


nsresult
nsCacheService::CreateRequest(nsCacheSession *   session,
                              const nsACString & clientKey,
                              nsCacheAccessMode  accessRequested,
                              bool               blockingMode,
                              nsICacheListener * listener,
                              nsCacheRequest **  request)
{
    NS_ASSERTION(request, "CreateRequest: request is null");
     
    nsCString * key = new nsCString(*session->ClientID());
    if (!key)
        return NS_ERROR_OUT_OF_MEMORY;
    key->Append(':');
    key->Append(clientKey);

    if (mMaxKeyLength < key->Length()) mMaxKeyLength = key->Length();

    // create request
    *request = new  nsCacheRequest(key, listener, accessRequested, blockingMode, session);    
    if (!*request) {
        delete key;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!listener)  return NS_OK;  // we're sync, we're done.

    // get the request's thread
    (*request)->mThread = do_GetCurrentThread();
    
    return NS_OK;
}


class nsCacheListenerEvent : public nsRunnable
{
public:
    nsCacheListenerEvent(nsICacheListener *listener,
                         nsICacheEntryDescriptor *descriptor,
                         nsCacheAccessMode accessGranted,
                         nsresult status)
        : mListener(listener)      // transfers reference
        , mDescriptor(descriptor)  // transfers reference (may be null)
        , mAccessGranted(accessGranted)
        , mStatus(status)
    {}

    NS_IMETHOD Run()
    {
        mListener->OnCacheEntryAvailable(mDescriptor, mAccessGranted, mStatus);

        NS_RELEASE(mListener);
        NS_IF_RELEASE(mDescriptor);
        return NS_OK;
    }

private:
    // We explicitly leak mListener or mDescriptor if Run is not called
    // because otherwise we cannot guarantee that they are destroyed on
    // the right thread.

    nsICacheListener        *mListener;
    nsICacheEntryDescriptor *mDescriptor;
    nsCacheAccessMode        mAccessGranted;
    nsresult                 mStatus;
};


nsresult
nsCacheService::NotifyListener(nsCacheRequest *          request,
                               nsICacheEntryDescriptor * descriptor,
                               nsCacheAccessMode         accessGranted,
                               nsresult                  status)
{
    NS_ASSERTION(request->mThread, "no thread set in async request!");

    // Swap ownership, and release listener on target thread...
    nsICacheListener *listener = request->mListener;
    request->mListener = nsnull;

    nsCOMPtr<nsIRunnable> ev =
            new nsCacheListenerEvent(listener, descriptor,
                                     accessGranted, status);
    if (!ev) {
        // Better to leak listener and descriptor if we fail because we don't
        // want to destroy them inside the cache service lock or on potentially
        // the wrong thread.
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return request->mThread->Dispatch(ev, NS_DISPATCH_NORMAL);
}


nsresult
nsCacheService::ProcessRequest(nsCacheRequest *           request,
                               bool                       calledFromOpenCacheEntry,
                               nsICacheEntryDescriptor ** result)
{
    // !!! must be called with mLock held !!!
    nsresult           rv;
    nsCacheEntry *     entry = nsnull;
    nsCacheEntry *     doomedEntry = nsnull;
    nsCacheAccessMode  accessGranted = nsICache::ACCESS_NONE;
    if (result) *result = nsnull;

    while(1) {  // Activate entry loop
        rv = ActivateEntry(request, &entry, &doomedEntry);  // get the entry for this request
        if (NS_FAILED(rv))  break;

        while(1) { // Request Access loop
            NS_ASSERTION(entry, "no entry in Request Access loop!");
            // entry->RequestAccess queues request on entry
            rv = entry->RequestAccess(request, &accessGranted);
            if (rv != NS_ERROR_CACHE_WAIT_FOR_VALIDATION) break;
            
            if (request->mListener) // async exits - validate, doom, or close will resume
                return rv;
            
            if (request->IsBlocking()) {
                // XXX this is probably wrong...
                Unlock();
                rv = request->WaitForValidation();
                Lock();
            }

            PR_REMOVE_AND_INIT_LINK(request);
            if (NS_FAILED(rv)) break;   // non-blocking mode returns WAIT_FOR_VALIDATION error
            // okay, we're ready to process this request, request access again
        }
        if (rv != NS_ERROR_CACHE_ENTRY_DOOMED)  break;

        if (entry->IsNotInUse()) {
            // this request was the last one keeping it around, so get rid of it
            DeactivateEntry(entry);
        }
        // loop back around to look for another entry
    }

    nsICacheEntryDescriptor *descriptor = nsnull;
    
    if (NS_SUCCEEDED(rv))
        rv = entry->CreateDescriptor(request, accessGranted, &descriptor);

    // If doomedEntry is set, ActivatEntry() doomed an existing entry and
    // created a new one for that cache-key. However, any pending requests
    // on the doomed entry were not processed and we need to do that here.
    // This must be done after adding the created entry to list of active
    // entries (which is done in ActivateEntry()) otherwise the hashkeys crash
    // (see bug ##561313). It is also important to do this after creating a
    // descriptor for this request, or some other request may end up being
    // executed first for the newly created entry.
    // Finally, it is worth to emphasize that if doomedEntry is set,
    // ActivateEntry() created a new entry for the request, which will be
    // initialized by RequestAccess() and they both should have returned NS_OK.
    if (doomedEntry) {
        (void) ProcessPendingRequests(doomedEntry);
        if (doomedEntry->IsNotInUse())
            DeactivateEntry(doomedEntry);
        doomedEntry = nsnull;
    }

    if (request->mListener) {  // Asynchronous
    
        if (NS_FAILED(rv) && calledFromOpenCacheEntry)
            return rv;  // skip notifying listener, just return rv to caller
            
        // call listener to report error or descriptor
        nsresult rv2 = NotifyListener(request, descriptor, accessGranted, rv);
        if (NS_FAILED(rv2) && NS_SUCCEEDED(rv)) {
            rv = rv2;  // trigger delete request
        }
    } else {        // Synchronous
        *result = descriptor;
    }
    return rv;
}


nsresult
nsCacheService::OpenCacheEntry(nsCacheSession *           session,
                               const nsACString &         key,
                               nsCacheAccessMode          accessRequested,
                               bool                       blockingMode,
                               nsICacheListener *         listener,
                               nsICacheEntryDescriptor ** result)
{
    CACHE_LOG_DEBUG(("Opening entry for session %p, key %s, mode %d, blocking %d\n",
                     session, PromiseFlatCString(key).get(), accessRequested,
                     blockingMode));
    NS_ASSERTION(gService, "nsCacheService::gService is null.");
    if (result)
        *result = nsnull;

    if (!gService->mInitialized)
        return NS_ERROR_NOT_INITIALIZED;

    nsCacheRequest * request = nsnull;

    nsresult rv = gService->CreateRequest(session,
                                          key,
                                          accessRequested,
                                          blockingMode,
                                          listener,
                                          &request);
    if (NS_FAILED(rv))  return rv;

    CACHE_LOG_DEBUG(("Created request %p\n", request));

    // Process the request on the background thread if we are on the main thread
    // and the the request is asynchronous
    if (NS_IsMainThread() && listener && gService->mCacheIOThread) {
        nsCOMPtr<nsIRunnable> ev =
            new nsProcessRequestEvent(request);
        rv = DispatchToCacheIOThread(ev);

        // delete request if we didn't post the event
        if (NS_FAILED(rv))
            delete request;
    }
    else {

        nsCacheServiceAutoLock lock;
        rv = gService->ProcessRequest(request, PR_TRUE, result);

        // delete requests that have completed
        if (!(listener && (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION)))
            delete request;
    }

    return rv;
}


nsresult
nsCacheService::ActivateEntry(nsCacheRequest * request, 
                              nsCacheEntry ** result,
                              nsCacheEntry ** doomedEntry)
{
    CACHE_LOG_DEBUG(("Activate entry for request %p\n", request));
    
    nsresult        rv = NS_OK;

    NS_ASSERTION(request != nsnull, "ActivateEntry called with no request");
    if (result) *result = nsnull;
    if (doomedEntry) *doomedEntry = nsnull;
    if ((!request) || (!result) || (!doomedEntry))
        return NS_ERROR_NULL_POINTER;

    // check if the request can be satisfied
    if (!mEnableMemoryDevice && !request->IsStreamBased())
        return NS_ERROR_FAILURE;
    if (!IsStorageEnabledForPolicy_Locked(request->StoragePolicy()))
        return NS_ERROR_FAILURE;

    // search active entries (including those not bound to device)
    nsCacheEntry *entry = mActiveEntries.GetEntry(request->mKey);
    CACHE_LOG_DEBUG(("Active entry for request %p is %p\n", request, entry));

    if (!entry) {
        // search cache devices for entry
        bool collision = false;
        entry = SearchCacheDevices(request->mKey, request->StoragePolicy(), &collision);
        CACHE_LOG_DEBUG(("Device search for request %p returned %p\n",
                         request, entry));
        // When there is a hashkey collision just refuse to cache it...
        if (collision) return NS_ERROR_CACHE_IN_USE;

        if (entry)  entry->MarkInitialized();
    } else {
        NS_ASSERTION(entry->IsActive(), "Inactive entry found in mActiveEntries!");
    }

    if (entry) {
        ++mCacheHits;
        entry->Fetched();
    } else {
        ++mCacheMisses;
    }

    if (entry &&
        ((request->AccessRequested() == nsICache::ACCESS_WRITE) ||
         ((request->StoragePolicy() != nsICache::STORE_OFFLINE) &&
          (entry->mExpirationTime <= SecondsFromPRTime(PR_Now()) &&
           request->WillDoomEntriesIfExpired()))))

    {
        // this is FORCE-WRITE request or the entry has expired
        // we doom entry without processing pending requests, but store it in
        // doomedEntry which causes pending requests to be processed below
        rv = DoomEntry_Internal(entry, false);
        *doomedEntry = entry;
        if (NS_FAILED(rv)) {
            // XXX what to do?  Increment FailedDooms counter?
        }
        entry = nsnull;
    }

    if (!entry) {
        if (! (request->AccessRequested() & nsICache::ACCESS_WRITE)) {
            // this is a READ-ONLY request
            rv = NS_ERROR_CACHE_KEY_NOT_FOUND;
            goto error;
        }

        entry = new nsCacheEntry(request->mKey,
                                 request->IsStreamBased(),
                                 request->StoragePolicy());
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;
        
        entry->Fetched();
        ++mTotalEntries;

        // XXX  we could perform an early bind in some cases based on storage policy
    }

    if (!entry->IsActive()) {
        rv = mActiveEntries.AddEntry(entry);
        if (NS_FAILED(rv)) goto error;
        CACHE_LOG_DEBUG(("Added entry %p to mActiveEntries\n", entry));
        entry->MarkActive();  // mark entry active, because it's now in mActiveEntries
    }
    *result = entry;
    return NS_OK;
    
 error:
    *result = nsnull;
    delete entry;
    return rv;
}


nsCacheEntry *
nsCacheService::SearchCacheDevices(nsCString * key, nsCacheStoragePolicy policy, bool *collision)
{
    nsCacheEntry * entry = nsnull;

    CACHE_LOG_DEBUG(("mMemoryDevice: 0x%p\n", mMemoryDevice));

    *collision = PR_FALSE;
    if ((policy == nsICache::STORE_ANYWHERE) || (policy == nsICache::STORE_IN_MEMORY)) {
        // If there is no memory device, then there is nothing to search...
        if (mMemoryDevice) {
            entry = mMemoryDevice->FindEntry(key, collision);
            CACHE_LOG_DEBUG(("Searching mMemoryDevice for key %s found: 0x%p, "
                             "collision: %d\n", key->get(), entry, collision));
        }
    }

    if (!entry && 
        ((policy == nsICache::STORE_ANYWHERE) || (policy == nsICache::STORE_ON_DISK))) {

        if (mEnableDiskDevice) {
            if (!mDiskDevice) {
                nsresult rv = CreateDiskDevice();
                if (NS_FAILED(rv))
                    return nsnull;
            }
            
            entry = mDiskDevice->FindEntry(key, collision);
        }
    }

    if (!entry && (policy == nsICache::STORE_OFFLINE ||
                   (policy == nsICache::STORE_ANYWHERE &&
                    gIOService->IsOffline()))) {

        if (mEnableOfflineDevice) {
            if (!mOfflineDevice) {
                nsresult rv = CreateOfflineDevice();
                if (NS_FAILED(rv))
                    return nsnull;
            }

            entry = mOfflineDevice->FindEntry(key, collision);
        }
    }

    return entry;
}


nsCacheDevice *
nsCacheService::EnsureEntryHasDevice(nsCacheEntry * entry)
{
    nsCacheDevice * device = entry->CacheDevice();
    // return device if found, possibly null if the entry is doomed i.e prevent
    // doomed entries to bind to a device (see e.g. bugs #548406 and #596443)
    if (device || entry->IsDoomed())  return device;

    PRInt64 predictedDataSize = entry->PredictedDataSize();
    if (entry->IsStreamData() && entry->IsAllowedOnDisk() && mEnableDiskDevice) {
        // this is the default
        if (!mDiskDevice) {
            (void)CreateDiskDevice();  // ignore the error (check for mDiskDevice instead)
        }

        if (mDiskDevice) {
            // Bypass the cache if Content-Length says the entry will be too big
            if (predictedDataSize != -1 &&
                entry->StoragePolicy() != nsICache::STORE_ON_DISK_AS_FILE &&
                mDiskDevice->EntryIsTooBig(predictedDataSize)) {
                DebugOnly<nsresult> rv = nsCacheService::DoomEntry(entry);
                NS_ASSERTION(NS_SUCCEEDED(rv),"DoomEntry() failed.");
                return nsnull;
            }

            entry->MarkBinding();  // enter state of binding
            nsresult rv = mDiskDevice->BindEntry(entry);
            entry->ClearBinding(); // exit state of binding
            if (NS_SUCCEEDED(rv))
                device = mDiskDevice;
        }
    }

    // if we can't use mDiskDevice, try mMemoryDevice
    if (!device && mEnableMemoryDevice && entry->IsAllowedInMemory()) {        
        if (!mMemoryDevice) {
            (void)CreateMemoryDevice();  // ignore the error (check for mMemoryDevice instead)
        }
        if (mMemoryDevice) {
            // Bypass the cache if Content-Length says entry will be too big
            if (predictedDataSize != -1 &&
                mMemoryDevice->EntryIsTooBig(predictedDataSize)) {
                DebugOnly<nsresult> rv = nsCacheService::DoomEntry(entry);
                NS_ASSERTION(NS_SUCCEEDED(rv),"DoomEntry() failed.");
                return nsnull;
            }

            entry->MarkBinding();  // enter state of binding
            nsresult rv = mMemoryDevice->BindEntry(entry);
            entry->ClearBinding(); // exit state of binding
            if (NS_SUCCEEDED(rv))
                device = mMemoryDevice;
        }
    }

    if (!device && entry->IsStreamData() &&
        entry->IsAllowedOffline() && mEnableOfflineDevice) {
        if (!mOfflineDevice) {
            (void)CreateOfflineDevice(); // ignore the error (check for mOfflineDevice instead)
        }

        if (mOfflineDevice) {
            entry->MarkBinding();
            nsresult rv = mOfflineDevice->BindEntry(entry);
            entry->ClearBinding();
            if (NS_SUCCEEDED(rv))
                device = mOfflineDevice;
        }
    }

    if (device) 
        entry->SetCacheDevice(device);
    return device;
}

PRInt64
nsCacheService::MemoryDeviceSize()
{
    nsMemoryCacheDevice *memoryDevice = GlobalInstance()->mMemoryDevice;
    return memoryDevice ? memoryDevice->TotalSize() : 0;
}

nsresult
nsCacheService::DoomEntry(nsCacheEntry * entry)
{
    return gService->DoomEntry_Internal(entry, true);
}


nsresult
nsCacheService::DoomEntry_Internal(nsCacheEntry * entry,
                                   bool doProcessPendingRequests)
{
    if (entry->IsDoomed())  return NS_OK;
    
    CACHE_LOG_DEBUG(("Dooming entry %p\n", entry));
    nsresult  rv = NS_OK;
    entry->MarkDoomed();
    
    NS_ASSERTION(!entry->IsBinding(), "Dooming entry while binding device.");
    nsCacheDevice * device = entry->CacheDevice();
    if (device)  device->DoomEntry(entry);

    if (entry->IsActive()) {
        // remove from active entries
        mActiveEntries.RemoveEntry(entry);
        CACHE_LOG_DEBUG(("Removed entry %p from mActiveEntries\n", entry));
        entry->MarkInactive();
     }

    // put on doom list to wait for descriptors to close
    NS_ASSERTION(PR_CLIST_IS_EMPTY(entry), "doomed entry still on device list");
    PR_APPEND_LINK(entry, &mDoomedEntries);

    // handle pending requests only if we're supposed to
    if (doProcessPendingRequests) {
        // tell pending requests to get on with their lives...
        rv = ProcessPendingRequests(entry);

        // All requests have been removed, but there may still be open descriptors
        if (entry->IsNotInUse()) {
            DeactivateEntry(entry); // tell device to get rid of it
        }
    }
    return rv;
}


void
nsCacheService::OnProfileShutdown(bool cleanse)
{
    if (!gService)  return;
    if (!gService->mInitialized) {
        // The cache service has been shut down, but someone is still holding
        // a reference to it. Ignore this call.
        return;
    }
    nsCacheServiceAutoLock lock;

    gService->DoomActiveEntries();
    gService->ClearDoomList();

    // Make sure to wait for any pending cache-operations before
    // proceeding with destructive actions (bug #620660)
    (void) SyncWithCacheIOThread();

    if (gService->mDiskDevice && gService->mEnableDiskDevice) {
        if (cleanse)
            gService->mDiskDevice->EvictEntries(nsnull);

        gService->mDiskDevice->Shutdown();
    }
    gService->mEnableDiskDevice = PR_FALSE;

    if (gService->mOfflineDevice && gService->mEnableOfflineDevice) {
        if (cleanse)
            gService->mOfflineDevice->EvictEntries(nsnull);

        gService->mOfflineDevice->Shutdown();
    }
    gService->mEnableOfflineDevice = PR_FALSE;

    if (gService->mMemoryDevice) {
        // clear memory cache
        gService->mMemoryDevice->EvictEntries(nsnull);
    }

}


void
nsCacheService::OnProfileChanged()
{
    if (!gService)  return;

    CACHE_LOG_DEBUG(("nsCacheService::OnProfileChanged"));
 
    nsCacheServiceAutoLock lock;
    
    gService->mEnableDiskDevice    = gService->mObserver->DiskCacheEnabled();
    gService->mEnableOfflineDevice = gService->mObserver->OfflineCacheEnabled();
    gService->mEnableMemoryDevice  = gService->mObserver->MemoryCacheEnabled();

    if (gService->mDiskDevice) {
        gService->mDiskDevice->SetCacheParentDirectory(gService->mObserver->DiskCacheParentDirectory());
        gService->mDiskDevice->SetCapacity(gService->mObserver->DiskCacheCapacity());

        // XXX initialization of mDiskDevice could be made lazily, if mEnableDiskDevice is false
        nsresult rv = gService->mDiskDevice->Init();
        if (NS_FAILED(rv)) {
            NS_ERROR("nsCacheService::OnProfileChanged: Re-initializing disk device failed");
            gService->mEnableDiskDevice = PR_FALSE;
            // XXX delete mDiskDevice?
        }
    }

    if (gService->mOfflineDevice) {
        gService->mOfflineDevice->SetCacheParentDirectory(gService->mObserver->OfflineCacheParentDirectory());
        gService->mOfflineDevice->SetCapacity(gService->mObserver->OfflineCacheCapacity());

        // XXX initialization of mOfflineDevice could be made lazily, if mEnableOfflineDevice is false
        nsresult rv = gService->mOfflineDevice->Init();
        if (NS_FAILED(rv)) {
            NS_ERROR("nsCacheService::OnProfileChanged: Re-initializing offline device failed");
            gService->mEnableOfflineDevice = PR_FALSE;
            // XXX delete mOfflineDevice?
        }
    }

    // If memoryDevice exists, reset its size to the new profile
    if (gService->mMemoryDevice) {
        if (gService->mEnableMemoryDevice) {
            // make sure that capacity is reset to the right value
            PRInt32 capacity = gService->mObserver->MemoryCacheCapacity();
            CACHE_LOG_DEBUG(("Resetting memory device capacity to %d\n",
                             capacity));
            gService->mMemoryDevice->SetCapacity(capacity);
        } else {
            // tell memory device to evict everything
            CACHE_LOG_DEBUG(("memory device disabled\n"));
            gService->mMemoryDevice->SetCapacity(0);
            // Don't delete memory device, because some entries may be active still...
        }
    }
}


void
nsCacheService::SetDiskCacheEnabled(bool    enabled)
{
    if (!gService)  return;
    nsCacheServiceAutoLock lock;
    gService->mEnableDiskDevice = enabled;
}


void
nsCacheService::SetDiskCacheCapacity(PRInt32  capacity)
{
    if (!gService)  return;
    nsCacheServiceAutoLock lock;

    if (gService->mDiskDevice) {
        gService->mDiskDevice->SetCapacity(capacity);
    }

    if (gService->mObserver)
        gService->mEnableDiskDevice = gService->mObserver->DiskCacheEnabled();
}

void
nsCacheService::SetDiskCacheMaxEntrySize(PRInt32  maxSize)
{
    if (!gService)  return;
    nsCacheServiceAutoLock lock;

    if (gService->mDiskDevice) {
        gService->mDiskDevice->SetMaxEntrySize(maxSize);
    }
}

void
nsCacheService::SetMemoryCacheMaxEntrySize(PRInt32  maxSize)
{
    if (!gService)  return;
    nsCacheServiceAutoLock lock;

    if (gService->mMemoryDevice) {
        gService->mMemoryDevice->SetMaxEntrySize(maxSize);
    }
}

void
nsCacheService::SetOfflineCacheEnabled(bool    enabled)
{
    if (!gService)  return;
    nsCacheServiceAutoLock lock;
    gService->mEnableOfflineDevice = enabled;
}

void
nsCacheService::SetOfflineCacheCapacity(PRInt32  capacity)
{
    if (!gService)  return;
    nsCacheServiceAutoLock lock;

    if (gService->mOfflineDevice) {
        gService->mOfflineDevice->SetCapacity(capacity);
    }

    gService->mEnableOfflineDevice = gService->mObserver->OfflineCacheEnabled();
}


void
nsCacheService::SetMemoryCache()
{
    if (!gService)  return;

    CACHE_LOG_DEBUG(("nsCacheService::SetMemoryCache"));

    nsCacheServiceAutoLock lock;

    gService->mEnableMemoryDevice = gService->mObserver->MemoryCacheEnabled();

    if (gService->mEnableMemoryDevice) {
        if (gService->mMemoryDevice) {
            PRInt32 capacity = gService->mObserver->MemoryCacheCapacity();
            // make sure that capacity is reset to the right value
            CACHE_LOG_DEBUG(("Resetting memory device capacity to %d\n",
                             capacity));
            gService->mMemoryDevice->SetCapacity(capacity);
        }
    } else {
        if (gService->mMemoryDevice) {
            // tell memory device to evict everything
            CACHE_LOG_DEBUG(("memory device disabled\n"));
            gService->mMemoryDevice->SetCapacity(0);
            // Don't delete memory device, because some entries may be active still...
        }
    }
}


/******************************************************************************
 * static methods for nsCacheEntryDescriptor
 *****************************************************************************/
void
nsCacheService::CloseDescriptor(nsCacheEntryDescriptor * descriptor)
{
    // ask entry to remove descriptor
    nsCacheEntry * entry       = descriptor->CacheEntry();
    bool           stillActive = entry->RemoveDescriptor(descriptor);
    nsresult       rv          = NS_OK;

    if (!entry->IsValid()) {
        rv = gService->ProcessPendingRequests(entry);
    }

    if (!stillActive) {
        gService->DeactivateEntry(entry);
    }
}


nsresult        
nsCacheService::GetFileForEntry(nsCacheEntry *         entry,
                                nsIFile **             result)
{
    nsCacheDevice * device = gService->EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED;
    
    return device->GetFileForEntry(entry, result);
}


nsresult
nsCacheService::OpenInputStreamForEntry(nsCacheEntry *     entry,
                                        nsCacheAccessMode  mode,
                                        PRUint32           offset,
                                        nsIInputStream  ** result)
{
    nsCacheDevice * device = gService->EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED;

    return device->OpenInputStreamForEntry(entry, mode, offset, result);
}

nsresult
nsCacheService::OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                         nsCacheAccessMode  mode,
                                         PRUint32           offset,
                                         nsIOutputStream ** result)
{
    nsCacheDevice * device = gService->EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED;

    return device->OpenOutputStreamForEntry(entry, mode, offset, result);
}


nsresult
nsCacheService::OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize)
{
    nsCacheDevice * device = gService->EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED;

    return device->OnDataSizeChange(entry, deltaSize);
}

void
nsCacheService::Lock()
{
    gService->mLock.Lock();
}

void
nsCacheService::Unlock()
{
    gService->mLock.AssertCurrentThreadOwns();

    nsTArray<nsISupports*> doomed;
    doomed.SwapElements(gService->mDoomedObjects);

    gService->mLock.Unlock();

    for (PRUint32 i = 0; i < doomed.Length(); ++i)
        doomed[i]->Release();
}

void
nsCacheService::ReleaseObject_Locked(nsISupports * obj,
                                     nsIEventTarget * target)
{
    gService->mLock.AssertCurrentThreadOwns();

    bool isCur;
    if (!target || (NS_SUCCEEDED(target->IsOnCurrentThread(&isCur)) && isCur)) {
        gService->mDoomedObjects.AppendElement(obj);
    } else {
        NS_ProxyRelease(target, obj);
    }
}


nsresult
nsCacheService::SetCacheElement(nsCacheEntry * entry, nsISupports * element)
{
    entry->SetData(element);
    entry->TouchData();
    return NS_OK;
}


nsresult
nsCacheService::ValidateEntry(nsCacheEntry * entry)
{
    nsCacheDevice * device = gService->EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED;

    entry->MarkValid();
    nsresult rv = gService->ProcessPendingRequests(entry);
    NS_ASSERTION(rv == NS_OK, "ProcessPendingRequests failed.");
    // XXX what else should be done?

    return rv;
}


void
nsCacheService::DeactivateEntry(nsCacheEntry * entry)
{
    CACHE_LOG_DEBUG(("Deactivating entry %p\n", entry));
    nsresult  rv = NS_OK;
    NS_ASSERTION(entry->IsNotInUse(), "### deactivating an entry while in use!");
    nsCacheDevice * device = nsnull;

    if (mMaxDataSize < entry->DataSize() )     mMaxDataSize = entry->DataSize();
    if (mMaxMetaSize < entry->MetaDataSize() ) mMaxMetaSize = entry->MetaDataSize();

    if (entry->IsDoomed()) {
        // remove from Doomed list
        PR_REMOVE_AND_INIT_LINK(entry);
    } else if (entry->IsActive()) {
        // remove from active entries
        mActiveEntries.RemoveEntry(entry);
        CACHE_LOG_DEBUG(("Removed deactivated entry %p from mActiveEntries\n",
                         entry));
        entry->MarkInactive();

        // bind entry if necessary to store meta-data
        device = EnsureEntryHasDevice(entry); 
        if (!device) {
            CACHE_LOG_DEBUG(("DeactivateEntry: unable to bind active "
                             "entry %p\n",
                             entry));
            NS_WARNING("DeactivateEntry: unable to bind active entry\n");
            return;
        }
    } else {
        // if mInitialized == PR_FALSE,
        // then we're shutting down and this state is okay.
        NS_ASSERTION(!mInitialized, "DeactivateEntry: bad cache entry state.");
    }

    device = entry->CacheDevice();
    if (device) {
        rv = device->DeactivateEntry(entry);
        if (NS_FAILED(rv)) {
            // increment deactivate failure count
            ++mDeactivateFailures;
        }
    } else {
        // increment deactivating unbound entry statistic
        ++mDeactivatedUnboundEntries;
        delete entry; // because no one else will
    }
}


nsresult
nsCacheService::ProcessPendingRequests(nsCacheEntry * entry)
{
    nsresult            rv = NS_OK;
    nsCacheRequest *    request = (nsCacheRequest *)PR_LIST_HEAD(&entry->mRequestQ);
    nsCacheRequest *    nextRequest;
    bool                newWriter = false;
    
    CACHE_LOG_DEBUG(("ProcessPendingRequests for %sinitialized %s %salid entry %p\n",
                    (entry->IsInitialized()?"" : "Un"),
                    (entry->IsDoomed()?"DOOMED" : ""),
                    (entry->IsValid()? "V":"Inv"), entry));

    if (request == &entry->mRequestQ)  return NS_OK;    // no queued requests

    if (!entry->IsDoomed() && entry->IsInvalid()) {
        // 1st descriptor closed w/o MarkValid()
        NS_ASSERTION(PR_CLIST_IS_EMPTY(&entry->mDescriptorQ), "shouldn't be here with open descriptors");

#if DEBUG
        // verify no ACCESS_WRITE requests(shouldn't have any of these)
        while (request != &entry->mRequestQ) {
            NS_ASSERTION(request->AccessRequested() != nsICache::ACCESS_WRITE,
                         "ACCESS_WRITE request should have been given a new entry");
            request = (nsCacheRequest *)PR_NEXT_LINK(request);
        }
        request = (nsCacheRequest *)PR_LIST_HEAD(&entry->mRequestQ);        
#endif
        // find first request with ACCESS_READ_WRITE (if any) and promote it to 1st writer
        while (request != &entry->mRequestQ) {
            if (request->AccessRequested() == nsICache::ACCESS_READ_WRITE) {
                newWriter = PR_TRUE;
                CACHE_LOG_DEBUG(("  promoting request %p to 1st writer\n", request));
                break;
            }

            request = (nsCacheRequest *)PR_NEXT_LINK(request);
        }
        
        if (request == &entry->mRequestQ)   // no requests asked for ACCESS_READ_WRITE, back to top
            request = (nsCacheRequest *)PR_LIST_HEAD(&entry->mRequestQ);
        
        // XXX what should we do if there are only READ requests in queue?
        // XXX serialize their accesses, give them only read access, but force them to check validate flag?
        // XXX or do readers simply presume the entry is valid
        // See fix for bug #467392 below
    }

    nsCacheAccessMode  accessGranted = nsICache::ACCESS_NONE;

    while (request != &entry->mRequestQ) {
        nextRequest = (nsCacheRequest *)PR_NEXT_LINK(request);
        CACHE_LOG_DEBUG(("  %sync request %p for %p\n",
                        (request->mListener?"As":"S"), request, entry));

        if (request->mListener) {

            // Async request
            PR_REMOVE_AND_INIT_LINK(request);

            if (entry->IsDoomed()) {
                rv = ProcessRequest(request, PR_FALSE, nsnull);
                if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION)
                    rv = NS_OK;
                else
                    delete request;

                if (NS_FAILED(rv)) {
                    // XXX what to do?
                }
            } else if (entry->IsValid() || newWriter) {
                rv = entry->RequestAccess(request, &accessGranted);
                NS_ASSERTION(NS_SUCCEEDED(rv),
                             "if entry is valid, RequestAccess must succeed.");
                // XXX if (newWriter)  NS_ASSERTION( accessGranted == request->AccessRequested(), "why not?");

                // entry->CreateDescriptor dequeues request, and queues descriptor
                nsICacheEntryDescriptor *descriptor = nsnull;
                rv = entry->CreateDescriptor(request,
                                             accessGranted,
                                             &descriptor);

                // post call to listener to report error or descriptor
                rv = NotifyListener(request, descriptor, accessGranted, rv);
                delete request;
                if (NS_FAILED(rv)) {
                    // XXX what to do?
                }
                
            } else {
                // read-only request to an invalid entry - need to wait for
                // the entry to become valid so we post an event to process
                // the request again later (bug #467392)
                nsCOMPtr<nsIRunnable> ev =
                    new nsProcessRequestEvent(request);
                rv = DispatchToCacheIOThread(ev);
                if (NS_FAILED(rv)) {
                    delete request; // avoid leak
                }
            }
        } else {

            // Synchronous request
            request->WakeUp();
        }
        if (newWriter)  break;  // process remaining requests after validation
        request = nextRequest;
    }

    return NS_OK;
}


void
nsCacheService::ClearPendingRequests(nsCacheEntry * entry)
{
    nsCacheRequest * request = (nsCacheRequest *)PR_LIST_HEAD(&entry->mRequestQ);
    
    while (request != &entry->mRequestQ) {
        nsCacheRequest * next = (nsCacheRequest *)PR_NEXT_LINK(request);

        // XXX we're just dropping these on the floor for now...definitely wrong.
        PR_REMOVE_AND_INIT_LINK(request);
        delete request;
        request = next;
    }
}


void
nsCacheService::ClearDoomList()
{
    nsCacheEntry * entry = (nsCacheEntry *)PR_LIST_HEAD(&mDoomedEntries);

    while (entry != &mDoomedEntries) {
        nsCacheEntry * next = (nsCacheEntry *)PR_NEXT_LINK(entry);
        
         entry->DetachDescriptors();
         DeactivateEntry(entry);
         entry = next;
    }        
}


void
nsCacheService::ClearActiveEntries()
{
    mActiveEntries.VisitEntries(DeactivateAndClearEntry, nsnull);
    mActiveEntries.Shutdown();
}


PLDHashOperator
nsCacheService::DeactivateAndClearEntry(PLDHashTable *    table,
                                        PLDHashEntryHdr * hdr,
                                        PRUint32          number,
                                        void *            arg)
{
    nsCacheEntry * entry = ((nsCacheEntryHashTableEntry *)hdr)->cacheEntry;
    NS_ASSERTION(entry, "### active entry = nsnull!");
    // only called from Shutdown() so we don't worry about pending requests
    gService->ClearPendingRequests(entry);
    entry->DetachDescriptors();
    
    entry->MarkInactive();  // so we don't call Remove() while we're enumerating
    gService->DeactivateEntry(entry);
    
    return PL_DHASH_REMOVE; // and continue enumerating
}


void
nsCacheService::DoomActiveEntries()
{
    nsAutoTArray<nsCacheEntry*, 8> array;

    mActiveEntries.VisitEntries(RemoveActiveEntry, &array);

    PRUint32 count = array.Length();
    for (PRUint32 i=0; i < count; ++i)
        DoomEntry_Internal(array[i], true);
}


PLDHashOperator
nsCacheService::RemoveActiveEntry(PLDHashTable *    table,
                                  PLDHashEntryHdr * hdr,
                                  PRUint32          number,
                                  void *            arg)
{
    nsCacheEntry * entry = ((nsCacheEntryHashTableEntry *)hdr)->cacheEntry;
    NS_ASSERTION(entry, "### active entry = nsnull!");

    nsTArray<nsCacheEntry*> * array = (nsTArray<nsCacheEntry*> *) arg;
    NS_ASSERTION(array, "### array = nsnull!");
    array->AppendElement(entry);

    // entry is being removed from the active entry list
    entry->MarkInactive();
    return PL_DHASH_REMOVE; // and continue enumerating
}


#if defined(PR_LOGGING)
void
nsCacheService::LogCacheStatistics()
{
    PRUint32 hitPercentage = (PRUint32)((((double)mCacheHits) /
        ((double)(mCacheHits + mCacheMisses))) * 100);
    CACHE_LOG_ALWAYS(("\nCache Service Statistics:\n\n"));
    CACHE_LOG_ALWAYS(("    TotalEntries   = %d\n", mTotalEntries));
    CACHE_LOG_ALWAYS(("    Cache Hits     = %d\n", mCacheHits));
    CACHE_LOG_ALWAYS(("    Cache Misses   = %d\n", mCacheMisses));
    CACHE_LOG_ALWAYS(("    Cache Hit %%    = %d%%\n", hitPercentage));
    CACHE_LOG_ALWAYS(("    Max Key Length = %d\n", mMaxKeyLength));
    CACHE_LOG_ALWAYS(("    Max Meta Size  = %d\n", mMaxMetaSize));
    CACHE_LOG_ALWAYS(("    Max Data Size  = %d\n", mMaxDataSize));
    CACHE_LOG_ALWAYS(("\n"));
    CACHE_LOG_ALWAYS(("    Deactivate Failures         = %d\n",
                      mDeactivateFailures));
    CACHE_LOG_ALWAYS(("    Deactivated Unbound Entries = %d\n",
                      mDeactivatedUnboundEntries));
}
#endif


void
nsCacheService::OnEnterExitPrivateBrowsing()
{
    if (!gService)  return;
    nsCacheServiceAutoLock lock;

    gService->DoomActiveEntries();

    if (gService->mMemoryDevice) {
        // clear memory cache
        gService->mMemoryDevice->EvictEntries(nsnull);
    }
}
