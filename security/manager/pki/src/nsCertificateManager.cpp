/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Ian McGreer <mcgreer@netscape.com>
 */

#include "nsIServiceManager.h"
#include "nsIX509CertDB.h"
#include "nsCertificateManager.h"

#include "prlog.h"
#ifdef PR_LOGGING
PRLogModuleInfo* gPIPPKILog = nsnull;
#endif

static NS_DEFINE_IID(kCertDBCID, NS_X509CERTDB_CID);

nsCertificateManager::nsCertificateManager()
{
  NS_INIT_REFCNT();
#ifdef PR_LOGGING
  if (!gPIPPKILog)
    gPIPPKILog = PR_NewLogModule("pippki");
#endif
}

nsCertificateManager::~nsCertificateManager()
{
}

NS_IMPL_ISUPPORTS(nsCertificateManager, NS_GET_IID(nsICertificateManager));

NS_IMETHODIMP
nsCertificateManager::GetCertNicknames(PRUint32 type,
                                       PRUnichar **_rNameList)
{
  nsresult rv;
  nsAutoString nameList;
  PR_LOG(gPIPPKILog, PR_LOG_ERROR, ("getting certdb service\n"));
  NS_WITH_SERVICE(nsIX509CertDB, certdb, kCertDBCID, &rv);
  if (NS_FAILED(rv)) return rv;
  PR_LOG(gPIPPKILog, PR_LOG_ERROR, ("getting cert names\n"));
  rv = certdb->GetCertificateNames(nsnull, nsIX509Cert::CA_CERT, nameList);
  if (NS_SUCCEEDED(rv)) {
  PR_LOG(gPIPPKILog, PR_LOG_ERROR, ("converting unicode\n"));
    *_rNameList = nameList.ToNewUnicode();
  }
  return rv;
}

//  wstring getCertCN(in string nickname);
NS_IMETHODIMP
nsCertificateManager::GetCertCN(const char *nickname,
                                PRUnichar **_rvCN)
{
  nsresult rv;
  nsIX509Cert *cert;
  PR_LOG(gPIPPKILog, PR_LOG_ERROR, ("getting certdb service\n"));
  NS_WITH_SERVICE(nsIX509CertDB, certdb, kCertDBCID, &rv);
  if (NS_FAILED(rv)) return rv;
  PR_LOG(gPIPPKILog, PR_LOG_ERROR, ("getting cert %s\n", nickname));
  rv = certdb->GetCertByName(nsnull, nickname, &cert);
  if (NS_SUCCEEDED(rv)) {
    PR_LOG(gPIPPKILog, PR_LOG_ERROR, ("converting unicode\n"));
    rv = cert->GetCommonName(_rvCN);
  }
  return rv;
}
