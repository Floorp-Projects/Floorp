/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import junit.framework.TestCase
import mozilla.components.browser.state.selector.privateTabs
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.browserScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.ext.components
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
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

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    @Throws(Exception::class)
    fun startWebServer() {
        webServer = MockWebServer()
        webServer.enqueue(createMockResponseFromAsset("tab1.html"))
        webServer.enqueue(createMockResponseFromAsset("tab2.html"))
        webServer.start()
    }

    @After
    @Throws(Exception::class)
    fun stopWebServer() {
        webServer.shutdown()
    }

    @Ignore("Failing on Firebase, see: https://github.com/mozilla-mobile/focus-android/issues/4996")
    @SmokeTest
    @Test
    fun testVisitingMultipleSites() {
        val firstPageUrl = webServer.url("tab1.html").toString()
        val firstPageTitle = webServer.hostName + "/tab1.html"
        val secondPageTitle = webServer.hostName + "/tab2.html"
        val eraseBrowsingButtonText = getStringResource(R.string.tabs_tray_action_erase)

        // Load website: Erase button visible, Tabs button not
        searchScreen {
        }.loadPage(firstPageUrl) {
            verifyPageContent("Tab 1")
            verifyFloatingEraseButton()
        }

        // Open link in new tab: Erase button hidden, Tabs button visible
        browserScreen {
            longPressLink("Tab 2")
            openLinkInNewTab()
            verifySnackBarText("New private tab opened")
            clickSnackBarActionButton("SWITCH")
            verifyNumberOfTabsOpened(2)

            // Open tabs tray and switch to first tab.
            openTabsTray()
            verifyTabsOrder(firstPageTitle, secondPageTitle, eraseBrowsingButtonText)
            selectTab(firstPageTitle)
            verifyPageContent("Tab 1")

        // Remove all tabs via the tabs tray
        }.eraseBrowsingHistoryFromTabsTray {
            verifySnackBarText(getStringResource(R.string.feedback_erase2))
            TestCase.assertTrue(store.state.privateTabs.isEmpty())
        }
    }
}
