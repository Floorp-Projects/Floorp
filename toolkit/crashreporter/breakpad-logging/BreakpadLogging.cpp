/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BreakpadLogging.h"

#include <ostream>

namespace mozilla {

// An output stream that acts like /dev/null and drops all output directed to it
// Passing 0 here causes the ostream to enter an error state, and so it silently
// drops all output directed to it.
std::ostream gNullStream(0);

} // namespace mozilla
