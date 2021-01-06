/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import android.view.View
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.spyk
import io.mockk.verify
import io.mockk.mockk
import io.mockk.impl.annotations.MockK
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TrackingProtectionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionOverlayTest {

    private lateinit var context: Context
    @MockK(relaxed = true) private lateinit var settings: Settings
    @MockK(relaxed = true) private lateinit var metrics: MetricController
    @MockK(relaxed = true) private lateinit var toolbar: View
    @MockK(relaxed = true) private lateinit var icon: View
    @MockK(relaxed = true) private lateinit var session: SessionState
    @MockK(relaxed = true) private lateinit var overlay: TrackingProtectionOverlay

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)
    private lateinit var store: BrowserStore

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        context = spyk(testContext)
        store = BrowserStore()
        val lifecycleOwner = MockedLifecycleOwner(Lifecycle.State.STARTED)

        overlay = spyk(
            TrackingProtectionOverlay(
                context,
                settings,
                metrics,
                store,
                lifecycleOwner
            ) { toolbar })
        every { toolbar.findViewById<View>(R.id.mozac_browser_toolbar_tracking_protection_indicator) } returns icon
    }

    @After
    fun cleanUp() {
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `WHEN loading state changes THEN overlay is notified`() {
        val tab = createTab("mozilla.org")
        every { overlay.onLoadingStateChanged(tab) } returns Unit

        overlay.start()

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(tab.id)).joinBlocking()
        store.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, false)).joinBlocking()

        val selectedTab = store.state.findTab(tab.id)!!

        verify(exactly = 1) {
            overlay.onLoadingStateChanged(selectedTab)
        }
    }

    @Test
    fun `WHEN overlay is stopped THEN listeners must be unsubscribed`() {
        every { overlay.cancelScope() } returns Unit

        overlay.stop()

        verify(exactly = 1) {
            overlay.cancelScope()
        }
    }

    @Test
    fun `no-op when loading`() {
        val trackingProtection =
            TrackingProtectionState(enabled = true, blockedTrackers = listOf(mockk()))
        every { settings.shouldShowTrackingProtectionCfr } returns true
        every { session.trackingProtection } returns trackingProtection
        every { session.content.loading } returns true

        overlay.onLoadingStateChanged(session)
        verify(exactly = 0) { settings.incrementTrackingProtectionOnboardingCount() }
    }

    @Test
    fun `no-op when should not show onboarding`() {
        every { settings.shouldShowTrackingProtectionCfr } returns false

        every { session.content.loading } returns false

        overlay.onLoadingStateChanged(session)
        verify(exactly = 0) { settings.incrementTrackingProtectionOnboardingCount() }
    }

    @Test
    fun `no-op when tracking protection disabled`() {
        every { settings.shouldShowTrackingProtectionCfr } returns true
        every { session.trackingProtection } returns TrackingProtectionState(enabled = false)
        every { session.content.loading } returns false

        overlay.onLoadingStateChanged(session)
        verify(exactly = 0) { settings.incrementTrackingProtectionOnboardingCount() }
    }

    @Test
    fun `no-op when no trackers blocked`() {
        every { settings.shouldShowTrackingProtectionCfr } returns true
        every { session.content.loading } returns false
        every { session.trackingProtection } returns TrackingProtectionState(
            enabled = true,
            blockedTrackers = emptyList()
        )

        overlay.onLoadingStateChanged(session)
        verify(exactly = 0) { settings.incrementTrackingProtectionOnboardingCount() }
    }

    @Test
    fun `show onboarding when trackers are blocked`() {
        every { toolbar.hasWindowFocus() } returns true
        every { settings.shouldShowTrackingProtectionCfr } returns true
        every { session.content.loading } returns false
        every { session.trackingProtection } returns TrackingProtectionState(
            enabled = true,
            blockedTrackers = listOf(mockk())
        )
        overlay.onLoadingStateChanged(session)
        verify { settings.incrementTrackingProtectionOnboardingCount() }
    }

    @Test
    fun `no-op when toolbar doesn't have focus`() {
        every { toolbar.hasWindowFocus() } returns false
        every { settings.shouldShowTrackingProtectionCfr } returns true
        every { session.content.loading } returns false
        every { session.trackingProtection } returns TrackingProtectionState(
            enabled = true,
            blockedTrackers = listOf(mockk())
        )
        overlay.onLoadingStateChanged(session)

        overlay.onLoadingStateChanged(session)
        verify(exactly = 0) { settings.incrementTrackingProtectionOnboardingCount() }
    }

    internal class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        private val lifecycleRegistry = LifecycleRegistry(this).apply {
            currentState = initialState
        }

        override fun getLifecycle(): Lifecycle = lifecycleRegistry
    }
}
