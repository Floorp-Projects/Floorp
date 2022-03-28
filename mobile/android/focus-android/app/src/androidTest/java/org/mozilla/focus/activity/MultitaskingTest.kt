/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.core.app.launchActivity
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import mozilla.components.browser.state.selector.privateTabs
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.customTab
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.ext.components
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.clickSnackBarActionButton
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.verifySnackBarText
import org.mozilla.focus.testAnnotations.SmokeTest

/**
 * Open multiple sessions and verify that the trash icon changes to a tabs counter
 */
@RunWith(AndroidJUnit4ClassRunner::class)
class MultitaskingTest {
    private lateinit var webServer: MockWebServer
    private val store = InstrumentationRegistry.getInstrumentation()
        .targetContext
        .applicationContext
        .components
        .store
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    @Throws(Exception::class)
    fun startWebServer() {
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
        webServer = MockWebServer()
        webServer.enqueue(createMockResponseFromAsset("tab1.html"))
        webServer.enqueue(createMockResponseFromAsset("tab2.html"))
        webServer.enqueue(createMockResponseFromAsset("tab3.html"))
        webServer.start()
    }

    @After
    @Throws(Exception::class)
    fun tearDown() {
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @Ignore("Failing, see: https://github.com/mozilla-mobile/focus-android/issues/6709")
    @SmokeTest
    @Test
    fun testVisitingMultipleSites() {
        val tab1Url = webServer.url("tab1.html").toString()
        val tab1Title = webServer.hostName + "/tab1.html"
        val tab2Title = webServer.hostName + "/tab2.html"
        val tab2Url = webServer.url("tab2.html").toString()
        val tab3Title = webServer.hostName + "/tab3.html"
        val eraseBrowsingSnackBarText = getStringResource(R.string.feedback_erase2)
        val customTabPage = webServer.url("plain_test.html").toString()
        val customTabTitle = webServer.hostName + "/plain_test.html"

        // Load website: Erase button visible, Tabs button not
        searchScreen {
        }.loadPage(tab1Url) {
            verifyPageContent("Tab 1")
            longPressLink("Tab 2")
            verifyLinkContextMenu(tab2Url)
            openLinkInNewTab()
            verifyNumberOfTabsOpened(2)
            longPressLink("Tab 3")
            openLinkInNewTab()
            verifySnackBarText("New private tab opened")
            clickSnackBarActionButton("SWITCH")
            verifyNumberOfTabsOpened(3)
        }

        launchActivity<IntentReceiverActivity>(TestHelper.createCustomTabIntent(customTabPage))

        customTab {
            progressBar.waitUntilGone(TestHelper.waitingTime)
            verifyPageURL(customTabPage)
            openCustomTabMenu()
        }.clickOpenInFocusButton() {
            verifyNumberOfTabsOpened(4)
        }.openTabsTray {
            verifyTabsOrderAndCorespondingCloseButton(tab1Title, tab3Title, tab2Title, customTabTitle)
        }.selectTab(tab1Title) {
            verifyPageContent("Tab 1")
        }.clearBrowsingData {
            verifySnackBarText(eraseBrowsingSnackBarText)
            assertTrue(store.state.privateTabs.isEmpty())
        }
    }

    @Ignore("Failing, see: https://github.com/mozilla-mobile/focus-android/issues/6708")
    @SmokeTest
    @Test
    fun closeTabButtonTest() {
        val tab1Url = webServer.url("tab1.html").toString()
        val tab1Title = webServer.hostName + "/tab1.html"
        val tab2Title = webServer.hostName + "/tab2.html"
        val tab3Title = webServer.hostName + "/tab3.html"

        searchScreen {
        }.loadPage(tab1Url) {
            verifyPageContent("Tab 1")
            longPressLink("Tab 2")
            openLinkInNewTab()
            longPressLink("Tab 3")
            openLinkInNewTab()
            verifyNumberOfTabsOpened(3)
        }.openTabsTray {
            verifyTabsOrderAndCorespondingCloseButton(tab1Title, tab3Title, tab2Title)
        }.closeTab(tab1Title) {
        }.openTabsTray {
            verifyTabsOrderAndCorespondingCloseButton(tab3Title, tab2Title)
        }.closeTab(tab3Title) {
            verifyTabsCounterNotShown()
        }
    }
}
