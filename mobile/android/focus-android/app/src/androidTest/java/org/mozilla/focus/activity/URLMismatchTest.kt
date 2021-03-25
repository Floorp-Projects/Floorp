/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.view.KeyEvent
import androidx.test.espresso.Espresso
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiSelector
import org.hamcrest.Matchers
import org.junit.After
import org.junit.Assert
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.helpers.EspressoHelper.childAtPosition
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime

// This test checks whether URL and displayed site are in sync
@RunWith(AndroidJUnit4ClassRunner::class)
class URLMismatchTest {
    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @After
    fun tearDown() {
        mActivityTestRule.activity.finishAndRemoveTask()
    }

    @Ignore("Flaky test, will be refactored")
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun MismatchTest() {
        val searchString = String.format("mozilla focus - %s Search", "google")
        val googleWebView = TestHelper.mDevice.findObject(
            UiSelector()
                .description(searchString)
                .className("android.webkit.WebView")
        )

        // Do search on text string
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = "mozilla "
        // Would you like to turn on search suggestions? Yes No
        // fresh install only)
        if (TestHelper.searchSuggestionsTitle.exists()) {
            TestHelper.searchSuggestionsButtonYes.waitForExists(waitingTime)
            TestHelper.searchSuggestionsButtonYes.click()
        }

        // Verify that at least 1 search hint is displayed and click it.
        TestHelper.mDevice.pressKeyCode(KeyEvent.KEYCODE_SPACE)
        TestHelper.suggestionList.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.suggestionList.childCount >= 1)
        Espresso.onView(
            Matchers.allOf(
                ViewMatchers.withText(Matchers.containsString("mozilla")),
                ViewMatchers.withId(R.id.suggestion),
                ViewMatchers.isDescendantOfA(
                    childAtPosition(
                        ViewMatchers.withId(R.id.suggestionList),
                        1
                    )
                )
            )
        )
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))

        // WebView is displayed
        pressEnterKey()
        googleWebView.waitForExists(waitingTime)
        TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime)

        // The displayed URL contains mozilla. Click on it to edit it again.
        Espresso.onView(ViewMatchers.withId(R.id.display_url))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .check(ViewAssertions.matches(ViewMatchers.withText(Matchers.containsString("mozilla"))))
            .perform(ViewActions.click())

        // Type "moz" - Verify that it auto-completes to "mozilla.org" and then load the website
        Espresso.onView(ViewMatchers.withId(R.id.urlView))
            .perform(ViewActions.click(), ViewActions.replaceText("mozilla"))
            .check(ViewAssertions.matches(ViewMatchers.withText("mozilla.org")))
            .perform(ViewActions.pressImeActionButton())

        // Wait until site is loaded
        Espresso.onView(ViewMatchers.withId(R.id.webview))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
        TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime)

        // The displayed URL contains www.mozilla.org
        Espresso.onView(ViewMatchers.withId(R.id.display_url))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .check(ViewAssertions.matches(ViewMatchers.withText(Matchers.containsString("www.mozilla.org"))))
    }
}
