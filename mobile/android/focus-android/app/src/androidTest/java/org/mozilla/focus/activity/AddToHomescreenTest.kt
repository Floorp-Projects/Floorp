/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.os.Build
import android.os.Build.VERSION
import androidx.test.espresso.action.ViewActions
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiSelector
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.pressBackKey
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.pressHomeKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.swipeScreenLeft
import org.mozilla.focus.helpers.TestHelper.waitForIdle
import org.mozilla.focus.helpers.TestHelper.waitForWebContent
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime
import java.io.IOException

// https://testrail.stage.mozaws.net/index.php?/cases/view/60852
// includes:
// https://testrail.stage.mozaws.net/index.php?/cases/view/40066
@RunWith(AndroidJUnit4ClassRunner::class)
class AddToHomescreenTest {
    private var webServer: MockWebServer? = null
    private var webServerPort = 0
    private var webServerBookmarkName: String? = null

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setup() {
        webServer = MockWebServer()
        // note: requesting getPort() will automatically start the mock server,
        //       so if you use the 2 lines, do not try to start server or it will choke.
        webServerPort = webServer!!.port
        webServerBookmarkName = "localhost_" + Integer.toString(webServerPort)
        try {
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("plain_test.html"))
            )
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("plain_test.html"))
            )
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

    private val welcomeBtn = TestHelper.mDevice.findObject(
        UiSelector()
            .resourceId("com.android.launcher3:id/cling_dismiss_longpress_info")
            .text("GOT IT")
            .enabled(true)
    )

    @Throws(UiObjectNotFoundException::class)
    private fun handleShortcutLayoutDialog() {
        TestHelper.AddautoBtn.waitForExists(waitingTime)
        TestHelper.AddautoBtn.click()
        TestHelper.AddautoBtn.waitUntilGone(waitingTime)
        pressHomeKey()
    }

    @Throws(UiObjectNotFoundException::class)
    private fun openAddtoHSDialog() {
        TestHelper.menuButton.perform(ViewActions.click())
        TestHelper.AddtoHSmenuItem.waitForExists(waitingTime)
        // If the menu item is not clickable, wait and retry
        while (!TestHelper.AddtoHSmenuItem.isClickable) {
            pressBackKey()
            TestHelper.menuButton.perform(ViewActions.click())
        }
        TestHelper.AddtoHSmenuItem.click()
    }

    @Ignore("Flaky test, will be refactored")
    @Test
    @Throws(UiObjectNotFoundException::class)
    @Suppress("LongMethod")
    fun AddToHomeScreenTest() {
        val shortcutIcon = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.TextView")
                .description(webServerBookmarkName)
                .enabled(true)
        )

        // Open website, and click 'Add to homescreen'
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        TestHelper.progressBar.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime))
        openAddtoHSDialog()
        // Add to Home screen dialog is now shown
        TestHelper.shortcutTitle.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.shortcutTitle.isEnabled)
        Assert.assertEquals(TestHelper.shortcutTitle.text, "gigantic experience")
        Assert.assertTrue(TestHelper.AddtoHSOKBtn.isEnabled)
        Assert.assertTrue(TestHelper.AddtoHSCancelBtn.isEnabled)

        // Edit shortcut text
        TestHelper.shortcutTitle.click()
        TestHelper.shortcutTitle.text = webServerBookmarkName
        TestHelper.AddtoHSOKBtn.click()

        // For Android O, we need additional steps
        if (VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            handleShortcutLayoutDialog()
        }

        // App is sent to background, in launcher now
        if (welcomeBtn.exists()) {
            welcomeBtn.click()
        }
        shortcutIcon.waitForExists(waitingTime)
        Assert.assertTrue(shortcutIcon.isEnabled)
        shortcutIcon.click()
        TestHelper.browserURLbar.waitForExists(waitingTime)
        Assert.assertTrue(
            TestHelper.browserURLbar.text
                    == webServer!!.url(TEST_PATH).toString()
        )
    }

    @Ignore("Flaky test, will be refactored")
    @Test
    @Throws(UiObjectNotFoundException::class)
    @Suppress("LongMethod")
    fun NonameTest() {
        val shortcutIcon = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.TextView")
                .description(webServerBookmarkName)
                .enabled(true)
        )

        // Open website, and click 'Add to homescreen'
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text =
            webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        TestHelper.progressBar.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime))
        openAddtoHSDialog()

        // "Add to Home screen" dialog is now shown
        TestHelper.shortcutTitle.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.shortcutTitle.isEnabled)
        Assert.assertEquals(TestHelper.shortcutTitle.text, "gigantic experience")
        Assert.assertTrue(TestHelper.AddtoHSOKBtn.isEnabled)
        Assert.assertTrue(TestHelper.AddtoHSCancelBtn.isEnabled)

        // replace shortcut text
        TestHelper.shortcutTitle.click()
        TestHelper.shortcutTitle.text = webServerBookmarkName
        TestHelper.AddtoHSOKBtn.click()

        // For Android O, we need additional steps
        if (VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            handleShortcutLayoutDialog()
        }
        if (welcomeBtn.exists()) {
            welcomeBtn.click()
        }
        // App is sent to background, in launcher now
        // Start from home and then swipe, to ensure we land where we want to search for shortcut
        TestHelper.mDevice.pressHome()
        swipeScreenLeft()
        shortcutIcon.waitForExists(waitingTime)
        Assert.assertTrue(shortcutIcon.isEnabled)
        shortcutIcon.click()
        TestHelper.browserURLbar.waitForExists(waitingTime)
        Assert.assertTrue(
            TestHelper.browserURLbar.text
                    == webServer!!.url(TEST_PATH).toString()
        )
    }

    @Ignore("Flaky test, will be refactored")
    @Test
    @Throws(UiObjectNotFoundException::class)
    @Suppress("LongMethod")
    fun SearchTermShortcutTest() {
        val shortcutIcon = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.TextView")
                .descriptionContains("helloworld")
                .enabled(true)
        )

        // Open website, and click 'Add to homescreen'
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = "helloworld"
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        // In certain cases, where progressBar disappears immediately, below will return false
        // since it busy waits, it will unblock when the bar isn't visible, regardless of the
        // return value
        TestHelper.progressBar.waitForExists(waitingTime)
        TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime)
        openAddtoHSDialog()

        // "Add to Home screen" dialog is now shown
        TestHelper.shortcutTitle.waitForExists(waitingTime)
        TestHelper.AddtoHSOKBtn.click()

        // For Android O, we need additional steps
        if (VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            handleShortcutLayoutDialog()
        }
        if (welcomeBtn.exists()) {
            welcomeBtn.click()
        }

        // App is sent to background, in launcher now
        // Start from home and then swipe, to ensure we land where we want to search for shortcut
        TestHelper.mDevice.pressHome()
        swipeScreenLeft()
        shortcutIcon.waitForExists(waitingTime)
        Assert.assertTrue(shortcutIcon.isEnabled)
        shortcutIcon.click()
        waitForIdle()
        waitForWebContent()

        // Tap URL bar and check the search term is still shown
        TestHelper.browserURLbar.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.browserURLbar.text.contains("helloworld"))
    }

    companion object {
        private const val TEST_PATH = "/"
    }
}
