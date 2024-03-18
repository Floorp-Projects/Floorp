/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.clearText
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Until
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the find in page UI.
 */
class FindInPageRobot {
    fun verifyFindInPageNextButton() {
        Log.i(TAG, "verifyFindInPageNextButton: Trying to verify find in page next result button is visible")
        findInPageNextButton()
            .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyFindInPageNextButton: Verified find in page next result button is visible")
    }
    fun verifyFindInPagePrevButton() {
        Log.i(TAG, "verifyFindInPagePrevButton: Trying to verify find in page previous result button is visible")
        findInPagePrevButton()
            .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyFindInPagePrevButton: Verified find in page previous result button is visible")
    }
    fun verifyFindInPageCloseButton() {
        Log.i(TAG, "verifyFindInPageCloseButton: Trying to verify find in page close button is visible")
        findInPageCloseButton()
            .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyFindInPageCloseButton: Verified find in page close button is visible")
    }
    fun clickFindInPageNextButton() {
        Log.i(TAG, "clickFindInPageNextButton: Trying to click next result button")
        findInPageNextButton().click()
        Log.i(TAG, "clickFindInPageNextButton: Clicked next result button")
    }
    fun clickFindInPagePrevButton() {
        Log.i(TAG, "clickFindInPagePrevButton: Trying to click previous result button")
        findInPagePrevButton().click()
        Log.i(TAG, "clickFindInPagePrevButton: Clicked previous result button")
    }

    fun enterFindInPageQuery(expectedText: String) {
        mDevice.waitNotNull(Until.findObject(By.res("org.mozilla.fenix.debug:id/find_in_page_query_text")), waitingTime)
        Log.i(TAG, "enterFindInPageQuery: Trying to clear find in page bar text")
        findInPageQuery().perform(clearText())
        Log.i(TAG, "enterFindInPageQuery: Cleared find in page bar text")
        mDevice.waitNotNull(Until.gone(By.res("org.mozilla.fenix.debug:id/find_in_page_result_text")), waitingTime)
        Log.i(TAG, "enterFindInPageQuery: Trying to type $expectedText in find in page bar")
        findInPageQuery().perform(typeText(expectedText))
        Log.i(TAG, "enterFindInPageQuery: Typed $expectedText in find page bar")
        mDevice.waitNotNull(Until.findObject(By.res("org.mozilla.fenix.debug:id/find_in_page_result_text")), waitingTime)
    }

    fun verifyFindInPageResult(ratioCounter: String) {
        mDevice.waitNotNull(Until.findObject(By.text(ratioCounter)), waitingTime)
        Log.i(TAG, "verifyFindInPageResult: Trying to verify $ratioCounter results")
        findInPageResult().check(matches(withText((ratioCounter))))
        Log.i(TAG, "verifyFindInPageResult: Verified $ratioCounter results")
    }

    class Transition {
        fun closeFindInPageWithCloseButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "closeFindInPageWithCloseButton: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "closeFindInPageWithCloseButton: Device was idle")
            Log.i(TAG, "closeFindInPageWithCloseButton: Trying to close find in page button")
            findInPageCloseButton().click()
            Log.i(TAG, "closeFindInPageWithCloseButton: Clicked close find in page button")
            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeFindInPageWithBackButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "closeFindInPageWithBackButton: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "closeFindInPageWithBackButton: Device was idle")

            // Will need to press back 2x, the first will only dismiss the keyboard
            Log.i(TAG, "closeFindInPageWithBackButton: Trying to press 1x the device back button")
            mDevice.pressBack()
            Log.i(TAG, "closeFindInPageWithBackButton: Pressed 1x the device back button")
            Log.i(TAG, "closeFindInPageWithBackButton: Trying to press 2x the device back button")
            mDevice.pressBack()
            Log.i(TAG, "closeFindInPageWithBackButton: Pressed 2x the device back button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private fun findInPageQuery() = onView(withId(R.id.find_in_page_query_text))
private fun findInPageResult() = onView(withId(R.id.find_in_page_result_text))
private fun findInPageNextButton() = onView(withId(R.id.find_in_page_next_btn))
private fun findInPagePrevButton() = onView(withId(R.id.find_in_page_prev_btn))
private fun findInPageCloseButton() = onView(withId(R.id.find_in_page_close_btn))
