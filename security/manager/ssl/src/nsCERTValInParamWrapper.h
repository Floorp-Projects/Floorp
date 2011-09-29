/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
