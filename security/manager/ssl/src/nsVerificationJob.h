/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _INC_NSVERIFICATIONJOB_H
#define _INC_NSVERIFICATIONJOB_H

#include "nspr.h"

#include "nsIX509Cert.h"
#include "nsIX509Cert3.h"
#include "nsICMSMessage.h"
#include "nsICMSMessage2.h"

class nsBaseVerificationJob
{
public:
  virtual ~nsBaseVerificationJob() {}
  virtual void Run() = 0;
};

class nsCertVerificationJob : public nsBaseVerificationJob
{
public:
  nsCOMPtr<nsIX509Cert> mCert;
  nsCOMPtr<nsICertVerificationListener> mListener;

  void Run();
};

class nsCertVerificationResult : public nsICertVerificationResult
{
public:
  nsCertVerificationResult();
  virtual ~nsCertVerificationResult();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTVERIFICATIONRESULT

private:
  nsresult mRV;
  PRUint32 mVerified;
  PRUint32 mCount;
  PRUnichar **mUsages;

friend class nsCertVerificationJob;
};

class nsSMimeVerificationJob : public nsBaseVerificationJob
{
public:
  nsSMimeVerificationJob() { digest_data = nullptr; digest_len = 0; }
  ~nsSMimeVerificationJob() { delete [] digest_data; }

  nsCOMPtr<nsICMSMessage> mMessage;
  nsCOMPtr<nsISMimeVerificationListener> mListener;

  unsigned char *digest_data;
  PRUint32 digest_len;

  void Run();
};



#endif
