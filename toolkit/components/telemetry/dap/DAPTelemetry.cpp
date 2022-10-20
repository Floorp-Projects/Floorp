/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DAPTelemetry.h"

#include "mozilla/Logging.h"
#include "mozilla/dap_ffi_generated.h"
#include "nsPromiseFlatString.h"

static mozilla::LazyLogModule sLogger("DAPTelemetry");
#undef LOG
#define LOG(...) MOZ_LOG(sLogger, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

NS_IMPL_ISUPPORTS(DAPTelemetry, nsIDAPTelemetry)

NS_IMETHODIMP DAPTelemetry::Test() {
  LOG("Calling dap_ffi wrapper");
  dap_ffi_test();
  return NS_OK;
}

}  // namespace mozilla
