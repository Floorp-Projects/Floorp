/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

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
import org.mozilla.focus.helpers.EspressoHelper.openSettings
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.pressBackKey
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitForWebContent
import org.mozilla.focus.helpers.TestHelper.waitingTime
import java.io.IOException

@RunWith(AndroidJUnit4ClassRunner::class)
@Ignore("Test fails, will be refactored")
class SettingsBlockToggleTest {
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

    @Suppress("LongMethod")
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun SettingsToggleTest() {
        val privacyHeading = TestHelper.mDevice.findObject(
            UiSelector()
                .text("Privacy & Security")
                .resourceId("android:id/title")
        )
        val blockAdTrackerEntry = TestHelper.settingsMenu.getChild(
            UiSelector()
                .className("android.widget.LinearLayout")
                .instance(1)
        )
        val blockAdTrackerSwitch = blockAdTrackerEntry.getChild(
            UiSelector()
                .resourceId(appName + ":id/switchWidget")
        )
        val blockAnalyticTrackerEntry = TestHelper.settingsMenu.getChild(
            UiSelector()
                .className("android.widget.LinearLayout")
                .instance(2)
        )
        val blockAnalyticTrackerSwitch = blockAnalyticTrackerEntry.getChild(
            UiSelector()
                .resourceId(appName + ":id/switchWidget")
        )
        val blockSocialTrackerEntry = TestHelper.settingsMenu.getChild(
            UiSelector()
                .className("android.widget.LinearLayout")
                .instance(4)
        )
        val blockSocialTrackerSwitch = blockSocialTrackerEntry.getChild(
            UiSelector()
                .resourceId(appName + ":id/switchWidget")
        )

        // Let's go to an actual URL which is http://
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = webServer!!.url(TEST_PATH).toString()
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()
        waitForWebContent()
        Assert.assertTrue(
            TestHelper.browserURLbar.text.contains(
                webServer!!.url(TEST_PATH).toString()
            )
        )
        Assert.assertTrue(!TestHelper.lockIcon.exists())

        /* Go to settings and disable everything */openSettings()
        privacyHeading.waitForExists(waitingTime)
        privacyHeading.click()
        val blockAdTrackerValue = blockAdTrackerSwitch.text
        val blockAnalyticTrackerValue = blockAnalyticTrackerSwitch.text
        val blockSocialTrackerValue = blockSocialTrackerSwitch.text

        // Turn off and back on
        blockAdTrackerEntry.click()
        blockAnalyticTrackerEntry.click()
        blockSocialTrackerEntry.click()
        org.junit.Assert.assertFalse(blockAdTrackerSwitch.text == blockAdTrackerValue)
        org.junit.Assert.assertFalse(blockAnalyticTrackerSwitch.text == blockAnalyticTrackerValue)
        org.junit.Assert.assertFalse(blockSocialTrackerSwitch.text == blockSocialTrackerValue)
        blockAdTrackerEntry.click()
        blockAnalyticTrackerEntry.click()
        blockSocialTrackerEntry.click()
        Assert.assertTrue(blockAdTrackerSwitch.text == blockAdTrackerValue)
        Assert.assertTrue(blockAnalyticTrackerSwitch.text == blockAnalyticTrackerValue)
        Assert.assertTrue(blockSocialTrackerSwitch.text == blockSocialTrackerValue)

        // Back to the webpage
        pressBackKey()
        pressBackKey()
        waitForWebContent()
        Assert.assertTrue(
            TestHelper.browserURLbar.text.contains(
                webServer!!.url(TEST_PATH).toString()
            )
        )
        Assert.assertTrue(!TestHelper.lockIcon.exists())
    }

    companion object {
        private const val TEST_PATH = "/"
    }
}
