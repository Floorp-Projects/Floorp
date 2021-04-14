/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.R
import org.mozilla.fenix.components.TestComponents
import org.robolectric.TestLifecycleApplication
import java.lang.reflect.Method

/**
 * An override of our application for use in Robolectric-based unit tests. We're forced to override
 * because our standard application fails to initialize in Robolectric with exceptions like:
 * "Crash handler service must run in a separate process".
 */
class FenixRobolectricTestApplication : FenixApplication(), TestLifecycleApplication {

    // Though JUnit 4 discourages global rules, we can apply global rules in robolectric so we do
    // to prevent confusion from devs.
    private val disableNavGraphProviderAssertionRule = DisableNavGraphProviderAssertionRule()

    override fun onCreate() {
        super.onCreate()
        setApplicationTheme()
    }

    override val components = TestComponents(this)

    override fun initializeGlean() = Unit

    override fun setupInAllProcesses() = Unit

    override fun setupInMainProcessOnly() = Unit

    private fun setApplicationTheme() {
        // According to the Robolectric devs, the application context will not have the <application>'s
        // theme but will use the platform's default team so we set our theme here. We change it here
        // rather than the production application because, upon testing, the production code appears
        // appears to be working correctly. Context here:
        // https://github.com/mozilla-mobile/fenix/pull/15646#issuecomment-707345798
        // https://github.com/mozilla-mobile/fenix/pull/15646#issuecomment-709411141
        setTheme(R.style.NormalTheme)
    }

    // Before test runs before the test class is initialized
    override fun beforeTest(method: Method?) {}

    // Prepare test runs once the test class and all its member variables (mock and
    // non mocks) are initialized. This method runs after application.onCreate
    override fun prepareTest(test: Any?) {
        // We call this in prepareTest rather than beforeTest because member vars
        // are initialized so it feels more correct to call it here.
        disableNavGraphProviderAssertionRule.setUp()
    }

    override fun afterTest(method: Method?) {
        disableNavGraphProviderAssertionRule.tearDown()
    }
}
