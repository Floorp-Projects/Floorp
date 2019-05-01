/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _INC_NSVERIFICATIONJOB_H
#define _INC_NSVERIFICATIONJOB_H

#include "nspr.h"

#include "nsIX509Cert.h"
#include "nsProxyRelease.h"

class nsBaseVerificationJob {
 public:
  virtual ~nsBaseVerificationJob() {}
  virtual void Run() = 0;
};

class nsCertVerificationJob : public nsBaseVerificationJob {
 public:
  nsCOMPtr<nsIX509Cert> mCert;
  nsMainThreadPtrHandle<nsICertVerificationListener> mListener;

  void Run();
};

class nsCertVerificationResult : public nsICertVerificationResult {
 public:
  nsCertVerificationResult();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICERTVERIFICATIONRESULT

 protected:
  virtual ~nsCertVerificationResult();

 private:
  nsresult mRV;
  uint32_t mVerified;
  uint32_t mCount;
  char16_t** mUsages;

  friend class nsCertVerificationJob;
};

#endif
