/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.os.Build
import android.os.Build.VERSION
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiSelector
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert
import org.junit.Assume
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.pressBackKey
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitForWebContent
import org.mozilla.focus.helpers.TestHelper.waitingTime
import java.io.IOException

@Ignore("Flaky test, will be refactored")
@RunWith(AndroidJUnit4ClassRunner::class)
// @Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView") // https://testrail.stage.mozaws.net/index.php?/cases/view/53141
class DownloadFileTest {
    private var webServer: MockWebServer? = null

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        Assume.assumeTrue(VERSION.SDK_INT >= Build.VERSION_CODES.N)
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
                    .setBody(readTestAsset("download.jpg"))
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
        // If notification is still up, this will take it off screen
        pressBackKey()
        mActivityTestRule.activity.finishAndRemoveTask()
        try {
            webServer!!.close()
            webServer!!.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
    }

    @Suppress("LongMethod")
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun DownloadTest() {
        val downloadIcon: UiObject
        downloadIcon = TestHelper.mDevice.findObject(
            UiSelector()
                .resourceId("download")
                .enabled(true)
        )

        // Load website with service worker
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        waitForWebContent()
        TestHelper.progressBar.waitUntilGone(waitingTime)
        downloadIcon.click()

        // If permission dialog appears, grant it
        if (TestHelper.permAllowBtn.waitForExists(waitingTime)) {
            TestHelper.permAllowBtn.click()
        }
        TestHelper.downloadTitle.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.downloadTitle.isEnabled)
        Assert.assertTrue(TestHelper.downloadCancelBtn.isEnabled)
        Assert.assertTrue(TestHelper.downloadBtn.isEnabled)
        Assert.assertEquals(TestHelper.downloadFileName.text, "download.jpg")
        Assert.assertEquals(
            TestHelper.downloadWarning.text,
            "Downloaded files will not be deleted when you erase Firefox Focus history."
        )
        TestHelper.downloadBtn.click()
        TestHelper.completedMsg.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.completedMsg.isEnabled)
        Assert.assertTrue(TestHelper.completedMsg.text.contains("finished"))
        TestHelper.mDevice.openNotification()
        TestHelper.mDevice.waitForIdle()
        TestHelper.savedNotification.waitForExists(waitingTime)
        TestHelper.savedNotification.swipeRight(50)
    }

    companion object {
        private const val TEST_PATH = "/"
    }
}
