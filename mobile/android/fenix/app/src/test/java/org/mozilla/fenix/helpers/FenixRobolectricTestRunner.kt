/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/**
 * A test runner that starts Robolectric with our custom configuration for use in unit tests.
 *
 * usage:
 * ```
 * @RunWith(FenixRobolectricTestRunner::class)
 * class ExampleUnitTest {
 * ```
 *
 * IMPORTANT NOTES:
 * - This should ALWAYS be used instead of RobolectricTestRunner and AndroidJUnit4 (note: the latter
 * just delegates to the former)
 * - You should only use Robolectric when necessary because it non-trivially increases test duration.
 */
class FenixRobolectricTestRunner(testClass: Class<*>) : RobolectricTestRunner(testClass) {

    override fun buildGlobalConfig(): Config {
        return Config.Builder()
            .setApplication(FenixRobolectricTestApplication::class.java)
            .build()
    }
}
