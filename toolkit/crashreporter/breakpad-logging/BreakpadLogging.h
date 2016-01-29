/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BreakpadLogging_h
#define BreakpadLogging_h

#include <ostream>

namespace mozilla {

// An output stream that acts like /dev/null and drops all output directed to it
extern std::ostream gNullStream;

} // namespace mozilla

// Override the breakpad info stream to disable INFO logs
#define BPLOG_INFO_STREAM mozilla::gNullStream

#endif // BreakpadLogging_h
