/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_finalizationwitnessservice_h__
#define mozilla_finalizationwitnessservice_h__

#include "nsIFinalizationWitnessService.h"

namespace mozilla {

/**
 * XPConnect initializer, for use in the main thread.
 */
class FinalizationWitnessService MOZ_FINAL : public nsIFinalizationWitnessService
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFINALIZATIONWITNESSSERVICE
 private:
  void operator=(const FinalizationWitnessService* other) MOZ_DELETE;
};

} // namespace mozilla

#endif // mozilla_finalizationwitnessservice_h__
