/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSCLRLINFO_H_
#define _NSCRLINFO_H_

#include "nsICRLInfo.h"

#include "certt.h"
#include "nsString.h"
	  
#define CRL_AUTOUPDATE_TIMIINGTYPE_PREF "security.crl.autoupdate.timingType"
#define CRL_AUTOUPDATE_TIME_PREF "security.crl.autoupdate.nextInstant"
#define CRL_AUTOUPDATE_URL_PREF "security.crl.autoupdate.url"
#define CRL_AUTOUPDATE_DAYCNT_PREF "security.crl.autoupdate.dayCnt"
#define CRL_AUTOUPDATE_FREQCNT_PREF "security.crl.autoupdate.freqCnt"
#define CRL_AUTOUPDATE_ERRCNT_PREF "security.crl.autoupdate.errCount"
#define CRL_AUTOUPDATE_ERRDETAIL_PREF "security.crl.autoupdate.errDetail"
#define CRL_AUTOUPDATE_ENABLED_PREF "security.crl.autoupdate.enable."
#define CRL_AUTOUPDATE_DEFAULT_DELAY 30000UL

class nsCRLInfo : public nsICRLInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICRLINFO

  nsCRLInfo();
  nsCRLInfo(CERTSignedCrl *);
  virtual ~nsCRLInfo();
  /* additional members */
private:
  nsString mOrg;
  nsString mOrgUnit;
  nsString mLastUpdateLocale;
  nsString mNextUpdateLocale;
  PRTime mLastUpdate;
  PRTime mNextUpdate;
  nsString mNameInDb;
  nsCString mLastFetchURL;
  nsString mNextAutoUpdateDate;
};

#endif
