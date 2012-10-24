/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsCERTValInParamWrapper_H
#define _nsCERTValInParamWrapper_H

#include "nsISupports.h"
#include "cert.h"

/*
 * This is a wrapper around type
 * CERTValInParam is a nested input parameter type for CERT_PKIXVerifyCert.
 * The values inside this type depend on application preferences,
 * as a consequence it's expensive to construct this object.
 * (and we shall avoid to access prefs from secondary threads anyway).
 * We want to create an instance of that input type once, and use as long as possible.
 * Every time the preferences change, we will create a new default object.
 *
 * A race is possible between "verification function is active and object in use"
 * and "must switch to new defaults".
 *
 * The global default object may be replaced at any time with a new object.
 * The contents of inner CERTValInParam are supposed to be stable (const).
 *
 * In order to protect against the race, we use a reference counted wrapper.
 * Each user of a foreign nsCERTValInParamWrapper object
 * (e.g. the current global default object)
 * must use nsRefPtr<nsCERTValInParamWrapper> = other-object
 * prior to calling CERT_PKIXVerifyCert.
 * 
 * This guarantees the object will still be alive after the call,
 * and if the default object has been replaced in the meantime,
 * the reference counter will go to zero, and the old default
 * object will get destroyed automatically.
 */
class nsCERTValInParamWrapper
{
 public:
    NS_IMETHOD_(nsrefcnt) AddRef();
    NS_IMETHOD_(nsrefcnt) Release();

public:
  nsCERTValInParamWrapper();
  virtual ~nsCERTValInParamWrapper();

  enum missing_cert_download_config { missing_cert_download_off = 0, missing_cert_download_on };
  enum crl_download_config { crl_local_only = 0, crl_download_allowed };
  enum ocsp_download_config { ocsp_off = 0, ocsp_on };
  enum ocsp_strict_config { ocsp_relaxed = 0, ocsp_strict };
  enum any_revo_fresh_config { any_revo_relaxed = 0, any_revo_strict };

  nsresult Construct(missing_cert_download_config ac, crl_download_config cdc,
                     ocsp_download_config odc, ocsp_strict_config osc,
                     any_revo_fresh_config arfc,
                     const char *firstNetworkRevocationMethod);

private:
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
  bool mAlreadyConstructed;
  CERTValInParam *mCVIN;
  CERTRevocationFlags *mRev;
  
public:
  CERTValInParam *GetRawPointerForNSS() { return mCVIN; }
};

#endif
