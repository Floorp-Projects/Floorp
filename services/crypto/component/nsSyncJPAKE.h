/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsSyncJPAKE_h__
#define nsSyncJPAKE_h__

#include "ScopedNSSTypes.h"
#include "nsISyncJPAKE.h"
#include "nsNSSShutDown.h"

#define NS_SYNCJPAKE_CONTRACTID \
  "@mozilla.org/services-crypto/sync-jpake;1"

#define NS_SYNCJPAKE_CID \
  {0x0b9721c0, 0x1805, 0x47c3, {0x86, 0xce, 0x68, 0x13, 0x79, 0x5a, 0x78, 0x3f}}

using namespace mozilla;

class nsSyncJPAKE : public nsISyncJPAKE
                  , public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISYNCJPAKE
  nsSyncJPAKE();
protected:
  virtual ~nsSyncJPAKE();
private:
  virtual void virtualDestroyNSSReference() override;
  void destructorSafeDestroyNSSReference();

  enum { JPAKENotStarted, JPAKEBeforeRound2, JPAKEAfterRound2 } round;
  ScopedPK11SymKey key;
};

NS_IMPL_ISUPPORTS(nsSyncJPAKE, nsISyncJPAKE)

#endif // nsSyncJPAKE_h__
