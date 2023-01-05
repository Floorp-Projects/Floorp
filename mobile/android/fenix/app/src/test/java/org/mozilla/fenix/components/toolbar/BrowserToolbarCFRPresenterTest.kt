/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.View
import androidx.compose.ui.unit.dp
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.cfr.CFRPopup
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class BrowserToolbarCFRPresenterTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `GIVEN the TCP CFR should be shown for a custom tab WHEN the custom tab is fully loaded THEN the TCP CFR is shown`() {
        val customTab = createCustomTab(url = "")
        val browserStore = createBrowserStore(customTab = customTab)
        val presenter = createPresenterThatShowsCFRs(
            browserStore = browserStore,
            sessionId = customTab.id,
        )

        presenter.start()

        assertNotNull(presenter.tcpCfrScope)

        browserStore.dispatch(ContentAction.UpdateProgressAction(customTab.id, 0)).joinBlocking()
        verify(exactly = 0) { presenter.showTcpCfr() }

        browserStore.dispatch(ContentAction.UpdateProgressAction(customTab.id, 33)).joinBlocking()
        verify(exactly = 0) { presenter.showTcpCfr() }

        browserStore.dispatch(ContentAction.UpdateProgressAction(customTab.id, 100)).joinBlocking()
        verify { presenter.showTcpCfr() }
    }

    @Test
    fun `GIVEN the TCP CFR should be shown WHEN the current normal tab is fully loaded THEN the TCP CFR is shown`() {
        val normalTab = createTab(url = "", private = false)
        val browserStore = createBrowserStore(
            tab = normalTab,
            selectedTabId = normalTab.id,
        )
        val presenter = createPresenterThatShowsCFRs(browserStore = browserStore)

        presenter.start()

        assertNotNull(presenter.tcpCfrScope)

        browserStore.dispatch(ContentAction.UpdateProgressAction(normalTab.id, 1)).joinBlocking()
        verify(exactly = 0) { presenter.showTcpCfr() }

        browserStore.dispatch(ContentAction.UpdateProgressAction(normalTab.id, 98)).joinBlocking()
        verify(exactly = 0) { presenter.showTcpCfr() }

        browserStore.dispatch(ContentAction.UpdateProgressAction(normalTab.id, 100)).joinBlocking()
        verify { presenter.showTcpCfr() }
    }

    @Test
    fun `GIVEN the TCP CFR should be shown WHEN the current private tab is fully loaded THEN the TCP CFR is shown`() {
        val privateTab = createTab(url = "", private = true)
        val browserStore = createBrowserStore(
            tab = privateTab,
            selectedTabId = privateTab.id,
        )
        val presenter = createPresenterThatShowsCFRs(browserStore = browserStore)

        presenter.start()

        assertNotNull(presenter.tcpCfrScope)

        browserStore.dispatch(ContentAction.UpdateProgressAction(privateTab.id, 14)).joinBlocking()
        verify(exactly = 0) { presenter.showTcpCfr() }

        browserStore.dispatch(ContentAction.UpdateProgressAction(privateTab.id, 99)).joinBlocking()
        verify(exactly = 0) { presenter.showTcpCfr() }

        browserStore.dispatch(ContentAction.UpdateProgressAction(privateTab.id, 100)).joinBlocking()
        verify { presenter.showTcpCfr() }
    }

    @Test
    fun `GIVEN the TCP CFR should be shown WHEN the current tab is fully loaded THEN the TCP CFR is only shown once`() {
        val tab = createTab(url = "")
        val browserStore = createBrowserStore(
            tab = tab,
            selectedTabId = tab.id,
        )
        val presenter = createPresenterThatShowsCFRs(browserStore = browserStore)

        presenter.start()

        assertNotNull(presenter.tcpCfrScope)

        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 99)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        verify(exactly = 1) { presenter.showTcpCfr() }
    }

    @Test
    fun `GIVEN the TCP CFR should not be shown WHEN the feature starts THEN don't observe the store for updates`() {
        val presenter = createPresenter(
            settings = mockk {
                every { shouldShowTotalCookieProtectionCFR } returns false
            },
        )

        presenter.start()

        assertNull(presenter.tcpCfrScope)
    }

    @Test
    fun `GIVEN the store is observed for updates WHEN the presenter is stopped THEN stop observing the store`() {
        val tcpScope: CoroutineScope = mockk {
            every { cancel() } just Runs
        }
        val presenter = createPresenter()
        presenter.tcpCfrScope = tcpScope

        presenter.stop()

        verify { tcpScope.cancel() }
    }

    @Test
    fun `WHEN the TCP CFR is to be shown THEN instantiate a new one and remember to not show it again`() {
        val settings: Settings = mockk(relaxed = true)
        val presenter = createPresenter(
            anchor = mockk(relaxed = true),
            settings = settings,
        )

        presenter.showTcpCfr()

        verify { settings.shouldShowTotalCookieProtectionCFR = false }
        assertNotNull(presenter.tcpCfrPopup)
    }

    @Test
    fun `WHEN the TCP CFR is instantiated THEN set the intended properties`() {
        val anchor: View = mockk(relaxed = true)
        val settings: Settings = mockk(relaxed = true)
        val presenter = createPresenter(
            anchor = anchor,
            settings = settings,
        )

        presenter.showTcpCfr()

        verify { settings.shouldShowTotalCookieProtectionCFR = false }
        assertNotNull(presenter.tcpCfrPopup)
        presenter.tcpCfrPopup?.let {
            assertEquals("Test", it.text)
            assertEquals(anchor, it.anchor)
            assertEquals(CFRPopup.DEFAULT_WIDTH.dp, it.properties.popupWidth)
            assertEquals(CFRPopup.IndicatorDirection.DOWN, it.properties.indicatorDirection)
            assertTrue(it.properties.dismissOnBackPress)
            assertTrue(it.properties.dismissOnClickOutside)
            assertFalse(it.properties.overlapAnchor)
            assertEquals(CFRPopup.DEFAULT_INDICATOR_START_OFFSET.dp, it.properties.indicatorArrowStartOffset)
        }
    }

    @Test
    fun `WHEN the TCP CFR is shown THEN log telemetry`() {
        val presenter = createPresenter(
            anchor = mockk(relaxed = true),
        )

        assertNull(TrackingProtection.tcpCfrShown.testGetValue())

        presenter.showTcpCfr()

        assertNotNull(TrackingProtection.tcpCfrShown.testGetValue())
    }

    @Test
    fun `WHEN the TCP CFR is dismissed THEN log telemetry`() {
        val presenter = createPresenter(
            anchor = mockk(relaxed = true),
        )
        every { presenter.tryToShowCookieBannerDialogIfNeeded() } just Runs

        presenter.showTcpCfr()

        assertNull(TrackingProtection.tcpCfrExplicitDismissal.testGetValue())
        presenter.tcpCfrPopup!!.onDismiss.invoke(true)
        assertNotNull(TrackingProtection.tcpCfrExplicitDismissal.testGetValue())

        assertNull(TrackingProtection.tcpCfrImplicitDismissal.testGetValue())
        presenter.tcpCfrPopup!!.onDismiss.invoke(false)
        assertNotNull(TrackingProtection.tcpCfrImplicitDismissal.testGetValue())
        verify {
            presenter.tryToShowCookieBannerDialogIfNeeded()
        }
    }

    /**
     * Creates and return a [spyk] of a [BrowserToolbarCFRPresenter] that can handle actually showing CFRs.
     */
    private fun createPresenterThatShowsCFRs(
        context: Context = mockk(),
        anchor: View = mockk(),
        browserStore: BrowserStore = mockk(),
        settings: Settings = mockk { every { shouldShowTotalCookieProtectionCFR } returns true },
        toolbar: BrowserToolbar = mockk(),
        sessionId: String? = null,
    ) = spyk(createPresenter(context, anchor, browserStore, settings, toolbar, sessionId)) {
        every { showTcpCfr() } just Runs
    }

    /**
     * Create and return a [BrowserToolbarCFRPresenter] with all constructor properties mocked by default.
     * Calls to show a CFR will fail. If this behavior is needed to work use [createPresenterThatShowsCFRs].
     */
    private fun createPresenter(
        context: Context = mockk { every { getString(R.string.tcp_cfr_message) } returns "Test" },
        anchor: View = mockk(),
        browserStore: BrowserStore = mockk(),
        settings: Settings = mockk(relaxed = true) { every { shouldShowTotalCookieProtectionCFR } returns true },
        toolbar: BrowserToolbar = mockk {
            every { findViewById<View>(R.id.mozac_browser_toolbar_security_indicator) } returns anchor
        },
        sessionId: String? = null,
    ) = spyk(
        BrowserToolbarCFRPresenter(
            context = context,
            browserStore = browserStore,
            settings = settings,
            toolbar = toolbar,
            sessionId = sessionId,
        ),
    )

    private fun createBrowserStore(
        tab: TabSessionState? = null,
        customTab: CustomTabSessionState? = null,
        selectedTabId: String? = null,
    ) = BrowserStore(
        initialState = BrowserState(
            tabs = if (tab != null) listOf(tab) else listOf(),
            customTabs = if (customTab != null) listOf(customTab) else listOf(),
            selectedTabId = selectedTabId,
        ),
    )
}
