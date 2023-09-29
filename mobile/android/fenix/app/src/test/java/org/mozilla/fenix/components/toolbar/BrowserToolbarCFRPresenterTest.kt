/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.View
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.ShoppingProductAction
import mozilla.components.browser.state.action.TabListAction
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
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.shopping.ShoppingExperienceFeature
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

        assertNotNull(presenter.scope)

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

        assertNotNull(presenter.scope)

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

        assertNotNull(presenter.scope)

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

        assertNotNull(presenter.scope)

        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 99)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        verify(exactly = 1) { presenter.showTcpCfr() }
    }

    @Test
    fun `GIVEN the Erase CFR should be shown WHEN in private mode and the current tab is fully loaded THEN the Erase CFR is only shown once`() {
        val tab = createTab(url = "", private = true)

        val browserStore = createBrowserStore(
            tab = tab,
            selectedTabId = tab.id,
        )

        val presenter = createPresenterThatShowsCFRs(
            browserStore = browserStore,
            settings = mockk {
                every { shouldShowEraseActionCFR } returns true
            },
            isPrivate = true,
        )

        presenter.start()

        assertNotNull(presenter.scope)

        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 99)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        verify { presenter.showEraseCfr() }
    }

    @Test
    fun `GIVEN no CFR shown WHEN the feature starts THEN don't observe the store for updates`() {
        val presenter = createPresenter(
            settings = mockk {
                every { shouldShowTotalCookieProtectionCFR } returns false
                every { shouldShowCookieBannerReEngagementDialog() } returns false
                every { shouldShowReviewQualityCheckCFR } returns false
                every { shouldShowEraseActionCFR } returns false
            },
        )

        presenter.start()

        assertNull(presenter.scope)
    }

    @Test
    fun `GIVEN the store is observed for updates WHEN the presenter is stopped THEN stop observing the store`() {
        val scope: CoroutineScope = mockk {
            every { cancel() } just Runs
        }
        val presenter = createPresenter()
        presenter.scope = scope

        presenter.stop()

        verify { scope.cancel() }
    }

    @Test
    fun `WHEN the TCP CFR is to be shown THEN instantiate a new one and remember show it again unless explicitly dismissed`() {
        val settings: Settings = mockk(relaxed = true)
        val presenter = createPresenter(
            anchor = mockk(relaxed = true),
            settings = settings,
        )

        presenter.showTcpCfr()

        verify(exactly = 0) { settings.shouldShowTotalCookieProtectionCFR = false }
        assertNotNull(presenter.popup)
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
    fun `GIVEN the current tab is showing a product page WHEN the tab is not loading THEN the CFR is shown`() {
        val tab = createTab(url = "")
        val browserStore = createBrowserStore(
            tab = tab,
            selectedTabId = tab.id,
        )
        val presenter = createPresenter(
            browserStore = browserStore,
            settings = mockk {
                every { shouldShowTotalCookieProtectionCFR } returns false
                every { shouldShowReviewQualityCheckCFR } returns true
                every { reviewQualityCheckOptInTimeInMillis } returns System.currentTimeMillis()
                every { shouldShowEraseActionCFR } returns false
            },
        )
        every { presenter.showShoppingCFR(any()) } just Runs

        presenter.start()

        assertNotNull(presenter.scope)

        browserStore.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, true)).joinBlocking()
        verify(exactly = 0) { presenter.showShoppingCFR(eq(false)) }

        browserStore.dispatch(ShoppingProductAction.UpdateProductUrlStatusAction(tab.id, true)).joinBlocking()
        verify(exactly = 0) { presenter.showShoppingCFR(eq(false)) }

        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        verify(exactly = 0) { presenter.showShoppingCFR(eq(false)) }

        browserStore.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, false)).joinBlocking()
        verify { presenter.showShoppingCFR(eq(false)) }
    }

    @Test
    fun `GIVEN the current tab is showing a product page WHEN the tab is not loading AND another CFR is shown THEN the shopping CFR is not shown`() {
        val tab = createTab(url = "")
        val browserStore = createBrowserStore(
            tab = tab,
            selectedTabId = tab.id,
        )
        val presenter = createPresenter(
            browserStore = browserStore,
            settings = mockk {
                every { shouldShowTotalCookieProtectionCFR } returns false
                every { shouldShowReviewQualityCheckCFR } returns true
                every { reviewQualityCheckOptInTimeInMillis } returns System.currentTimeMillis()
                every { shouldShowEraseActionCFR } returns false
            },
        )
        every { presenter.popup } returns mockk()
        every { presenter.showShoppingCFR(any()) } just Runs

        presenter.start()

        assertNotNull(presenter.scope)

        browserStore.dispatch(ShoppingProductAction.UpdateProductUrlStatusAction(tab.id, true)).joinBlocking()
        verify(exactly = 0) { presenter.showShoppingCFR(eq(false)) }

        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        verify(exactly = 0) { presenter.showShoppingCFR(eq(false)) }
    }

    @Test
    fun `GIVEN the user opted in the shopping feature AND the opted in shopping CFR should be shown WHEN the tab finishes loading THEN the CFR is shown`() {
        val tab = createTab(url = "")
        val browserStore = createBrowserStore(
            tab = tab,
            selectedTabId = tab.id,
        )

        val presenter = createPresenter(
            settings = mockk {
                every { shouldShowTotalCookieProtectionCFR } returns false
                every { shouldShowReviewQualityCheckCFR } returns true
                every { shouldShowEraseActionCFR } returns false
                every { reviewQualityCheckOptInTimeInMillis } returns System.currentTimeMillis() - Settings.TWO_DAYS_MS
            },
            browserStore = browserStore,
        )
        every { presenter.showShoppingCFR(any()) } just Runs

        presenter.start()

        assertNotNull(presenter.scope)

        browserStore.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, true)).joinBlocking()
        verify(exactly = 0) { presenter.showShoppingCFR(eq(true)) }

        browserStore.dispatch(ShoppingProductAction.UpdateProductUrlStatusAction(tab.id, true)).joinBlocking()
        verify(exactly = 0) { presenter.showShoppingCFR(eq(true)) }

        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        verify(exactly = 0) { presenter.showShoppingCFR(eq(true)) }

        browserStore.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, false)).joinBlocking()
        verify { presenter.showShoppingCFR(eq(true)) }
    }

    @Test
    fun `GIVEN the user opted in the shopping feature AND the opted in shopping CFR should be shown WHEN opening a loaded product page THEN the CFR is shown`() {
        val tab1 = createTab(url = "", id = "tab1")
        val tab2 = createTab(url = "", id = "tab2")
        val browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab1, tab2),
                selectedTabId = tab2.id,
            ),
        )

        val presenter = createPresenter(
            settings = mockk {
                every { shouldShowTotalCookieProtectionCFR } returns false
                every { shouldShowReviewQualityCheckCFR } returns true
                every { shouldShowEraseActionCFR } returns false
                every { reviewQualityCheckOptInTimeInMillis } returns System.currentTimeMillis() - Settings.TWO_DAYS_MS
            },
            browserStore = browserStore,
        )
        every { presenter.showShoppingCFR(any()) } just Runs

        presenter.start()

        assertNotNull(presenter.scope)

        browserStore.dispatch(ShoppingProductAction.UpdateProductUrlStatusAction(tab1.id, true)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab1.id, 100)).joinBlocking()
        verify(exactly = 0) { presenter.showShoppingCFR(any()) }

        browserStore.dispatch(TabListAction.SelectTabAction(tab1.id)).joinBlocking()
        verify(exactly = 1) { presenter.showShoppingCFR(any()) }
    }

    /**
     * Creates and return a [spyk] of a [BrowserToolbarCFRPresenter] that can handle actually showing CFRs.
     */
    private fun createPresenterThatShowsCFRs(
        context: Context = mockk(),
        anchor: View = mockk(),
        browserStore: BrowserStore = mockk(),
        settings: Settings = mockk {
            every { shouldShowTotalCookieProtectionCFR } returns true
            every { shouldShowCookieBannerReEngagementDialog() } returns false
            every { shouldShowReviewQualityCheckCFR } returns false
            every { shouldShowEraseActionCFR } returns false
        },
        toolbar: BrowserToolbar = mockk(),
        isPrivate: Boolean = false,
        sessionId: String? = null,
    ) = spyk(createPresenter(context, anchor, browserStore, settings, toolbar, sessionId, isPrivate)) {
        every { showTcpCfr() } just Runs
        every { showShoppingCFR(any()) } just Runs
        every { showEraseCfr() } just Runs
    }

    /**
     * Create and return a [BrowserToolbarCFRPresenter] with all constructor properties mocked by default.
     * Calls to show a CFR will fail. If this behavior is needed to work use [createPresenterThatShowsCFRs].
     */
    private fun createPresenter(
        context: Context = mockk {
            every { getString(R.string.tcp_cfr_message) } returns "Test"
            every { getColor(any()) } returns 0
            every { getString(R.string.pref_key_should_show_review_quality_cfr) } returns "test"
        },
        anchor: View = mockk(relaxed = true),
        browserStore: BrowserStore = mockk(),
        settings: Settings = mockk(relaxed = true) {
            every { shouldShowTotalCookieProtectionCFR } returns true
            every { shouldShowCookieBannerReEngagementDialog() } returns false
            every { shouldShowEraseActionCFR } returns true
            every { shouldShowReviewQualityCheckCFR } returns true
        },
        toolbar: BrowserToolbar = mockk {
            every { findViewById<View>(R.id.mozac_browser_toolbar_security_indicator) } returns anchor
            every { findViewById<View>(R.id.mozac_browser_toolbar_page_actions) } returns anchor
            every { findViewById<View>(R.id.mozac_browser_toolbar_navigation_actions) } returns anchor
        },
        sessionId: String? = null,
        isPrivate: Boolean = false,
        shoppingExperienceFeature: ShoppingExperienceFeature = mockk {
            every { isEnabled } returns true
        },
    ) = spyk(
        BrowserToolbarCFRPresenter(
            context = context,
            browserStore = browserStore,
            settings = settings,
            toolbar = toolbar,
            sessionId = sessionId,
            isPrivate = isPrivate,
            onShoppingCfrActionClicked = {},
            shoppingExperienceFeature = shoppingExperienceFeature,
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
