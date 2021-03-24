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
import org.junit.Rule
import org.junit.runner.RunWith
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.pressBackKey
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitForWebContent
import org.mozilla.focus.helpers.TestHelper.waitingTime
import java.io.IOException

// This test opens a webpage, and selects "Open in" menu
@RunWith(AndroidJUnit4ClassRunner::class)
class OpenInExternalBrowserDialogueTest {
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
    }

    companion object {
        private const val TEST_PATH = "/"
    }

    init {
        val openWithBtn = TestHelper.mDevice.findObject(
            UiSelector()
                .resourceId(appName + ":id/open_select_browser")
                .enabled(true)
        )
        val openWithTitle = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.TextView")
                .text("Open in…")
                .enabled(true)
        )
        val openWithList = TestHelper.mDevice.findObject(
            UiSelector()
                .resourceId(appName + ":id/apps")
                .enabled(true)
        )

        /* Go to mozilla page */TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        waitForWebContent()

        /* Select Open with from menu, check appearance */TestHelper.menuButton.perform(ViewActions.click())
        openWithBtn.waitForExists(waitingTime)
        openWithBtn.click()
        openWithTitle.waitForExists(waitingTime)
        Assert.assertTrue(openWithTitle.exists())
        Assert.assertTrue(openWithList.exists())
        pressBackKey()
    }
}
