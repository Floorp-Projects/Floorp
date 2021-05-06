/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.telemetry

import androidx.test.core.app.ApplicationProvider
import io.mockk.mockk
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.base.android.Clock
import mozilla.components.support.test.ext.joinBlocking
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.GleanMetrics.EngineTab as EngineMetrics

@RunWith(FenixRobolectricTestRunner::class)
class TelemetryLifecycleObserverTest {
    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    private val clock = FakeClock()

    @Before
    fun setUp() {
        Clock.delegate = clock
    }

    @After
    fun tearDown() {
        Clock.reset()
    }

    @Test
    fun `resume without a pause does not record any metrics`() {
        val store = BrowserStore()
        val observer = TelemetryLifecycleObserver(store)
        observer.onResume()

        assertFalse(EngineMetrics.foregroundMetrics.testHasValue())
    }

    @Test
    fun `resume after pause records metrics`() {
        val store = BrowserStore()
        val observer = TelemetryLifecycleObserver(store)

        observer.onPause()

        clock.elapsedTime = 550

        observer.onResume()

        assertTrue(EngineMetrics.foregroundMetrics.testHasValue())

        val metrics = EngineMetrics.foregroundMetrics.testGetValue()
        assertEquals(1, metrics.size)

        val metric = metrics[0]
        assertNotNull(metric.extra)
        assertEquals("550", metric.extra!!["time_in_background"])
    }

    @Test
    fun `resume records expected values`() {
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla", engineSession = mockk(relaxed = true)),
                    createTab("https://news.google.com", id = "news"),
                    createTab("https://theverge.com", id = "theverge", engineSession = mockk(relaxed = true)),
                    createTab("https://www.google.com", id = "google", engineSession = mockk(relaxed = true)),
                    createTab("https://getpocket.com", id = "pocket", crashed = true)
                )
            ),
            middleware = EngineMiddleware.create(engine = mockk())
        )

        val observer = TelemetryLifecycleObserver(store)

        clock.elapsedTime = 120

        observer.onPause()

        store.dispatch(
            EngineAction.KillEngineSessionAction("theverge")
        ).joinBlocking()

        store.dispatch(
            EngineAction.SuspendEngineSessionAction("mozilla")
        ).joinBlocking()

        clock.elapsedTime = 10340

        observer.onResume()

        assertTrue(EngineMetrics.foregroundMetrics.testHasValue())

        val metrics = EngineMetrics.foregroundMetrics.testGetValue()
        assertEquals(1, metrics.size)

        val metric = metrics[0]
        assertNotNull(metric.extra)
        assertEquals("10220", metric.extra!!["time_in_background"])
        assertEquals("3", metric.extra!!["background_active_tabs"])
        assertEquals("1", metric.extra!!["background_crashed_tabs"])
        assertEquals("5", metric.extra!!["background_total_tabs"])
        assertEquals("1", metric.extra!!["foreground_active_tabs"])
        assertEquals("1", metric.extra!!["foreground_crashed_tabs"])
        assertEquals("5", metric.extra!!["foreground_total_tabs"])
    }
}
