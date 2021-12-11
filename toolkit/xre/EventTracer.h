/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XRE_EVENTTRACER_H_
#define XRE_EVENTTRACER_H_

namespace mozilla {

// Create a thread that will fire events back at the
// main thread to measure responsiveness. Return true
// if the thread was created successfully.
//   aLog If the tracing results should be printed to
//        the console.
bool InitEventTracing(bool aLog);

// Signal the background thread to stop, and join it.
// Must be called from the same thread that called InitEventTracing.
void ShutdownEventTracing();

}  // namespace mozilla

#endif /* XRE_EVENTTRACER_H_ */
