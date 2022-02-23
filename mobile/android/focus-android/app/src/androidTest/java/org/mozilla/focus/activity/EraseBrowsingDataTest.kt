/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.content.Intent
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Until
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.hamcrest.core.IsNull
import org.junit.After
import org.junit.Assert.assertThat
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.notificationTray
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.pressHomeKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.restartApp
import org.mozilla.focus.helpers.TestHelper.verifySnackBarText
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

// These tests verify interaction with the browsing notification and erasing browsing data
@RunWith(AndroidJUnit4ClassRunner::class)
class EraseBrowsingDataTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        webServer = MockWebServer()
        try {
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("plain_test.html"))
            )
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("plain_test.html"))
            )
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("plain_test.html"))
            )
            webServer.start()
        } catch (e: IOException) {
            throw AssertionError("Could not start web server", e)
        }
        featureSettingsHelper.setShieldIconCFREnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
    }

    @After
    fun tearDown() {
        try {
            webServer.close()
            webServer.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun trashButtonTest() {
        // Open a webpage
        searchScreen {
        }.loadPage(webServer.url("").toString()) {
            verifyPageContent("focus test page")
            // Press erase button, and check for message and return to the main page
        }.clearBrowsingData {
            verifySnackBarText(getStringResource(R.string.feedback_erase2))
            verifyEmptySearchBar()
        }
    }

    @SmokeTest
    @Test
    fun notificationEraseAndOpenButtonTest() {
        notificationTray {
            mDevice.openNotification()
            clearNotifications()
        }
        // Open a webpage
        searchScreen {
        }.loadPage(webServer.url("").toString()) { }
        // Send app to background
        pressHomeKey()
        // Pull down system bar and select Erase and Open
        mDevice.openNotification()
        notificationTray {
            verifySystemNotificationExists(getStringResource(R.string.notification_erase_text))
            expandEraseBrowsingNotification()
        }.clickEraseAndOpenNotificationButton {
            verifySnackBarText(getStringResource(R.string.feedback_erase2))
            verifyEmptySearchBar()
        }
    }

    @SmokeTest
    @Test
    fun deleteHistoryOnRestartTest() {
        searchScreen {
        }.loadPage(webServer.url("").toString()) {}
        restartApp(mActivityTestRule)
        homeScreen {
            verifyEmptySearchBar()
        }
    }

    @SmokeTest
    @Test
    fun systemBarHomeViewTest() {
        val LAUNCH_TIMEOUT = 5000
        val launcherPackage = mDevice.launcherPackageName

        notificationTray {
            mDevice.openNotification()
            clearNotifications()
        }

        searchScreen {
        }.loadPage(webServer.url("").toString()) { }

        // Switch out of Focus, pull down system bar and select delete browsing history
        pressHomeKey()
        mDevice.openNotification()
        notificationTray {
            verifySystemNotificationExists(getStringResource(R.string.notification_erase_text))
            expandEraseBrowsingNotification()
        }.clickNotificationMessage {
            // Wait for launcher
            assertThat(launcherPackage, IsNull.notNullValue())
            mDevice.wait(
                Until.hasObject(By.pkg(launcherPackage).depth(0)),
                LAUNCH_TIMEOUT.toLong()
            )

            // Re-launch the app, verify it's not showing the previous browsing session
            mActivityTestRule.launchActivity(Intent(Intent.ACTION_MAIN))
            verifyEmptySearchBar()
        }
    }
}
