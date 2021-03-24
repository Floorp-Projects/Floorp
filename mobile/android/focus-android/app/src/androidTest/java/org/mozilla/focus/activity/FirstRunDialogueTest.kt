/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import org.junit.After
import org.junit.Assert
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.waitingTime

// https://testrail.stage.mozaws.net/index.php?/cases/view/40062
@RunWith(AndroidJUnit4ClassRunner::class)
class FirstRunDialogueTest {

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = true)

    @After
    fun tearDown() {
        mActivityTestRule.activity.finishAndRemoveTask()
    }

    @Test
    fun FirstRunDialogueTest() {
        // Let's search for something
        TestHelper.firstSlide.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.firstSlide.exists())
        TestHelper.nextBtn.click()
        TestHelper.secondSlide.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.secondSlide.exists())
        TestHelper.nextBtn.click()
        TestHelper.thirdSlide.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.thirdSlide.exists())
        TestHelper.nextBtn.click()
        TestHelper.lastSlide.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.lastSlide.exists())
        TestHelper.finishBtn.click()
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists())
    }
}
