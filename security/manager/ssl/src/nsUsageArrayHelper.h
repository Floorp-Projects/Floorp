/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 */

#ifndef _NSUSAGEARRAYHELPER_H_
#define _NSUSAGEARRAYHELPER_H_

#include "certt.h"

#include "nsNSSComponent.h"

class nsUsageArrayHelper
{
public:
  nsUsageArrayHelper(CERTCertificate *aCert);

  nsresult GetUsagesArray(char *suffix,
               PRBool ignoreOcsp,
               PRUint32 outArraySize,
               PRUint32 *_verified,
               PRUint32 *_count,
               PRUnichar **tmpUsages);

  enum { max_returned_out_array_size = 12 };

private:
  CERTCertificate *mCert;
  nsresult m_rv;
  CERTCertDBHandle *defaultcertdb;
  nsCOMPtr<nsINSSComponent> nssComponent;
  int mCached_NonInadequateReason;

  void check(char *suffix,
             SECCertUsage aCertUsage,
             PRUint32 &aCounter,
             PRUnichar **outUsages);

  void verifyFailed(PRUint32 *_verified);
};

#endif
