/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FakeLogging_h
#define FakeLogging_h

namespace mozilla {
namespace detail {
void log_print(const PRLogModuleInfo* aModule,
                      LogLevel aLevel,
                      const char* aFmt, ...)
  {
    // copied from Logging.cpp:#48-53
    va_list ap;
    va_start(ap, aFmt);
    char* buff = PR_vsmprintf(aFmt, ap);
    PR_LogPrint("%s", buff);
    PR_smprintf_free(buff);
    va_end(ap);
  }

}
}

#endif
