/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      Added PR_CALLBACK for Optlink use in OS2
 */

/* Allow logging in the release build */
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "nsNativeModuleLoader.h"

#include "prlog.h"
#include "prinit.h"
#include "prerror.h"

#include "nsComponentManager.h"
#include "ManifestParser.h" // for LogMessage
#include "nsCRTGlue.h"
#include "nsThreadUtils.h"
#include "nsTraceRefcnt.h"

#include "nsIFile.h"
#include "mozilla/WindowsDllBlocklist.h"

#ifdef XP_WIN
#include <windows.h>
#endif

#ifdef XP_MACOSX
#include <signal.h>
#endif

#ifdef VMS
#include <lib$routines.h>
#include <ssdef.h>
#endif

#ifdef DEBUG
#define IMPLEMENT_BREAK_AFTER_LOAD
#endif

using namespace mozilla;

static PRLogModuleInfo *
GetNativeModuleLoaderLog()
{
    static PRLogModuleInfo *sLog;
    if (!sLog)
        sLog = PR_NewLogModule("nsNativeModuleLoader");
    return sLog;
}

#define LOG(level, args) PR_LOG(GetNativeModuleLoaderLog(), level, args)

nsresult
nsNativeModuleLoader::Init()
{
    MOZ_ASSERT(NS_IsMainThread(), "Startup not on main thread?");
    LOG(PR_LOG_DEBUG, ("nsNativeModuleLoader::Init()"));
    return NS_OK;
}

class LoadModuleMainThreadRunnable : public nsRunnable
{
public:
    LoadModuleMainThreadRunnable(nsNativeModuleLoader* aLoader,
                                 FileLocation &aFile)
        : mManager(nsComponentManagerImpl::gComponentManager)
        , mLoader(aLoader)
        , mFile(aFile)
        , mResult(nullptr)
    { }

    NS_IMETHOD Run()
    {
        mResult = mLoader->LoadModule(mFile);
        return NS_OK;
    }

    nsRefPtr<nsComponentManagerImpl> mManager;
    nsNativeModuleLoader* mLoader;
    FileLocation mFile;
    const mozilla::Module* mResult;
};

const mozilla::Module*
nsNativeModuleLoader::LoadModule(FileLocation &aFile)
{
    if (aFile.IsZip()) {
        NS_ERROR("Binary components cannot be loaded from JARs");
        return nullptr;
    }
    nsCOMPtr<nsIFile> file = aFile.GetBaseFile();
    nsresult rv;

    if (!NS_IsMainThread()) {
        // If this call is off the main thread, synchronously proxy it
        // to the main thread.
        nsRefPtr<LoadModuleMainThreadRunnable> r = new LoadModuleMainThreadRunnable(this, aFile);
        NS_DispatchToMainThread(r, NS_DISPATCH_SYNC);
        return r->mResult;
    }

    nsCOMPtr<nsIHashable> hashedFile(do_QueryInterface(file));
    if (!hashedFile) {
        NS_ERROR("nsIFile is not nsIHashable");
        return nullptr;
    }

    nsAutoCString filePath;
    file->GetNativePath(filePath);

    NativeLoadData data;

    if (mLibraries.Get(hashedFile, &data)) {
        NS_ASSERTION(data.mModule, "Corrupt mLibraries hash");
        LOG(PR_LOG_DEBUG,
            ("nsNativeModuleLoader::LoadModule(\"%s\") - found in cache",
             filePath.get()));
        return data.mModule;
    }

    // We haven't loaded this module before
    {
#ifdef HAS_DLL_BLOCKLIST
      AutoSetXPCOMLoadOnMainThread guard;
#endif
      rv = file->Load(&data.mLibrary);
    }

    if (NS_FAILED(rv)) {
        char errorMsg[1024] = "<unknown; can't get error from NSPR>";

        if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
            PR_GetErrorText(errorMsg);

        LogMessage("Failed to load native module at path '%s': (%lx) %s",
                   filePath.get(), rv, errorMsg);

        return nullptr;
    }

#ifdef IMPLEMENT_BREAK_AFTER_LOAD
    nsAutoCString leafName;
    file->GetNativeLeafName(leafName);

    char *env = getenv("XPCOM_BREAK_ON_LOAD");
    char *blist;
    if (env && *env && (blist = strdup(env))) {
        char *nextTok = blist;
        while (char *token = NS_strtok(":", &nextTok)) {
            if (leafName.Find(token, true) != kNotFound) {
                NS_BREAK();
            }
        }

        free(blist);
    }
#endif

    void *module = PR_FindSymbol(data.mLibrary, "NSModule");
    if (!module) {
        LogMessage("Native module at path '%s' doesn't export symbol `NSModule`.",
                   filePath.get());
        PR_UnloadLibrary(data.mLibrary);
        return nullptr;
    }

    data.mModule = *(mozilla::Module const *const *) module;
    if (mozilla::Module::kVersion != data.mModule->mVersion) {
        LogMessage("Native module at path '%s' is incompatible with this version of Firefox, has version %i, expected %i.",
                   filePath.get(), data.mModule->mVersion,
                   mozilla::Module::kVersion);
        PR_UnloadLibrary(data.mLibrary);
        return nullptr;
    }

    mLibraries.Put(hashedFile, data); // infallible
    return data.mModule;
}

PLDHashOperator
nsNativeModuleLoader::ReleaserFunc(nsIHashable* aHashedFile,
                                   NativeLoadData& aLoadData, void*)
{
    aLoadData.mModule = nullptr;
    return PL_DHASH_NEXT;
}

PLDHashOperator
nsNativeModuleLoader::UnloaderFunc(nsIHashable* aHashedFile,
                                   NativeLoadData& aLoadData, void*)
{
    if (PR_LOG_TEST(GetNativeModuleLoaderLog(), PR_LOG_DEBUG)) {
        nsCOMPtr<nsIFile> file(do_QueryInterface(aHashedFile));

        nsAutoCString filePath;
        file->GetNativePath(filePath);

        LOG(PR_LOG_DEBUG,
            ("nsNativeModuleLoader::UnloaderFunc(\"%s\")", filePath.get()));
    }

#ifdef NS_BUILD_REFCNT_LOGGING
    nsTraceRefcnt::SetActivityIsLegal(false);
#endif

#if 0
    // XXXbsmedberg: do this as soon as the static-destructor crash(es)
    // are fixed
    PRStatus ret = PR_UnloadLibrary(aLoadData.mLibrary);
    NS_ASSERTION(ret == PR_SUCCESS, "Failed to unload library");
#endif

#ifdef NS_BUILD_REFCNT_LOGGING
    nsTraceRefcnt::SetActivityIsLegal(true);
#endif

    return PL_DHASH_REMOVE;
}

void
nsNativeModuleLoader::UnloadLibraries()
{
    MOZ_ASSERT(NS_IsMainThread(), "Shutdown not on main thread?");
    mLibraries.Enumerate(ReleaserFunc, nullptr);
    mLibraries.Enumerate(UnloaderFunc, nullptr);
}
