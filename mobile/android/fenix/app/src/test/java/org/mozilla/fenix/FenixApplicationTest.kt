/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.PerfStartup
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class FenixApplicationTest {

    @get:Rule val gleanTestRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    private lateinit var application: FenixApplication

    @Before
    fun setUp() {
        application = ApplicationProvider.getApplicationContext()
    }

    @Ignore("See https://github.com/mozilla-mobile/fenix/issues/18102")
    @Test
    fun `GIVEN onCreate is called THEN the duration is measured`() {
        // application.onCreate is called before the test as part of test set up:
        // https://robolectric.blogspot.com/2013/04/the-test-lifecycle-in-20.html
        assertTrue(PerfStartup.applicationOnCreate.testHasValue())
    }
}
