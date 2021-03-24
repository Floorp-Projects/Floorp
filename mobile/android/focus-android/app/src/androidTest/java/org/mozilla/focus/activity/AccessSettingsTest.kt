/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.uiautomator.UiSelector
import org.junit.Assert
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.helpers.EspressoHelper.openSettings
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.waitingTime

// This test checks all the headings in the Settings menu are there
// https://testrail.stage.mozaws.net/index.php?/cases/view/40064
@RunWith(AndroidJUnit4ClassRunner::class)
class AccessSettingsTest {
    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    val generalHeading = TestHelper.mDevice.findObject(
        UiSelector()
            .text("General")
            .resourceId("android:id/title")
    )
    val privacyHeading = TestHelper.mDevice.findObject(
        UiSelector()
            .text("Privacy & Security")
            .resourceId("android:id/title")
    )
    val searchHeading = TestHelper.mDevice.findObject(
        UiSelector()
            .text("Search")
            .resourceId("android:id/title")
    )
    val mozHeading = TestHelper.mDevice.findObject(
        UiSelector()
            .text("Mozilla")
            .resourceId("android:id/title")
    )

    @Test
    fun AccessSettingsTest() {
        /* Go to Settings */TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        openSettings()
        generalHeading.waitForExists(waitingTime)

        /* Check the first element and other headings are present */
        Assert.assertTrue(generalHeading.exists())
        Assert.assertTrue(searchHeading.exists())
        Assert.assertTrue(privacyHeading.exists())
        Assert.assertTrue(mozHeading.exists())
    }
}
