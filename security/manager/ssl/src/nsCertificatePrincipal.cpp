/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCertificatePrincipal.h"

NS_IMPL_ISUPPORTS1(nsCertificatePrincipal, nsICertificatePrincipal)

NS_IMETHODIMP
nsCertificatePrincipal::GetFingerprint(nsACString& aFingerprint)
{
  aFingerprint = mFingerprint;
  return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetSubjectName(nsACString& aSubjectName)
{
  aSubjectName = mSubjectName;
  return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetPrettyName(nsACString& aPrettyName)
{
  aPrettyName = mPrettyName;
  return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetCertificate(nsISupports** aCert)
{
  nsCOMPtr<nsISupports> cert = mCert;
  cert.forget(aCert);
  return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::GetHasCertificate(bool* rv)
{
  *rv = true;
  return NS_OK;
}

NS_IMETHODIMP
nsCertificatePrincipal::Equals(nsICertificatePrincipal* aOther, bool* rv)
{
  nsAutoCString str;
  aOther->GetFingerprint(str);
  if (!str.Equals(mFingerprint)) {
    *rv = false;
    return NS_OK;
  }

  // If either subject name is empty, just let the result stand, but if they're
  // both non-empty, only claim equality if they're equal.
  if (!mSubjectName.IsEmpty()) {
    // Check the other principal's subject name
    aOther->GetSubjectName(str);
    *rv = str.Equals(mSubjectName) || str.IsEmpty();
    return NS_OK;
  }

  *rv = true;
  return NS_OK;
}
