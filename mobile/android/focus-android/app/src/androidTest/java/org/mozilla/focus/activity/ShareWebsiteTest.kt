/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.espresso.action.ViewActions
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.uiautomator.UiSelector
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.runner.RunWith
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.pressBackKey
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitingTime
import java.io.IOException

// This test opens share menu
// https://testrail.stage.mozaws.net/index.php?/cases/view/47592
@RunWith(AndroidJUnit4ClassRunner::class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
class ShareWebsiteTest {
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

    companion object {
        private const val TEST_PATH = "/"
    }

    init {
        val shareBtn = TestHelper.mDevice.findObject(
            UiSelector()
                .resourceId(appName + ":id/share")
                .enabled(true)
        )

        /* Go to a webpage */TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        Assert.assertTrue(TestHelper.webView.waitForExists(waitingTime))

        /* Select share */TestHelper.menuButton.perform(ViewActions.click())
        shareBtn.waitForExists(waitingTime)
        shareBtn.click()

        // For simulators, where apps are not installed, it'll take to message app
        TestHelper.shareMenuHeader.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.shareMenuHeader.exists())
        Assert.assertTrue(TestHelper.shareAppList.exists())
        pressBackKey()
    }
}
