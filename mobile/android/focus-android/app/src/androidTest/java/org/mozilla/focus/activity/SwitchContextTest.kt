/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.notificationTray
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.TestAssetHelper
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.pressHomeKey
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

// This test switches out of Focus and opens it from the private browsing notification
@RunWith(AndroidJUnit4ClassRunner::class)
class SwitchContextTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }

        notificationTray {
            mDevice.openNotification()
            clearNotifications()
        }
    }

    @After
    fun tearDown() {
        try {
            webServer.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun notificationOpenButtonTest() {
        val testPage = TestAssetHelper.getGenericAsset(webServer)

        searchScreen {
        }.loadPage(testPage.url) {
            verifyPageContent(testPage.content)
        }
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

    @SmokeTest
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
        val testPage = TestAssetHelper.getGenericAsset(webServer)

        // Open a webpage
        searchScreen {
        }.loadPage(testPage.url) { }

        // Switch out of Focus, open settings app
        pressHomeKey()

        // Wait for launcher
        Assert.assertNotNull(launcherPackage)
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
