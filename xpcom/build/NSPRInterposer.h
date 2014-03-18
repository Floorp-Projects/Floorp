/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSPRINTERPOSER_H_
#define NSPRINTERPOSER_H_

namespace mozilla {

/**
 * Initialize IO interposing for NSPR. This will report NSPR read, writes and
 * fsyncs to the IOInterposerObserver. It is only safe to call this from the
 * main-thread when no other threads are running.
 */
void InitNSPRIOInterposing();

/**
 * Removes interception of NSPR IO methods as setup by InitNSPRIOInterposing.
 * Note, that it is only safe to call this on the main-thread when all other
 * threads have stopped. Which is typically the case at shutdown.
 */
void ClearNSPRIOInterposing();

} // namespace mozilla

#endif // NSPRINTERPOSER_H_
