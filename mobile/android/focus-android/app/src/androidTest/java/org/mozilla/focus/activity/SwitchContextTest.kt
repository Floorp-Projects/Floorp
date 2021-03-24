/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.hamcrest.core.IsNull
import org.junit.After
import org.junit.Assert
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.expandNotification
import org.mozilla.focus.helpers.TestHelper.openNotification
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.pressHomeKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitForIdle
import org.mozilla.focus.helpers.TestHelper.waitForWebSiteTitleLoad
import org.mozilla.focus.helpers.TestHelper.waitingTime
import java.io.IOException

// This test opens enters and invalid URL, and Focus should provide an appropriate error message
@RunWith(AndroidJUnit4ClassRunner::class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
class SwitchContextTest {
    private var webServer: MockWebServer? = null

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        webServer = MockWebServer()
        try {
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("image_test.html"))
                    .addHeader(
                        "Set-Cookie",
                        "sphere=battery; Expires=Wed, 21 Oct 2035 07:28:00 GMT;"
                    )
            )
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("rabbit.jpg"))
            )
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("download.jpg"))
            )
            webServer!!.start()
        } catch (e: IOException) {
            throw AssertionError("Could not start web server", e)
        }
    }

    @After
    fun tearDown() {
        mActivityTestRule.activity.finishAndRemoveTask()
        try {
            webServer!!.close()
            webServer!!.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
    }

    @Test
    @Throws(UiObjectNotFoundException::class)
    fun ForegroundTest() {

        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists())
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text =
            webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()

        // Assert website is loaded
        waitForWebSiteTitleLoad()

        // Switch out of Focus, pull down system bar and select open action
        pressHomeKey()
        openNotification()
        waitForIdle()
        // If simulator has more recent message, the options need to be expanded
        expandNotification()
        TestHelper.notificationOpenItem.click()

        // Verify that it's on the main view, showing the previous browsing session
        TestHelper.browserURLbar.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.browserURLbar.exists())
        waitForWebSiteTitleLoad()
    }

    @Test
    @Throws(UiObjectNotFoundException::class)
    fun eraseAndOpenTest() {

        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists())
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text =
            webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()

        // Assert website is loaded
        waitForWebSiteTitleLoad()

        // Switch out of Focus, pull down system bar and select open action
        pressHomeKey()
        openNotification()

        // If simulator has more recent message, the options need to be expanded
        expandNotification()
        TestHelper.notificationEraseOpenItem.click()

        // Verify that it's on the main view, showing the initial view
        TestHelper.erasedMsg.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.erasedMsg.exists())
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists())
        Assert.assertTrue(TestHelper.initialView.exists())
    }

    @Suppress("LongMethod")
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun settingsToFocus() {

        // Initialize UiDevice instance
        val LAUNCH_TIMEOUT = 5000
        val SETTINGS_APP = "com.android.settings"
        val settings = TestHelper.mDevice.findObject(
            UiSelector()
                .packageName(SETTINGS_APP)
                .enabled(true)
        )

        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()

        // Assert website is loaded
        waitForWebSiteTitleLoad()

        // Switch out of Focus, open settings app
        pressHomeKey()

        // Wait for launcher
        val launcherPackage = TestHelper.mDevice.launcherPackageName
        Assert.assertThat(launcherPackage, IsNull.notNullValue())
        TestHelper.mDevice.wait(
            Until.hasObject(By.pkg(launcherPackage).depth(0)),
            LAUNCH_TIMEOUT.toLong()
        )

        // Launch the app
        val context = InstrumentationRegistry.getInstrumentation()
            .targetContext
            .applicationContext
        val intent = context.packageManager
            .getLaunchIntentForPackage(SETTINGS_APP)
        context.startActivity(intent)

        // switch to Focus
        settings.waitForExists(waitingTime)
        Assert.assertTrue(settings.exists())
        openNotification()

        // If simulator has more recent message, the options need to be expanded
        expandNotification()
        TestHelper.notificationOpenItem.click()

        // Verify that it's on the main view, showing the previous browsing session
        TestHelper.browserURLbar.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.browserURLbar.exists())
        waitForWebSiteTitleLoad()
    }

    companion object {
        private const val TEST_PATH = "/"
    }
}
