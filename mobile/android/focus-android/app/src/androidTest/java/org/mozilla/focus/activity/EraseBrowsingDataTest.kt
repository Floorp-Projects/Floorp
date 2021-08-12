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
import org.junit.Assert
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.notificationTray
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.pressHomeKey
import org.mozilla.focus.helpers.TestHelper.progressBar
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.restartApp
import org.mozilla.focus.helpers.TestHelper.verifySnackBarText
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime
import org.mozilla.focus.testAnnotations.SmokeTest
import org.mozilla.focus.utils.FeatureFlags
import java.io.IOException

// This test erases browsing data and checks for message
@RunWith(AndroidJUnit4ClassRunner::class)
class EraseBrowsingDataTest {
    private lateinit var webServer: MockWebServer

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

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
    }

    @After
    fun tearDown() {
        try {
            webServer.close()
            webServer.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
    }

    @SmokeTest
    @Test
    fun trashButtonTest() {
        // Establish feedback message id
        val feedbackEraseId = if (FeatureFlags.isMvp)
            R.string.feedback_erase2
        else
            R.string.feedback_erase

        // Open a webpage
        searchScreen {
        }.loadPage(webServer.url("").toString()) {
            verifyPageContent("focus test page")
            // Press erase button, and check for message and return to the main page
        }.clearBrowsingData {
            verifySnackBarText(getStringResource(feedbackEraseId))
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

        // Establish feedback message id
        val feedbackEraseId = if (FeatureFlags.isMvp)
            R.string.feedback_erase2
        else
            R.string.feedback_erase

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
            verifySnackBarText(getStringResource(feedbackEraseId))
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

    @Ignore("Ignore until https://github.com/mozilla-mobile/focus-android/issues/4788 is solved")
    @SmokeTest
    @Test
    fun systemBarHomeViewTest() {
        val LAUNCH_TIMEOUT = 5000

        // Open a webpage
        searchScreen {
            typeInSearchBar(webServer.url("").toString())
            pressEnterKey()
            progressBar.waitUntilGone(webPageLoadwaitingTime)
        }
        // Switch out of Focus, pull down system bar and select delete browsing history
        pressHomeKey()
        mDevice.openNotification()
        notificationTray {
            verifySystemNotificationExists(getStringResource(R.string.notification_erase_text))
        }

        TestHelper.notificationBarDeleteItem.waitForExists(waitingTime)
        TestHelper.notificationBarDeleteItem.click()

        // Wait for launcher
        val launcherPackage = mDevice.launcherPackageName
        Assert.assertThat(launcherPackage, IsNull.notNullValue())
        mDevice.wait(
            Until.hasObject(By.pkg(launcherPackage).depth(0)),
            LAUNCH_TIMEOUT.toLong()
        )

        // Launch the app
        mActivityTestRule.launchActivity(Intent(Intent.ACTION_MAIN))
        // Verify that it's on the main view, not showing the previous browsing session
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        assertTrue(TestHelper.inlineAutocompleteEditText.exists())
    }
}
