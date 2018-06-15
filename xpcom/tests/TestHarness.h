/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test harness for XPCOM objects, providing a scoped XPCOM initializer,
 * nsCOMPtr, nsRefPtr, do_CreateInstance, do_GetService, ns(Auto|C|)String,
 * and stdio.h/stdlib.h.
 */

#ifndef TestHarness_h__
#define TestHarness_h__

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"

#include "prenv.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIDirectoryService.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static uint32_t gFailCount = 0;

/**
 * Prints the given failure message and arguments using printf, prepending
 * "TEST-UNEXPECTED-FAIL " for the benefit of the test harness and
 * appending "\n" to eliminate having to type it at each call site.
 */
MOZ_FORMAT_PRINTF(1, 2) void fail(const char* msg, ...)
{
  va_list ap;

  printf("TEST-UNEXPECTED-FAIL | ");

  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);

  putchar('\n');
  ++gFailCount;
}

/**
 * Prints the given success message and arguments using printf, prepending
 * "TEST-PASS " for the benefit of the test harness and
 * appending "\n" to eliminate having to type it at each call site.
 */
MOZ_FORMAT_PRINTF(1, 2) void passed(const char* msg, ...)
{
  va_list ap;

  printf("TEST-PASS | ");

  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);

  putchar('\n');
}

//-----------------------------------------------------------------------------

class ScopedXPCOM : public nsIDirectoryServiceProvider2
{
  public:
    NS_DECL_ISUPPORTS

    explicit ScopedXPCOM(const char* testName,
                         nsIDirectoryServiceProvider *dirSvcProvider = nullptr)
    : mServMgr(nullptr)
    , mDirSvcProvider(dirSvcProvider)
    {
      mTestName = testName;
      printf("Running %s tests...\n", mTestName);

      nsresult rv = NS_InitXPCOM2(&mServMgr, nullptr, this);
      if (NS_FAILED(rv))
      {
        fail("NS_InitXPCOM2 returned failure code 0x%" PRIx32, static_cast<uint32_t>(rv));
        mServMgr = nullptr;
        return;
      }
    }

    ~ScopedXPCOM()
    {
      // If we created a profile directory, we need to remove it.
      if (mProfD) {
        nsCOMPtr<nsIObserverService> os =
          do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
        MOZ_RELEASE_ASSERT(os);
        MOZ_ALWAYS_SUCCEEDS(os->NotifyObservers(nullptr, "profile-change-net-teardown", nullptr));
        MOZ_ALWAYS_SUCCEEDS(os->NotifyObservers(nullptr, "profile-change-teardown", nullptr));
        MOZ_ALWAYS_SUCCEEDS(os->NotifyObservers(nullptr, "profile-before-change", nullptr));
        MOZ_ALWAYS_SUCCEEDS(os->NotifyObservers(nullptr, "profile-before-change-qm", nullptr));
        MOZ_ALWAYS_SUCCEEDS(os->NotifyObservers(nullptr, "profile-before-change-telemetry", nullptr));

        if (NS_FAILED(mProfD->Remove(true))) {
          NS_WARNING("Problem removing profile directory");
        }

        mProfD = nullptr;
      }

      if (mServMgr)
      {
        NS_RELEASE(mServMgr);
        nsresult rv = NS_ShutdownXPCOM(nullptr);
        if (NS_FAILED(rv))
        {
          fail("XPCOM shutdown failed with code 0x%" PRIx32, static_cast<uint32_t>(rv));
          exit(1);
        }
      }

      printf("Finished running %s tests.\n", mTestName);
    }

    bool failed()
    {
      return mServMgr == nullptr;
    }

    already_AddRefed<nsIFile> GetProfileDirectory()
    {
      if (mProfD) {
        nsCOMPtr<nsIFile> copy = mProfD;
        return copy.forget();
      }

      // Create a unique temporary folder to use for this test.
      // Note that runcppunittests.py will run tests with a temp
      // directory as the cwd, so just put something under that.
      nsCOMPtr<nsIFile> profD;
      nsresult rv = NS_GetSpecialDirectory(NS_OS_CURRENT_PROCESS_DIR,
                                           getter_AddRefs(profD));
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = profD->Append(NS_LITERAL_STRING("cpp-unit-profd"));
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = profD->CreateUnique(nsIFile::DIRECTORY_TYPE, 0755);
      NS_ENSURE_SUCCESS(rv, nullptr);

      mProfD = profD;
      return profD.forget();
    }

