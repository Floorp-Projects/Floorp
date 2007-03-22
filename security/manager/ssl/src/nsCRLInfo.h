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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
