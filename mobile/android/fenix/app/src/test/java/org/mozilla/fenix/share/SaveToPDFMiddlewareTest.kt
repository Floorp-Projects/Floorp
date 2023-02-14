/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertNotNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SaveToPDFMiddlewareTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @get:Rule
    val mainCoroutineTestRule = MainCoroutineRule()

    @Test
    fun `GIVEN a save to pdf request WHEN it fails THEN telemetry is sent`() = runTestOnMain {
        val middleware = SaveToPDFMiddleware(testContext)
        val browserStore = BrowserStore(middleware = listOf(middleware))

        browserStore.dispatch(EngineAction.SaveToPdfExceptionAction("14", RuntimeException("reader save to pdf failed")))
        browserStore.waitUntilIdle()
        testScheduler.advanceUntilIdle()

        assertNotNull(Events.saveToPdfFailure.testGetValue())
    }
}
