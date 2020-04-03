/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/**
 * A test runner that starts Robolectric with our custom configuration for use in unit tests. This
 * should ALWAYS be used instead of RobolectricTestRunner and AndroidJUnit4. You should only use
 * Robolectric when necessary because it non-trivially increases test duration.
 *
 * usage:
 * ```
 * @RunWith(FenixRobolectricTestRunner::class)
 * class ExampleUnitTest {
 * ```
 *
 * There were three common test runners before this patch:
 * 1. The default (@RunWith not specified) = JUnit4
 * 2. @RunWith(RobolectricTestRunner::class) = JUnit4 with support for the Android framework via Robolectric
 * 3. @RunWith(AndroidJUnit4::class) = JUnit4 with support for the Android framework. This currently
 * delegates to Robolectric but is presumably generically named so that it can support different
 * implementations in the future. The name creates confusion on over the difference between this and
 * JUnit without any Android support (1).
 *
 * We chose the name RobolectricTestRunner because we want folks to know they're starting Robolectric
 * because it increases test runtime. Furthermore, the naming of 3) is unclear so we didn't want to
 * use that name.
 */
class FenixRobolectricTestRunner(testClass: Class<*>) : RobolectricTestRunner(testClass) {

    override fun buildGlobalConfig(): Config {
        return Config.Builder()
            .setApplication(FenixRobolectricTestApplication::class.java)
            .build()
    }
}
