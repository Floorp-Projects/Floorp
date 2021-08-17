/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.app.PendingIntent
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.net.Uri
import androidx.browser.customtabs.CustomTabsIntent
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import junit.framework.TestCase.assertTrue
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.browserScreen
import org.mozilla.focus.activity.robots.customTab
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime
import java.io.IOException

@RunWith(AndroidJUnit4ClassRunner::class)
class CustomTabTest {
    private lateinit var webServer: MockWebServer
    private val MENU_ITEM_LABEL = "TestItem4223"
    private val ACTION_BUTTON_DESCRIPTION = "TestButton"
    private val TEST_PAGE_HEADER_TEXT = "focus test page"

    @get: Rule
    val activityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java, true, false
    )

    @Before
    fun setUp() {
        webServer = MockWebServer()
        webServer.enqueue(createMockResponseFromAsset("plain_test.html"))
        webServer.enqueue(createMockResponseFromAsset("tab1.html"))
        webServer.start()
    }

    @After
    fun tearDown() {
        try {
            webServer.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
    }

    @Test
    fun testCustomTabUI() {
        val customTabPage = webServer.url("plain_test.html").toString()
        val customTabActivity = activityTestRule.launchActivity(createCustomTabIntent(customTabPage))

        browserScreen {
            progressBar.waitUntilGone(webPageLoadwaitingTime)
            verifyPageContent(TEST_PAGE_HEADER_TEXT)
            verifyPageURL(customTabPage)
        }

        customTab {
            verifyCustomTabActionButton(ACTION_BUTTON_DESCRIPTION)
            verifyShareButtonIsDisplayed()
            openCustomTabMenu()
            verifyTheStandardMenuItems()
            verifyCustomMenuItem(MENU_ITEM_LABEL)
            // Close the menu and close the tab
            mDevice.pressBack()
            closeCustomTab()
            assertTrue(customTabActivity.isDestroyed)
        }
    }

    @Test
    fun openCustomTabInFocusTest() {
        val browserPage = webServer.url("plain_test.html").toString()
        val customTabPage = webServer.url("tab1.html").toString()

        activityTestRule.launchActivity(null)
        homeScreen {
            skipFirstRun()
        }

        searchScreen {
        }.loadPage(browserPage) {
            verifyPageURL(browserPage)
        }

        activityTestRule.launchActivity(createCustomTabIntent(customTabPage))
        customTab {
            progressBar.waitUntilGone(webPageLoadwaitingTime)
            verifyPageURL(customTabPage)
            openCustomTabMenu()
            clickOpenInFocusButton()
        }

        browserScreen {
            mDevice.waitForIdle(waitingTime)
            verifyNumberOfTabsOpened(2)
            mDevice.pressBack()
            verifyPageURL(browserPage)
        }
    }

    @Suppress("Deprecation")
    private fun createCustomTabIntent(pageUrl: String): Intent {
        val appContext = InstrumentationRegistry.getInstrumentation()
            .targetContext
            .applicationContext
        val pendingIntent = PendingIntent.getActivity(appContext, 0, Intent(), 0)
        val customTabsIntent = CustomTabsIntent.Builder()
            .addMenuItem(MENU_ITEM_LABEL, pendingIntent)
            .addDefaultShareMenuItem()
            .setActionButton(createTestBitmap(), ACTION_BUTTON_DESCRIPTION, pendingIntent, true)
            .setToolbarColor(Color.MAGENTA)
            .build()
        customTabsIntent.intent.data = Uri.parse(pageUrl)
        return customTabsIntent.intent
    }

    private fun createTestBitmap(): Bitmap {
        val bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        canvas.drawColor(Color.GREEN)
        return bitmap
    }
}
