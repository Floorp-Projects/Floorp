/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.navigation.NavController
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.infobanner.DynamicInfoBanner
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class OpenInAppOnboardingObserverTest {

    private lateinit var store: BrowserStore
    private lateinit var lifecycleOwner: MockedLifecycleOwner
    private lateinit var openInAppOnboardingObserver: OpenInAppOnboardingObserver
    private lateinit var navigationController: NavController
    private lateinit var settings: Settings
    private lateinit var appLinksUseCases: AppLinksUseCases
    private lateinit var context: Context
    private lateinit var container: ViewGroup
    private lateinit var infoBanner: DynamicInfoBanner

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setUp() {
        store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(url = "https://www.mozilla.org", id = "1")
                ),
                selectedTabId = "1"
            )
        )
        lifecycleOwner = MockedLifecycleOwner(Lifecycle.State.STARTED)
        navigationController = mockk(relaxed = true)
        settings = mockk(relaxed = true)
        appLinksUseCases = mockk(relaxed = true)
        container = mockk(relaxed = true)
        context = mockk(relaxed = true)
        infoBanner = mockk(relaxed = true)
        openInAppOnboardingObserver = spyk(
            OpenInAppOnboardingObserver(
                context = context,
                store = store,
                lifecycleOwner = lifecycleOwner,
                navController = navigationController,
                settings = settings,
                appLinksUseCases = appLinksUseCases,
                container = container,
                shouldScrollWithTopToolbar = true
            )
        )
        every { openInAppOnboardingObserver.createInfoBanner() } returns infoBanner
    }

    @After
    fun teardown() {
        openInAppOnboardingObserver.stop()
    }

    @Test
    fun `GIVEN user configured to open links in external app WHEN page finishes loading THEN do not show banner`() {
        every { settings.openLinksInExternalApp } returns true
        every { settings.shouldShowOpenInAppCfr } returns true
        every { appLinksUseCases.appLinkRedirect.invoke(any()).hasExternalApp() } returns true
        store.dispatch(ContentAction.UpdateLoadingStateAction("1", true)).joinBlocking()

        openInAppOnboardingObserver.start()
        store.dispatch(ContentAction.UpdateLoadingStateAction("1", false)).joinBlocking()
        verify(exactly = 0) { infoBanner.showBanner() }
    }

    @Test
    fun `GIVEN user has not configured to open links in external app WHEN page finishes loading THEN show banner`() {
        every { settings.openLinksInExternalApp } returns false
        every { settings.shouldShowOpenInAppCfr } returns true
        every { appLinksUseCases.appLinkRedirect.invoke(any()).hasExternalApp() } returns true
        store.dispatch(ContentAction.UpdateLoadingStateAction("1", true)).joinBlocking()

        openInAppOnboardingObserver.start()

        store.dispatch(ContentAction.UpdateLoadingStateAction("1", false)).joinBlocking()
        verify(exactly = 1) { infoBanner.showBanner() }
    }

    @Test
    fun `GIVEN banner was already displayed WHEN page finishes loading THEN do not show banner`() {
        every { settings.openLinksInExternalApp } returns false
        every { settings.shouldShowOpenInAppCfr } returns false
        every { appLinksUseCases.appLinkRedirect.invoke(any()).hasExternalApp() } returns true
        store.dispatch(ContentAction.UpdateLoadingStateAction("1", true)).joinBlocking()

        openInAppOnboardingObserver.start()
        store.dispatch(ContentAction.UpdateLoadingStateAction("1", false)).joinBlocking()
        verify(exactly = 0) { infoBanner.showBanner() }
    }

    @Test
    fun `GIVEN banner should be displayed WHEN no application found THEN do not show banner`() {
        every { settings.openLinksInExternalApp } returns false
        every { settings.shouldShowOpenInAppCfr } returns true
        every { appLinksUseCases.appLinkRedirect.invoke(any()).hasExternalApp() } returns false
        store.dispatch(ContentAction.UpdateLoadingStateAction("1", true)).joinBlocking()

        openInAppOnboardingObserver.start()

        store.dispatch(ContentAction.UpdateLoadingStateAction("1", false)).joinBlocking()
        verify(exactly = 0) { infoBanner.showBanner() }
    }

    @Test
    fun `GIVEN banner is displayed WHEN user navigates to different domain THEN banner is dismissed`() {
        every { settings.openLinksInExternalApp } returns false
        every { settings.shouldShowOpenInAppCfr } returns true
        every { appLinksUseCases.appLinkRedirect.invoke(any()).hasExternalApp() } returns true
        every { context.components.analytics.metrics.track(any()) } just runs
        store.dispatch(ContentAction.UpdateLoadingStateAction("1", true)).joinBlocking()

        openInAppOnboardingObserver.start()

        store.dispatch(ContentAction.UpdateLoadingStateAction("1", false)).joinBlocking()
        verify(exactly = 1) { infoBanner.showBanner() }
        verify(exactly = 0) { infoBanner.dismiss() }

        store.dispatch(ContentAction.UpdateUrlAction("1", "https://www.mozilla.org/en-US/")).joinBlocking()
        verify(exactly = 0) { infoBanner.dismiss() }

        store.dispatch(ContentAction.UpdateUrlAction("1", "https://www.firefox.com")).joinBlocking()
        verify(exactly = 1) { infoBanner.dismiss() }
    }

    @Test
    fun `GIVEN a observer WHEN createInfoBanner() THEN the scrollWithTopToolbar is passed to the DynamicInfoBanner`() {
        // Mockk currently doesn't support verifying constructor parameters
        // But we can check the values found in the constructed objects

        openInAppOnboardingObserver = spyk(
            OpenInAppOnboardingObserver(
                testContext, mockk(), mockk(), mockk(), mockk(), mockk(), FrameLayout(testContext), shouldScrollWithTopToolbar = true
            )
        )
        val banner1 = openInAppOnboardingObserver.createInfoBanner()
        assertTrue(banner1.shouldScrollWithTopToolbar)

        openInAppOnboardingObserver = spyk(
            OpenInAppOnboardingObserver(
                testContext, mockk(), mockk(), mockk(), mockk(), mockk(), FrameLayout(testContext), shouldScrollWithTopToolbar = false
            )
        )
        val banner2 = openInAppOnboardingObserver.createInfoBanner()
        assertFalse(banner2.shouldScrollWithTopToolbar)
    }

    internal class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        val lifecycleRegistry = LifecycleRegistry(this).apply {
            currentState = initialState
        }

        override fun getLifecycle(): Lifecycle = lifecycleRegistry
    }
}
