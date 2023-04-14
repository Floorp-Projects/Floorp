#include "MozHelpers.h"

namespace mozilla::gtest {

void DisableCrashReporter() {
  nsCOMPtr<nsICrashReporter> crashreporter =
      do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  if (crashreporter) {
    crashreporter->SetEnabled(false);
  }
}

}  // namespace mozilla::gtest
