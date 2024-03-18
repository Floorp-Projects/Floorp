/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchwidget

import android.app.Activity
import android.content.Context
import android.os.Bundle
import mozilla.components.browser.state.selector.allTabs
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.SessionState
import mozilla.components.feature.search.widget.BaseVoiceSearchActivity
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertNotNull
import org.junit.Rule
import org.junit.Test
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertNull
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.runner.RunWith
import org.mozilla.focus.GleanMetrics.SearchWidget
import org.mozilla.focus.TestFocusApplication
import org.mozilla.focus.activity.IntentReceiverActivity
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings
import org.mozilla.focus.perf.Performance
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.utils.SearchUtils
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.Implementation
import org.robolectric.annotation.Implements

@RunWith(RobolectricTestRunner::class)
@Config(application = TestFocusApplication::class)
internal class ExternalIntentNavigationTest {
    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val activity: Activity = Robolectric.buildActivity(Activity::class.java).setup().get()
    private val appStore = activity.components.appStore

    @Test
    fun `GIVEN the onboarding should be shown and the app is not used in performance tests WHEN the app is opened THEN show the onboarding`() {
        activity.settings.isFirstRun = true

        ExternalIntentNavigation.handleAppOpened(null, activity)
        appStore.waitUntilIdle()

        assertEquals(Screen.FirstRun, appStore.state.screen)
    }

    @Test
    @Config(shadows = [ShadowPerformance::class])
    fun `GIVEN the onboarding should be shown and the app is used in a performance test WHEN the app is opened THEN show the homescreen`() {
        // The AppStore is initialized before the test runs. By default isFirstRun is true. Simulate it being false.
        appStore.dispatch(AppAction.ShowHomeScreen)
        appStore.waitUntilIdle()
        activity.settings.isFirstRun = true

        ExternalIntentNavigation.handleAppOpened(null, activity)
        appStore.waitUntilIdle()

        assertEquals(Screen.Home, appStore.state.screen)
    }

    @Test
    @Config(shadows = [ShadowPerformance::class])
    fun `GIVEN the onboarding should not be shown and in a performance test WHEN the app is opened THEN show the home screen`() {
        // The AppStore is initialized before the test runs. By default isFirstRun is true. Simulate it being false.
        appStore.dispatch(AppAction.ShowHomeScreen)
        appStore.waitUntilIdle()
        activity.settings.isFirstRun = false

        ExternalIntentNavigation.handleAppOpened(null, activity)
        appStore.waitUntilIdle()

        assertEquals(Screen.Home, appStore.state.screen)
    }

    @Test
    fun `GIVEN the onboarding should not be shown and not in a performance test WHEN the app is opened THEN show the home screen`() {
        // The AppStore is initialized before the test runs. By default isFirstRun is true. Simulate it being false.
        appStore.dispatch(AppAction.ShowHomeScreen)
        appStore.waitUntilIdle()
        activity.settings.isFirstRun = false

        ExternalIntentNavigation.handleAppOpened(null, activity)
        appStore.waitUntilIdle()

        assertEquals(Screen.Home, appStore.state.screen)
    }

    @Test
    fun `GIVEN a tab is already open WHEN trying to navigate to the current tab THEN navigate to it and return true`() {
        activity.components.tabsUseCases.addTab(url = "https://mozilla.com")
        activity.components.store.waitUntilIdle()
        val result = ExternalIntentNavigation.handleBrowserTabAlreadyOpen(activity)
        activity.components.appStore.waitUntilIdle()

        assertTrue(result)
        val selectedTabId = activity.components.store.state.selectedTabId!!
        assertEquals(Screen.Browser(selectedTabId, false), activity.components.appStore.state.screen)
    }

    @Test
    fun `GIVEN no tabs are currently open WHEN trying to navigate to the current tab THEN navigate home and return false`() {
        // The AppStore is initialized before the test runs. By default isFirstRun is true. Simulate it being false.
        appStore.dispatch(AppAction.ShowHomeScreen)
        appStore.waitUntilIdle()
        val result = ExternalIntentNavigation.handleBrowserTabAlreadyOpen(activity)
        activity.components.appStore.waitUntilIdle()

        assertFalse(result)
        assertEquals(Screen.Home, activity.components.appStore.state.screen)
    }

    @Test
    fun `GIVEN a text search from the search widget WHEN handling widget interactions THEN record telemetry, show the home screen and return true`() {
        val bundle = Bundle().apply {
            putBoolean(IntentReceiverActivity.SEARCH_WIDGET_EXTRA, true)
        }

        val result = ExternalIntentNavigation.handleWidgetTextSearch(bundle, activity)
        appStore.waitUntilIdle()

        assertTrue(result)
        assertNotNull(SearchWidget.newTabButton.testGetValue())
        assertEquals(Screen.Home, appStore.state.screen)
    }

    @Test
    fun `GIVEN no text search from the search widget WHEN handling widget interactions THEN don't record telemetry, show the home screen and false true`() {
        // The AppStore is initialized before the test runs. By default isFirstRun is true. Simulate it being false.
        appStore.dispatch(AppAction.ShowHomeScreen)
        appStore.waitUntilIdle()
        val bundle = Bundle()

        val result = ExternalIntentNavigation.handleWidgetTextSearch(bundle, activity)
        appStore.waitUntilIdle()

        assertFalse(result)
        assertNull(SearchWidget.newTabButton.testGetValue())
        assertEquals(Screen.Home, appStore.state.screen)
    }

    @Test
    fun `GIVEN a voice search WHEN handling widget interactions THEN create and open a new tab and return true`() {
        val browserStore = activity.components.store
        val searchArgument = "test"
        val bundle = Bundle().apply {
            putString(BaseVoiceSearchActivity.SPEECH_PROCESSING, searchArgument)
        }

        val result = ExternalIntentNavigation.handleWidgetVoiceSearch(bundle, activity)

        assertTrue(result)
        browserStore.waitUntilIdle()
        assertEquals(1, browserStore.state.allTabs.size)
        assertEquals(1, browserStore.state.privateTabs.size)
        val voiceSearchTab = browserStore.state.privateTabs[0]
        assertEquals(voiceSearchTab, browserStore.state.findCustomTabOrSelectedTab())
        assertEquals(SearchUtils.createSearchUrl(activity, searchArgument), voiceSearchTab.content.url)
        assertEquals(SessionState.Source.External.ActionSend(null), voiceSearchTab.source)
        assertEquals(searchArgument, voiceSearchTab.content.searchTerms)
        appStore.waitUntilIdle()
        assertEquals(Screen.Browser(voiceSearchTab.id, false), appStore.state.screen)
    }

    @Test
    fun `GIVEN no voice search WHEN handling widget interactions THEN don't open a new tab and return false`() {
        // The AppStore is initialized before the test runs. By default isFirstRun is true. Simulate it being false.
        appStore.dispatch(AppAction.ShowHomeScreen)
        appStore.waitUntilIdle()
        val browserStore = activity.components.store
        val bundle = Bundle()

        val result = ExternalIntentNavigation.handleWidgetVoiceSearch(bundle, activity)

        assertFalse(result)
        browserStore.waitUntilIdle()
        assertEquals(0, browserStore.state.allTabs.size)
        appStore.waitUntilIdle()
        assertEquals(Screen.Home, appStore.state.screen)
    }
}

/**
 * Shadow of [Performance] that will have [processIntentIfPerformanceTest] always return `true`.
 */
@Implements(Performance::class)
class ShadowPerformance {
    @Implementation
    @Suppress("Unused_Parameter")
    fun processIntentIfPerformanceTest(bundle: Bundle?, context: Context) = true
}
