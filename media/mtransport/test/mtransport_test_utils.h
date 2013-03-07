/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef mtransport_test_utils_h__
#define mtransport_test_utils_h__

#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsXPCOMGlue.h"
#include "nsXPCOM.h"

#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsISocketTransportService.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#endif
#include "nsPISocketTransportService.h"
#include "nsServiceManagerUtils.h"
#include "TestHarness.h"
#include "mozilla/mozPoisonWrite.h"

class MtransportTestUtils {
 public:
  MtransportTestUtils() : xpcom_("") {
    if (!sts_) {
      InitServices();
    }
  }

  ~MtransportTestUtils() {
    sts_->Shutdown();
  }

  void InitServices() {
    nsresult rv;
    ioservice_ = do_GetIOService(&rv);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    sts_target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    sts_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

#ifdef MOZ_CRASHREPORTER
    char *crashreporter = PR_GetEnv("MOZ_CRASHREPORTER");
    if (crashreporter && !strcmp(crashreporter, "1")) {
      //TODO: move this to an even-more-common location to use in all
      // C++ unittests
      crashreporter_ = do_GetService("@mozilla.org/toolkit/crash-reporter;1");
      if (crashreporter_) {
        std::cerr << "Setting up crash reporting" << std::endl;

        nsCOMPtr<nsIProperties> dirsvc =
            do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
        nsCOMPtr<nsIFile> cwd;
        rv = dirsvc->Get(NS_OS_CURRENT_WORKING_DIR,
                         NS_GET_IID(nsIFile),
                         getter_AddRefs(cwd));
        MOZ_ASSERT(NS_SUCCEEDED(rv));
        crashreporter_->SetEnabled(true);
        crashreporter_->SetMinidumpPath(cwd);
      }
    }
#endif
  }

  nsCOMPtr<nsIEventTarget> sts_target() { return sts_target_; }

 private:
  ScopedXPCOM xpcom_;
  nsCOMPtr<nsIIOService> ioservice_;
  nsCOMPtr<nsIEventTarget> sts_target_;
  nsCOMPtr<nsPISocketTransportService> sts_;
#ifdef MOZ_CRASHREPORTER
  nsCOMPtr<nsICrashReporter> crashreporter_;
#endif
};


MtransportTestUtils *mtransport_test_utils;

#define SETUP_MTRANSPORT_TEST_UTILS() \
  MtransportTestUtils utils_; mtransport_test_utils = &utils_

#define CHECK_ENVIRONMENT_FLAG(envname) \
  char *test_flag = getenv(envname); \
  if (!test_flag || strcmp(test_flag, "1")) { \
    printf("To run this test set %s=1 in your environment\n", envname); \
    exit(0); \
  } \


#endif

