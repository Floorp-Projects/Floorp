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

#include "nsNSSCertCache.h"
#include "nsAutoLock.h"
#include "cert.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsNSSHelper.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsNSSCertCache, nsINSSCertCache)

nsNSSCertCache::nsNSSCertCache()
:mCertList(nsnull)
{
  mutex = PR_NewLock();
}

nsNSSCertCache::~nsNSSCertCache()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsNSSCertCache::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsNSSCertCache::destructorSafeDestroyNSSReference()
{
  if (isAlreadyShutDown())
    return;

  if (mCertList) {
    CERT_DestroyCertList(mCertList);
  }
  if (mutex) {
    PR_DestroyLock(mutex);
    mutex = nsnull;
  }
}

NS_IMETHODIMP
nsNSSCertCache::CacheAllCerts()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  {
    nsAutoLock lock(mutex);
    if (mCertList) {
      CERT_DestroyCertList(mCertList);
      mCertList = nsnull;
    }
  }

  nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();
  
  CERTCertList *newList = PK11_ListCerts(PK11CertListUnique, cxt);

  if (newList) {
    nsAutoLock lock(mutex);
    mCertList = newList;
  }
  
  return NS_OK;
}

void* nsNSSCertCache::GetCachedCerts()
{
  if (isAlreadyShutDown())
    return nsnull;

  nsAutoLock lock(mutex);
  return mCertList;
}
