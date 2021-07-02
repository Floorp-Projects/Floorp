/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.telemetry

import androidx.test.core.app.ApplicationProvider
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.base.android.Clock
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.GleanMetrics.EngineTab as EngineMetrics

@RunWith(FenixRobolectricTestRunner::class)
@ExperimentalCoroutinesApi
class TelemetryMiddlewareTest {

    private lateinit var store: BrowserStore
    private lateinit var settings: Settings
    private lateinit var telemetryMiddleware: TelemetryMiddleware
    private lateinit var metrics: MetricController
    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    private val clock = FakeClock()

    @Before
    fun setUp() {
        Clock.delegate = clock

        settings = Settings(testContext)
        metrics = mockk(relaxed = true)
        telemetryMiddleware = TelemetryMiddleware(
            settings,
            metrics
        )
        store = BrowserStore(
            middleware = listOf(telemetryMiddleware) + EngineMiddleware.create(engine = mockk()),
            initialState = BrowserState()
        )
    }

    @After
    fun tearDown() {
        Clock.reset()
    }

    @Test
    fun `WHEN a tab is added THEN the open tab count is updated`() {
        assertEquals(0, settings.openTabsCount)

        store.dispatch(TabListAction.AddTabAction(createTab("https://mozilla.org"))).joinBlocking()
        assertEquals(1, settings.openTabsCount)
        verify(exactly = 1) { metrics.track(Event.HaveOpenTabs) }
    }

    @Test
    fun `WHEN a private tab is added THEN the open tab count is not updated`() {
        assertEquals(0, settings.openTabsCount)

        store.dispatch(TabListAction.AddTabAction(createTab("https://mozilla.org", private = true))).joinBlocking()
        assertEquals(0, settings.openTabsCount)
        verify(exactly = 1) { metrics.track(Event.HaveNoOpenTabs) }
    }

    @Test
    fun `WHEN multiple tabs are added THEN the open tab count is updated`() {
        assertEquals(0, settings.openTabsCount)
        store.dispatch(
            TabListAction.AddMultipleTabsAction(listOf(
                createTab("https://mozilla.org"),
                createTab("https://firefox.com"))
            )
        ).joinBlocking()

        assertEquals(2, settings.openTabsCount)
        verify(exactly = 1) { metrics.track(Event.HaveOpenTabs) }
    }

    @Test
    fun `WHEN a tab is removed THEN the open tab count is updated`() {
        store.dispatch(
            TabListAction.AddMultipleTabsAction(listOf(
                createTab(id = "1", url = "https://mozilla.org"),
                createTab(id = "2", url = "https://firefox.com"))
            )
        ).joinBlocking()
        assertEquals(2, settings.openTabsCount)
        verify(exactly = 1) { metrics.track(Event.HaveOpenTabs) }

        store.dispatch(TabListAction.RemoveTabAction("1")).joinBlocking()
        assertEquals(1, settings.openTabsCount)
        verify(exactly = 2) { metrics.track(Event.HaveOpenTabs) }
    }

    @Test
    fun `WHEN all tabs are removed THEN the open tab count is updated`() {
        store.dispatch(
            TabListAction.AddMultipleTabsAction(listOf(
                createTab("https://mozilla.org"),
                createTab("https://firefox.com"))
            )
        ).joinBlocking()
        assertEquals(2, settings.openTabsCount)
        verify(exactly = 1) { metrics.track(Event.HaveOpenTabs) }

        store.dispatch(TabListAction.RemoveAllTabsAction).joinBlocking()
        assertEquals(0, settings.openTabsCount)
        verify(exactly = 1) { metrics.track(Event.HaveNoOpenTabs) }
    }

    @Test
    fun `WHEN all normal tabs are removed THEN the open tab count is updated`() {
        store.dispatch(
            TabListAction.AddMultipleTabsAction(listOf(
                createTab("https://mozilla.org"),
                createTab("https://firefox.com"),
                createTab("https://getpocket.com", private = true))
            )
        ).joinBlocking()
        assertEquals(2, settings.openTabsCount)
        verify(exactly = 1) { metrics.track(Event.HaveOpenTabs) }

        store.dispatch(TabListAction.RemoveAllNormalTabsAction).joinBlocking()
        assertEquals(0, settings.openTabsCount)
        verify(exactly = 1) { metrics.track(Event.HaveNoOpenTabs) }
    }

    @Test
    fun `WHEN tabs are restored THEN the open tab count is updated`() {
        assertEquals(0, settings.openTabsCount)
        val tabsToRestore = listOf(
            createTab("https://mozilla.org"),
            createTab("https://firefox.com")
        )

        store.dispatch(TabListAction.RestoreAction(tabsToRestore)).joinBlocking()
        assertEquals(2, settings.openTabsCount)
        verify(exactly = 1) { metrics.track(Event.HaveOpenTabs) }
    }

