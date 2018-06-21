/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelemetryGeckoViewTesting.h"
#include "TelemetryGeckoViewPersistence.h"
#include "TelemetryScalar.h"

namespace TelemetryGeckoViewTesting {
  // This is defined in TelemetryGeckoViewPersistence.cpp
  void TestDispatchPersist();
} // TelemetryGeckoViewTesting

NS_IMPL_ISUPPORTS(TelemetryGeckoViewTestingImpl, nsITelemetryGeckoViewTesting)

// We don't need |aCx|. It's there to make these test functions harder
// to call from C++.
NS_IMETHODIMP
TelemetryGeckoViewTestingImpl::InitPersistence(JSContext*)
{
  TelemetryGeckoViewPersistence::InitPersistence();
  return NS_OK;
}

NS_IMETHODIMP
TelemetryGeckoViewTestingImpl::DeInitPersistence(JSContext*)
{
  TelemetryGeckoViewPersistence::DeInitPersistence();
  return NS_OK;
}

NS_IMETHODIMP
TelemetryGeckoViewTestingImpl::ClearPersistenceData(JSContext*)
{
  TelemetryGeckoViewPersistence::ClearPersistenceData();
  return NS_OK;
}

NS_IMETHODIMP
TelemetryGeckoViewTestingImpl::ForcePersist(JSContext*)
{
  TelemetryGeckoViewTesting::TestDispatchPersist();
  return NS_OK;
}

NS_IMETHODIMP
TelemetryGeckoViewTestingImpl::DeserializationStarted(JSContext*)
{
  TelemetryScalar::DeserializationStarted();
  return NS_OK;
}
