/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FakeLogging_h
#define FakeLogging_h

namespace mozilla {
namespace detail {
void log_print(const PRLogModuleInfo* aModule,
                      LogLevel aLevel,
                      const char* aFmt, ...) { }
}
}

#endif
