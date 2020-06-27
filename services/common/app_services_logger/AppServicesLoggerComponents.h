/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_services_AppServicesLoggerComponents_h_
#define mozilla_services_AppServicesLoggerComponents_h_

#include "mozIAppServicesLogger.h"
#include "nsCOMPtr.h"

extern "C" {

// Implemented in Rust, in the `app_services_logger` crate.
nsresult NS_NewAppServicesLogger(mozIAppServicesLogger** aResult);

}  // extern "C"

namespace mozilla {
namespace appservices {

// The C++ constructor for a `services.appServicesLogger` service. This wrapper
// exists because `components.conf` requires a component class constructor to
// return an `already_AddRefed<T>`, but Rust doesn't have such a type. So we
// call the Rust constructor using a `nsCOMPtr` (which is compatible with Rust's
// `xpcom::RefPtr`) out param, and return that.
already_AddRefed<mozIAppServicesLogger> NewLogService() {
  nsCOMPtr<mozIAppServicesLogger> logger;
  nsresult rv = NS_NewAppServicesLogger(getter_AddRefs(logger));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return logger.forget();
}

}  // namespace appservices
}  // namespace mozilla

#endif  // mozilla_services_AppServicesLoggerComponents_h_