    already_AddRefed<nsIFile> GetGREDirectory()
    {
      if (mGRED) {
        nsCOMPtr<nsIFile> copy = mGRED;
        return copy.forget();
      }

      char* env = PR_GetEnv("MOZ_XRE_DIR");
      nsCOMPtr<nsIFile> greD;
      if (env) {
        NS_NewLocalFile(NS_ConvertUTF8toUTF16(env), false,
                        getter_AddRefs(greD));
      }

      mGRED = greD;
      return greD.forget();
    }

    already_AddRefed<nsIFile> GetGREBinDirectory()
    {
      if (mGREBinD) {
        nsCOMPtr<nsIFile> copy = mGREBinD;
        return copy.forget();
      }

      nsCOMPtr<nsIFile> greD = GetGREDirectory();
      if (!greD) {
        return greD.forget();
      }
      greD->Clone(getter_AddRefs(mGREBinD));

#ifdef XP_MACOSX
      nsAutoCString leafName;
      mGREBinD->GetNativeLeafName(leafName);
      if (leafName.EqualsLiteral("Resources")) {
        mGREBinD->SetNativeLeafName(NS_LITERAL_CSTRING("MacOS"));
      }
#endif

      nsCOMPtr<nsIFile> copy = mGREBinD;
      return copy.forget();
    }

    ////////////////////////////////////////////////////////////////////////////
    //// nsIDirectoryServiceProvider

    NS_IMETHOD GetFile(const char *aProperty, bool *_persistent,
                       nsIFile **_result) override
    {
      // If we were supplied a directory service provider, ask it first.
      if (mDirSvcProvider &&
          NS_SUCCEEDED(mDirSvcProvider->GetFile(aProperty, _persistent,
                                                _result))) {
        return NS_OK;
      }

      // Otherwise, the test harness provides some directories automatically.
      if (0 == strcmp(aProperty, NS_APP_USER_PROFILE_50_DIR) ||
          0 == strcmp(aProperty, NS_APP_USER_PROFILE_LOCAL_50_DIR) ||
          0 == strcmp(aProperty, NS_APP_PROFILE_LOCAL_DIR_STARTUP)) {
        nsCOMPtr<nsIFile> profD = GetProfileDirectory();
        NS_ENSURE_TRUE(profD, NS_ERROR_FAILURE);

        nsCOMPtr<nsIFile> clone;
        nsresult rv = profD->Clone(getter_AddRefs(clone));
        NS_ENSURE_SUCCESS(rv, rv);

        *_persistent = true;
        clone.forget(_result);
        return NS_OK;
      } else if (0 == strcmp(aProperty, NS_GRE_DIR)) {
        nsCOMPtr<nsIFile> greD = GetGREDirectory();
        NS_ENSURE_TRUE(greD, NS_ERROR_FAILURE);

        *_persistent = true;
        greD.forget(_result);
        return NS_OK;
      } else if (0 == strcmp(aProperty, NS_GRE_BIN_DIR)) {
        nsCOMPtr<nsIFile> greBinD = GetGREBinDirectory();
        NS_ENSURE_TRUE(greBinD, NS_ERROR_FAILURE);

        *_persistent = true;
        greBinD.forget(_result);
        return NS_OK;
      }

      return NS_ERROR_FAILURE;
    }

    ////////////////////////////////////////////////////////////////////////////
    //// nsIDirectoryServiceProvider2

    NS_IMETHOD GetFiles(const char *aProperty, nsISimpleEnumerator **_enum) override
    {
      // If we were supplied a directory service provider, ask it first.
      nsCOMPtr<nsIDirectoryServiceProvider2> provider =
        do_QueryInterface(mDirSvcProvider);
      if (provider && NS_SUCCEEDED(provider->GetFiles(aProperty, _enum))) {
        return NS_OK;
      }

     return NS_ERROR_FAILURE;
   }

  private:
    const char* mTestName;
    nsIServiceManager* mServMgr;
    nsCOMPtr<nsIDirectoryServiceProvider> mDirSvcProvider;
    nsCOMPtr<nsIFile> mProfD;
    nsCOMPtr<nsIFile> mGRED;
    nsCOMPtr<nsIFile> mGREBinD;
};

NS_IMPL_QUERY_INTERFACE(
  ScopedXPCOM,
  nsIDirectoryServiceProvider,
  nsIDirectoryServiceProvider2
)

NS_IMETHODIMP_(MozExternalRefCountType)
ScopedXPCOM::AddRef()
{
  return 2;
}

NS_IMETHODIMP_(MozExternalRefCountType)
ScopedXPCOM::Release()
{
  return 1;
}

#endif  // TestHarness_h__
