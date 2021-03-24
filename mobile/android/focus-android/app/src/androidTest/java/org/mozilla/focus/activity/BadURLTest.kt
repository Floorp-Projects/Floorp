/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.web.sugar.Web
import androidx.test.filters.RequiresDevice
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiSelector
import org.junit.After
import org.junit.Assert
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.waitingTime

// This test opens enters and invalid URL, and Focus should provide an appropriate error message
@RunWith(AndroidJUnit4ClassRunner::class)
@RequiresDevice
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
class BadURLTest {
    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @After
    fun tearDown() {
        mActivityTestRule.activity.finishAndRemoveTask()
    }

    @Suppress("LongMethod")
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun BadURLcheckTest() {
        val cancelOpenAppBtn = TestHelper.mDevice.findObject(
            UiSelector()
                .resourceId("android:id/button2")
        )
        val openAppalert = TestHelper.mDevice.findObject(
            UiSelector()
                .text("Open link in another app")
        )

        // provide an invalid URL
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = "htps://www.mozilla.org"
        TestHelper.hint.waitForExists(waitingTime)
        pressEnterKey()

        // Check for error message
        Web.onWebView(ViewMatchers.withText(R.string.error_malformedURI_title))
        Web.onWebView(ViewMatchers.withText(R.string.error_malformedURI_message))
        Web.onWebView(ViewMatchers.withText("Try Again"))
        TestHelper.floatingEraseButton.perform(ViewActions.click())

        /* provide market URL that is handled by Google Play app */TestHelper.inlineAutocompleteEditText.waitForExists(
            waitingTime
        )
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text =
            "market://details?id=org.mozilla.firefox&referrer=" +
                    "utm_source%3Dmozilla%26utm_medium%3DReferral%26utm_campaign%3Dmozilla-org"
        pressEnterKey()

        // Wait for the dialog
        cancelOpenAppBtn.waitForExists(waitingTime)
        Assert.assertTrue(openAppalert.exists())
        Assert.assertTrue(cancelOpenAppBtn.exists())
        cancelOpenAppBtn.click()
        TestHelper.floatingEraseButton.perform(ViewActions.click())
        TestHelper.erasedMsg.waitForExists(waitingTime)
    }
}
