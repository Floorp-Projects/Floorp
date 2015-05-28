/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSCertificateFakeTransport_h
#define nsNSSCertificateFakeTransport_h

#include "mozilla/Vector.h"
#include "nsCOMPtr.h"
#include "nsIClassInfo.h"
#include "nsISerializable.h"
#include "nsIX509Cert.h"
#include "nsIX509CertList.h"
#include "secitem.h"

class nsNSSCertificateFakeTransport : public nsIX509Cert,
                                      public nsISerializable,
                                      public nsIClassInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERT
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  nsNSSCertificateFakeTransport();

protected:
  virtual ~nsNSSCertificateFakeTransport();

private:
  SECItem* mCertSerialization;
};

class nsNSSCertListFakeTransport : public nsIX509CertList,
                                   public nsISerializable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERTLIST
  NS_DECL_NSISERIALIZABLE

  nsNSSCertListFakeTransport();

protected:
  virtual ~nsNSSCertListFakeTransport();

private:
  mozilla::Vector<nsCOMPtr<nsIX509Cert> > mFakeCertList;
};

#endif // nsNSSCertificateFakeTransport_h
