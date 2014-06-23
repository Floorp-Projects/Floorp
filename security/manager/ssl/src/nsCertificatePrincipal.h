/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_CERTIFICATEPRINCIPAL_H
#define __NS_CERTIFICATEPRINCIPAL_H

#include "nsICertificatePrincipal.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsCertificatePrincipal : public nsICertificatePrincipal
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTIFICATEPRINCIPAL

  nsCertificatePrincipal(const nsACString& aFingerprint,
                         const nsACString& aSubjectName,
                         const nsACString& aPrettyName,
                         nsISupports* aCert)
                         : mFingerprint(aFingerprint)
                         , mSubjectName(aSubjectName)
                         , mPrettyName(aPrettyName)
                         , mCert(aCert)
    {}

protected:
  virtual ~nsCertificatePrincipal() {}

private:
  nsCString             mFingerprint;
  nsCString             mSubjectName;
  nsCString             mPrettyName;
  nsCOMPtr<nsISupports> mCert;
};

#endif /* __NS_CERTIFICATEPRINCIPAL_H */
