/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.support.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.newSingleThreadContext
import kotlinx.coroutines.runBlocking

@ObsoleteCoroutinesApi
internal object Dispatchers {
    class WaitableCoroutineScope(val coroutineScope: CoroutineScope) {
        // When true, jobs will be run synchronously
        internal var testingMode = false

        /**
         * Launch a block of work asyncronously.
         *
         * If [enableTestingMode] has been called, the work will run
         * synchronously.
         *
         * @return Job, or null if run synchronously.
         */
        fun launch(
            block: suspend CoroutineScope.() -> Unit
        ): Job? {
            if (testingMode) {
                runBlocking {
                    block()
                }
                return null
            } else {
                return coroutineScope.launch(block = block)
            }
        }

        /**
         * Helper function to ensure glean is being used in testing mode and async
         * jobs are being run synchronously.  This should be called from every method
         * in the testing API to make sure that the results of the main API can be
         * tested as expected.
        */
        @VisibleForTesting(otherwise = VisibleForTesting.NONE)
        fun assertInTestingMode() {
            assert(
                testingMode,
                {
                    "To use the testing API, glean must be in testing mode by calling "
                    "Glean.enableTestingMode() (for example, in a @Before method)."
                }
            )
        }

        /**
         * Enable testing mode, which makes all of the glean public API synchronous.
         */
        @VisibleForTesting(otherwise = VisibleForTesting.NONE)
        fun enableTestingMode() {
            testingMode = true
        }
    }

    /**
     * A coroutine scope to make it easy to dispatch API calls off the main thread.
     * This needs to be a `var` so that our tests can override this.
     */
    var API = WaitableCoroutineScope(CoroutineScope(newSingleThreadContext("GleanAPIPool")))
}
