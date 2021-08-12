/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.os.SystemClock

/**
 *  A class to store the application initialization time.
 *  Time is stores in elapsed real time nano seconds
 */
internal class ApplicationInitTimeContainer(
    private val getElapsedRealtimeNanos: () -> Long = SystemClock::elapsedRealtimeNanos
) {

    var applicationInitNanos = -1L
        private set
    private var isApplicationInitCalled = false

    fun onApplicationInit() {
        // This gets called from multiple processes: don't do anything expensive. See call site for details.
        //
        // In the main process, there are multiple Application impl so we ensure it's only set by
        // the first one.
        if (!isApplicationInitCalled) {
            isApplicationInitCalled = true
            applicationInitNanos = getElapsedRealtimeNanos()
        }
    }
}
