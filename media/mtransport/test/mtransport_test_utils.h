/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef mtransport_test_utils_h__
#define mtransport_test_utils_h__

#include "nsCOMPtr.h"
#include "nsNetCID.h"

#include "nsIEventTarget.h"
#include "nsPISocketTransportService.h"
#include "nsServiceManagerUtils.h"

class MtransportTestUtils {
 public:
  MtransportTestUtils() {
      InitServices();
  }

  ~MtransportTestUtils() {
  }

  void InitServices() {
    nsresult rv;
    sts_target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    sts_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  nsIEventTarget* sts_target() { return sts_target_; }

 private:
  nsCOMPtr<nsIEventTarget> sts_target_;
  nsCOMPtr<nsPISocketTransportService> sts_;
};


#define CHECK_ENVIRONMENT_FLAG(envname) \
  char *test_flag = getenv(envname); \
  if (!test_flag || strcmp(test_flag, "1")) { \
    printf("To run this test set %s=1 in your environment\n", envname); \
    exit(0); \
  } \


#endif