    @Test
    fun `GIVEN a page is loading WHEN loading is complete THEN we record a UriOpened event`() {
        val tab = createTab(id = "1", url = "https://mozilla.org")
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, true)).joinBlocking()
        verify(exactly = 0) { metrics.track(Event.UriOpened) }
        verify(exactly = 0) { metrics.track(Event.NormalAndPrivateUriOpened) }

        store.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, false)).joinBlocking()
        verify(exactly = 1) { metrics.track(Event.UriOpened) }
        verify(exactly = 1) { metrics.track(Event.NormalAndPrivateUriOpened) }
    }

    @Test
    fun `GIVEN a private page is loading WHEN loading is complete THEN we never record a UriOpened event`() {
        val tab = createTab(id = "1", url = "https://mozilla.org", private = true)
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, true)).joinBlocking()
        verify(exactly = 0) { metrics.track(Event.UriOpened) }
        verify(exactly = 0) { metrics.track(Event.NormalAndPrivateUriOpened) }

        store.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, false)).joinBlocking()
        verify(exactly = 0) { metrics.track(Event.UriOpened) }
        verify(exactly = 1) { metrics.track(Event.NormalAndPrivateUriOpened) }
    }

    @Test
    fun `WHEN a download is added THEN the downloads count is updated`() {
        store.dispatch(DownloadAction.AddDownloadAction(mock())).joinBlocking()

        verify { metrics.track(Event.DownloadAdded) }
    }

    @Test
    fun `WHEN foreground tab getting killed THEN middleware counts it`() {
        store.dispatch(TabListAction.RestoreAction(
            listOf(
                createTab("https://www.mozilla.org", id = "foreground"),
                createTab("https://getpocket.com", id = "background_pocket"),
                createTab("https://theverge.com", id = "background_verge")
            ),
            selectedTabId = "foreground"
        )).joinBlocking()

        assertFalse(EngineMetrics.kills["foreground"].testHasValue())
        assertFalse(EngineMetrics.kills["background"].testHasValue())

        store.dispatch(
            EngineAction.KillEngineSessionAction("foreground")
        ).joinBlocking()

        assertTrue(EngineMetrics.kills["foreground"].testHasValue())
    }

    @Test
    fun `WHEN background tabs getting killed THEN middleware counts it`() {
        store.dispatch(TabListAction.RestoreAction(
            listOf(
                createTab("https://www.mozilla.org", id = "foreground"),
                createTab("https://getpocket.com", id = "background_pocket"),
                createTab("https://theverge.com", id = "background_verge")
            ),
            selectedTabId = "foreground"
        )).joinBlocking()

        assertFalse(EngineMetrics.kills["foreground"].testHasValue())
        assertFalse(EngineMetrics.kills["background"].testHasValue())

        store.dispatch(
            EngineAction.KillEngineSessionAction("background_pocket")
        ).joinBlocking()

        assertFalse(EngineMetrics.kills["foreground"].testHasValue())
        assertTrue(EngineMetrics.kills["background"].testHasValue())
        assertEquals(1, EngineMetrics.kills["background"].testGetValue())

        store.dispatch(
            EngineAction.KillEngineSessionAction("background_verge")
        ).joinBlocking()

        assertFalse(EngineMetrics.kills["foreground"].testHasValue())
        assertTrue(EngineMetrics.kills["background"].testHasValue())
        assertEquals(2, EngineMetrics.kills["background"].testGetValue())
    }

    @Test
    fun `WHEN foreground tab gets killed THEN middleware records foreground age`() {
        store.dispatch(TabListAction.RestoreAction(
            listOf(
                createTab("https://www.mozilla.org", id = "foreground"),
                createTab("https://getpocket.com", id = "background_pocket"),
                createTab("https://theverge.com", id = "background_verge")
            ),
            selectedTabId = "foreground"
        )).joinBlocking()

        clock.elapsedTime = 100

        store.dispatch(EngineAction.LinkEngineSessionAction(
            tabId = "foreground",
            engineSession = mock()
        )).joinBlocking()

        assertFalse(EngineMetrics.killForegroundAge.testHasValue())
        assertFalse(EngineMetrics.killBackgroundAge.testHasValue())

        clock.elapsedTime = 500

        store.dispatch(
            EngineAction.KillEngineSessionAction("foreground")
        ).joinBlocking()

        assertTrue(EngineMetrics.killForegroundAge.testHasValue())
        assertFalse(EngineMetrics.killBackgroundAge.testHasValue())
        assertEquals(400_000_000, EngineMetrics.killForegroundAge.testGetValue().sum)
    }

    @Test
    fun `WHEN background tab gets killed THEN middleware records background age`() {
        store.dispatch(TabListAction.RestoreAction(
            listOf(
                createTab("https://www.mozilla.org", id = "foreground"),
                createTab("https://getpocket.com", id = "background_pocket"),
                createTab("https://theverge.com", id = "background_verge")
            ),
            selectedTabId = "foreground"
        )).joinBlocking()

        clock.elapsedTime = 100

        store.dispatch(EngineAction.LinkEngineSessionAction(
            tabId = "background_pocket",
            engineSession = mock()
        )).joinBlocking()

        clock.elapsedTime = 700

        assertFalse(EngineMetrics.killForegroundAge.testHasValue())
        assertFalse(EngineMetrics.killBackgroundAge.testHasValue())

        store.dispatch(
            EngineAction.KillEngineSessionAction("background_pocket")
        ).joinBlocking()

        assertTrue(EngineMetrics.killBackgroundAge.testHasValue())
        assertFalse(EngineMetrics.killForegroundAge.testHasValue())
        assertEquals(600_000_000, EngineMetrics.killBackgroundAge.testGetValue().sum)
    }
}

internal class FakeClock : Clock.Delegate {
    var elapsedTime: Long = 0
    override fun elapsedRealtime(): Long = elapsedTime
}
