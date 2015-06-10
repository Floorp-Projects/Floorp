/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_finalizationwitnessservice_h__
#define mozilla_finalizationwitnessservice_h__

#include "nsIFinalizationWitnessService.h"
#include "nsIObserver.h"

namespace mozilla {

/**
 * XPConnect initializer, for use in the main thread.
 */
class FinalizationWitnessService final : public nsIFinalizationWitnessService,
                                         public nsIObserver
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFINALIZATIONWITNESSSERVICE
  NS_DECL_NSIOBSERVER

  nsresult Init();
 private:
  ~FinalizationWitnessService() {}
  void operator=(const FinalizationWitnessService* other) = delete;
};

} // namespace mozilla

#endif // mozilla_finalizationwitnessservice_h__
