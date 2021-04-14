/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import io.mockk.every
import io.mockk.mockkObject
import io.mockk.unmockkObject
import org.junit.rules.TestWatcher
import org.junit.runner.Description
import org.mozilla.fenix.perf.NavGraphProvider

/**
 * Disables the call order assertions defined by the [NavGraphProvider] for use in testing.
 * This is necessary because unit tests generally don't follow the application lifecycle and thus
 * call the methods out of order, causing an assertion to be thrown unexpectedly. You may need to
 * apply this rule if you see the following exception in your test:
 *
 * Unfortunately, JUnit 4 discourages setting test state globally so we apply this to each test that
 * has the failure rather than disabling it globally.
 */
class DisableNavGraphProviderAssertionRule : TestWatcher() {

    // public for code reuse.
    fun setUp() {
        mockkObject(NavGraphProvider)
        every { NavGraphProvider.blockForNavGraphInflation(any()) } returns Unit
    }

    // public for code reuse.
    fun tearDown() { //
        unmockkObject(NavGraphProvider)
    }

    override fun starting(description: Description?) {
        setUp()
    }

    override fun finished(description: Description?) {
        tearDown()
    }
}
