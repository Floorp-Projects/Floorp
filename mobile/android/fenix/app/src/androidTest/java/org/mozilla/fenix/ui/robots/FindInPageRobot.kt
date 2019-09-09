/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.clearText
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the find in page UI.
 */
class FindInPageRobot {

    val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())!!

    fun verifyFindInPageNextButton() = assertFindInPageNextButton()!!
    fun verifyFindInPagePrevButton() = assertFindInPagePrevButton()!!
    fun verifyFindInPageCloseButton() = assertFindInPageCloseButton()!!

    fun enterFindInPageQuery(expectedText: String) {
        mDevice.wait(Until.findObject(By.res("find_in_page_query_text")), waitingTimeShort)
        findInPageQuery().perform(clearText(), typeText(expectedText))
    }

    fun verifyFindNextInPageResult(ratioCounter: String) {
        mDevice.waitForIdle()
        findInPageResult().check(matches(withText((ratioCounter))))
        findInPageNextButton().click()
    }

    fun verifyFindPrevInPageResult(ratioCounter: String) {
        mDevice.waitForIdle()
        findInPageResult().check(matches(withText((ratioCounter))))
        findInPagePrevButton().click()
    }

    class Transition {
        fun closeFindInPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitForIdle()
            findInPageCloseButton().click()
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

private fun assertFindInPageNextButton() = findInPageNextButton()
    .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
private fun assertFindInPagePrevButton() = findInPagePrevButton()
    .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
private fun assertFindInPageCloseButton() = findInPageCloseButton()
    .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
