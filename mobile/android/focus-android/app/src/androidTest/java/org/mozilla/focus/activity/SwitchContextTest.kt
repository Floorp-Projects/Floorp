/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.hamcrest.core.IsNull
import org.junit.After
import org.junit.Assert.assertThat
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.notificationTray
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.pressHomeKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

// This test switches out of Focus and opens it from the private browsing notification
@RunWith(AndroidJUnit4ClassRunner::class)
class SwitchContextTest {
    private lateinit var webServer: MockWebServer

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        webServer = MockWebServer()
        try {
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("image_test.html"))
                    .addHeader(
                        "Set-Cookie",
                        "sphere=battery; Expires=Wed, 21 Oct 2035 07:28:00 GMT;"
                    )
            )
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("rabbit.jpg"))
            )
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("download.jpg"))
            )
            webServer.start()
        } catch (e: IOException) {
            throw AssertionError("Could not start web server", e)
        }

        notificationTray {
            mDevice.openNotification()
            clearNotifications()
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
    fun notificationOpenButtonTest() {
        // Open a webpage
        searchScreen {
        }.loadPage(webServer.url("").toString()) { }
        // Send app to background
        pressHomeKey()
        // Pull down system bar and select Open
        mDevice.openNotification()
        notificationTray {
            verifySystemNotificationExists(getStringResource(R.string.notification_erase_text))
            expandEraseBrowsingNotification()
        }.clickNotificationOpenButton {
            verifyBrowserView()
        }
    }

    @Test
    fun switchFromSettingsToFocusTest() {
        // Initialize UiDevice instance
        val LAUNCH_TIMEOUT = 5000
        val SETTINGS_APP = "com.android.settings"
        val settingsApp = mDevice.findObject(
            UiSelector()
                .packageName(SETTINGS_APP)
                .enabled(true)
        )
        val launcherPackage = mDevice.launcherPackageName
        val context = InstrumentationRegistry.getInstrumentation().targetContext.applicationContext
        val intent = context.packageManager
            .getLaunchIntentForPackage(SETTINGS_APP)

        // Open a webpage
        searchScreen {
        }.loadPage(webServer.url("").toString()) { }

        // Switch out of Focus, open settings app
        pressHomeKey()

        // Wait for launcher
        assertThat(launcherPackage, IsNull.notNullValue())
        mDevice.wait(
            Until.hasObject(By.pkg(launcherPackage).depth(0)),
            LAUNCH_TIMEOUT.toLong()
        )

        // Launch the app
        context.startActivity(intent)
        settingsApp.waitForExists(waitingTime)
        assertTrue(settingsApp.exists())

        // switch to Focus
        mDevice.openNotification()
        notificationTray {
            verifySystemNotificationExists(getStringResource(R.string.notification_erase_text))
            expandEraseBrowsingNotification()
        }.clickNotificationOpenButton {
            verifyBrowserView()
        }
    }
}
