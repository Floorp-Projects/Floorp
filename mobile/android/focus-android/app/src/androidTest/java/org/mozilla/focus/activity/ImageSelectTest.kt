/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.os.Build
import android.os.Build.VERSION
import android.view.KeyEvent
import androidx.test.espresso.action.ViewActions
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.uiautomator.UiObject
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
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.pressBackKey
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitForWebSiteTitleLoad
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

@RunWith(AndroidJUnit4ClassRunner::class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
class ImageSelectTest {
    private var webServer: MockWebServer? = null
    private var rabbitImage: UiObject? = null
    private val imageMenuTitle = TestHelper.mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/topPanel")
            .enabled(true)
    )
    private val imageMenuTitleText = TestHelper.mDevice.findObject(
        UiSelector()
            .className("android.widget.TextView")
            .enabled(true)
            .instance(0)
    )
    private val shareMenu = TestHelper.mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/design_menu_item_text")
            .text("Share image")
            .enabled(true)
    )
    private val copyMenu = TestHelper.mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/design_menu_item_text")
            .text("Copy image address")
            .enabled(true)
    )
    private val saveMenu = TestHelper.mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/design_menu_item_text")
            .text("Save image")
            .enabled(true)
    )
    private val warning = TestHelper.mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/warning")
            .text("Saved and shared images will not be deleted when you erase Firefox Focus history.")
            .enabled(true)
    )

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        rabbitImage = if (VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            TestHelper.mDevice.findObject(
                UiSelector()
                    .resourceId("rabbitImage")
                    .enabled(true)
            )
        } else {
            TestHelper.mDevice.findObject(
                UiSelector()
                    .description("Smiley face")
                    .enabled(true)
            )
        }
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
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("rabbit.jpg"))
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
    fun ImageMenuTest() {
        val imagePath = webServer!!.url(TEST_PATH).toString() + "rabbit.jpg"

        // Load website with service worker
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        Assert.assertTrue(TestHelper.webView.waitForExists(waitingTime))

        // Assert website is loaded
        waitForWebSiteTitleLoad()

        // Find image and long tap it
        rabbitImage!!.waitForExists(waitingTime)
        Assert.assertTrue(rabbitImage!!.exists())
        rabbitImage!!.dragTo(rabbitImage, 10)
        imageMenuTitle.waitForExists(waitingTime)
        Assert.assertTrue(imageMenuTitle.exists())
        Assert.assertEquals(imageMenuTitleText.text, imagePath)
        Assert.assertTrue(shareMenu.exists())
        Assert.assertTrue(copyMenu.exists())
        Assert.assertTrue(saveMenu.exists())
        Assert.assertTrue(warning.exists())
        if (VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            copyMenu.click()

            // Erase browsing session
            TestHelper.floatingEraseButton.perform(ViewActions.click())
            TestHelper.erasedMsg.waitForExists(waitingTime)
            Assert.assertTrue(TestHelper.erasedMsg.exists())

            // KeyEvent.KEYCODE_PASTE) requires API 24 or above
            TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
            TestHelper.mDevice.pressKeyCode(KeyEvent.KEYCODE_PASTE)
            Assert.assertEquals(TestHelper.inlineAutocompleteEditText.text, imagePath)
            mDevice.pressBack()
        }
    }

    @Suppress("LongMethod")
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun ShareImageTest() {

        // Load website with service worker
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        Assert.assertTrue(TestHelper.webView.waitForExists(waitingTime))

        // Assert website is loaded
        waitForWebSiteTitleLoad()
        TestHelper.progressBar.waitUntilGone(waitingTime)

        // Find image and long tap it
        rabbitImage!!.waitForExists(waitingTime)
        Assert.assertTrue(rabbitImage!!.exists())
        rabbitImage!!.dragTo(rabbitImage, 10)
        imageMenuTitle.waitForExists(waitingTime)
        Assert.assertTrue(shareMenu.exists())
        Assert.assertTrue(copyMenu.exists())
        Assert.assertTrue(saveMenu.exists())
        shareMenu.click()

        // For simulators, where apps are not installed, it'll take to message app
        TestHelper.shareMenuHeader.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.shareMenuHeader.exists())
        Assert.assertTrue(TestHelper.shareAppList.exists())
        pressBackKey()
        TestHelper.floatingEraseButton.perform(ViewActions.click())
        TestHelper.erasedMsg.waitForExists(waitingTime)
    }

    @Suppress("LongMethod")
    @SmokeTest
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun DownloadImageMenuTest() {

        // Load website with service worker
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        Assert.assertTrue(TestHelper.webView.waitForExists(waitingTime))

        // Find image and long tap it
        rabbitImage!!.waitForExists(waitingTime)
        Assert.assertTrue(rabbitImage!!.exists())
        rabbitImage!!.dragTo(rabbitImage, 10)
        imageMenuTitle.waitForExists(waitingTime)
        Assert.assertTrue(imageMenuTitle.exists())
        Assert.assertTrue(shareMenu.exists())
        Assert.assertTrue(copyMenu.exists())
        Assert.assertTrue(saveMenu.exists())
        saveMenu.click()

        // If permission dialog appears, grant it
        if (TestHelper.permAllowBtn.waitForExists(waitingTime)) {
            TestHelper.permAllowBtn.click()
            TestHelper.downloadTitle.waitForExists(waitingTime)
            TestHelper.downloadBtn.click()
        }
        TestHelper.mDevice.openNotification()
        TestHelper.savedNotification.waitForExists(waitingTime)
        TestHelper.savedNotification.swipeRight(50)
        pressBackKey()
        TestHelper.floatingEraseButton.perform(ViewActions.click())
        TestHelper.erasedMsg.waitForExists(waitingTime)
    }

    companion object {
        private const val TEST_PATH = "/"
    }
}
