/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_nsIDAPTelemetry_h__
#define mozilla_nsIDAPTelemetry_h__

#include "nsIDAPTelemetry.h"

namespace mozilla {

class DAPTelemetry final : public nsIDAPTelemetry {
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDAPTELEMETRY

 public:
  DAPTelemetry() = default;

 private:
  ~DAPTelemetry() = default;
};

}  // namespace mozilla

#endif
