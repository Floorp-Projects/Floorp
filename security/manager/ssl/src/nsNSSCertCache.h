/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSNSSCERTCACHE_H_
#define _NSNSSCERTCACHE_H_

#include "nsINSSCertCache.h"
#include "nsIX509CertList.h"
#include "certt.h"
#include "mozilla/Mutex.h"
#include "nsNSSShutDown.h"
#include "nsCOMPtr.h"

class nsNSSCertCache : public nsINSSCertCache,
                       public nsNSSShutDownObject
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINSSCERTCACHE

  nsNSSCertCache();
  virtual ~nsNSSCertCache();

private:
  mozilla::Mutex mutex;
  nsCOMPtr<nsIX509CertList> mCertList;
  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

#endif
