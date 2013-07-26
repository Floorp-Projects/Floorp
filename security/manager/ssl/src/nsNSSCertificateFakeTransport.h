/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_NSSCERTIFICATECHILD_H_
#define _NS_NSSCERTIFICATECHILD_H_

#include "nsIX509Cert.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "secitem.h"

/* Certificate */
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
  virtual ~nsNSSCertificateFakeTransport();

private:
  SECItem *mCertSerialization;
};

#endif /* _NS_NSSCERTIFICATECHILD_H_ */
