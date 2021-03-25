/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.content.Intent
import androidx.test.espresso.action.ViewActions
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiObjectNotFoundException
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
import org.mozilla.focus.helpers.TestHelper.openNotification
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.pressHomeKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitForWebContent
import org.mozilla.focus.helpers.TestHelper.waitingTime
import java.io.IOException

// This test erases URL and checks for message
// https://testrail.stage.mozaws.net/index.php?/cases/view/40068
@Ignore("Flaky test, will be refactored")
@RunWith(AndroidJUnit4ClassRunner::class)
class EraseAllUserDataTest {
    private var webServer: MockWebServer? = null

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        webServer = MockWebServer()
        try {
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("plain_test.html"))
            )
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("plain_test.html"))
            )
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("plain_test.html"))
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
    fun TrashTest() {

        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text =
            webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        waitForWebContent()

        // Press erase button, and check for message and return to the main page
        TestHelper.floatingEraseButton.perform(ViewActions.click())
        TestHelper.erasedMsg.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.erasedMsg.exists())
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists())
    }

    @Test
    @Throws(UiObjectNotFoundException::class)
    fun systemBarTest() {
        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        waitForWebContent()
        TestHelper.menuButton.perform(ViewActions.click())
        TestHelper.blockCounterItem.waitForExists(waitingTime)

        // Pull down system bar and select delete browsing history
        openNotification()
        TestHelper.notificationBarDeleteItem.waitForExists(waitingTime)
        TestHelper.notificationBarDeleteItem.click()
        TestHelper.erasedMsg.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.erasedMsg.exists())
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists())
        Assert.assertFalse(TestHelper.menulist.exists())
    }

    @Test
    @Throws(UiObjectNotFoundException::class)
    fun systemBarHomeViewTest() {

        // Initialize UiDevice instance
        val LAUNCH_TIMEOUT = 5000

        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text =
            webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        waitForWebContent()
        // Switch out of Focus, pull down system bar and select delete browsing history
        pressHomeKey()
        openNotification()
        TestHelper.notificationBarDeleteItem.waitForExists(waitingTime)
        TestHelper.notificationBarDeleteItem.click()

        // Wait for launcher
        val launcherPackage = TestHelper.mDevice.launcherPackageName
        org.junit.Assert.assertThat(launcherPackage, IsNull.notNullValue())
        TestHelper.mDevice.wait(
            Until.hasObject(By.pkg(launcherPackage).depth(0)),
            LAUNCH_TIMEOUT.toLong()
        )

        // Launch the app
        mActivityTestRule.launchActivity(Intent(Intent.ACTION_MAIN))
        // Verify that it's on the main view, not showing the previous browsing session
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists())
    }

    companion object {
        private const val TEST_PATH = "/"
    }
}
