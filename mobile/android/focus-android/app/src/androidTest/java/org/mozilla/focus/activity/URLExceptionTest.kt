/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.espresso.Espresso
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.hamcrest.Matchers
import org.hamcrest.core.AllOf
import org.junit.After
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.helpers.EspressoHelper.openSettings
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitForWebContent
import org.mozilla.focus.testAnnotations.SmokeTest

// https://testrail.stage.mozaws.net/index.php?/cases/view/104577
@Ignore("Failing because of: https://github.com/mozilla-mobile/focus-android/issues/4729")
@RunWith(AndroidJUnit4ClassRunner::class)
class URLExceptionTest {
    private val site = "680news.com"
    private val exceptionsTitle = TestHelper.mDevice.findObject(
        UiSelector()
            .text("Exceptions")
            .resourceId("android:id/title")
    )

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @After
    fun tearDown() {
        mActivityTestRule.activity.finishAndRemoveTask()
    }

    @Throws(UiObjectNotFoundException::class)
    private fun OpenExceptionsFragment() {
        mDevice.waitForIdle()
        openSettings()
        Espresso.onView(ViewMatchers.withText("Privacy & Security"))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())

        /* change locale to non-english in the setting, verify the locale is changed */
        val appViews = UiScrollable(UiSelector().scrollable(true))
        appViews.scrollIntoView(exceptionsTitle)
        if (exceptionsTitle.isEnabled) {
            exceptionsTitle.click()
        }
        mDevice.waitForIdle()
    }

    @SmokeTest
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun AddAndRemoveExceptionTest() {
        addException(site)
        OpenExceptionsFragment()
        Espresso.onView(
            AllOf.allOf(
                ViewMatchers.withText(site),
                ViewMatchers.withId(R.id.domainView)
            )
        )
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
        mDevice.waitForIdle()
        removeException()
        mDevice.waitForIdle()

        // Should be in main menu
        Espresso.onView(ViewMatchers.withText("Privacy & Security"))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))

        /* change locale to non-english in the setting, verify the locale is changed */
        val appViews = UiScrollable(UiSelector().scrollable(true))
        appViews.scrollIntoView(exceptionsTitle)
        Espresso.onView(ViewMatchers.withText("Exceptions"))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .check(ViewAssertions.matches(Matchers.not(ViewMatchers.isEnabled())))
    }

    private fun addException(url: String) {
        // Load site
        TestHelper.inlineAutocompleteEditText.waitForExists(TestHelper.waitingTime)
        Espresso.onView(ViewMatchers.withId(R.id.mozac_browser_toolbar_edit_url_view))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .check(ViewAssertions.matches(ViewMatchers.hasFocus()))
            .perform(ViewActions.typeText(url), ViewActions.pressImeActionButton())
        waitForWebContent()
        TestHelper.progressBar.waitUntilGone(TestHelper.waitingTime)

        // The blocking badge is not disabled
        Espresso.onView(ViewMatchers.withId(R.id.mozac_browser_toolbar_tracking_protection_indicator))
            .check(
                ViewAssertions.matches(
                    ViewMatchers.withContentDescription(
                        R.string.mozac_browser_toolbar_content_description_tracking_protection_on_no_trackers_blocked
                    )
                )
            )

        // Open the menu
        Espresso.onView(ViewMatchers.withId(R.id.menuView))
            .perform(ViewActions.click())
        Espresso.onView(ViewMatchers.withId(R.id.trackers_count))
            .check(ViewAssertions.matches(Matchers.not(ViewMatchers.withText("-"))))

        // Disable blocking
        Espresso.onView(ViewMatchers.withId(R.id.blocking_switch))
            .check(ViewAssertions.matches(ViewMatchers.isChecked()))
        Espresso.onView(ViewMatchers.withId(R.id.blocking_switch))
            .perform(ViewActions.click())
        waitForWebContent()
    }

    private fun removeException() {
        mDevice.waitForIdle()
        Espresso.openActionBarOverflowOrOptionsMenu(InstrumentationRegistry.getInstrumentation().getContext())
        mDevice.waitForIdle() // wait until dialog fully appears
        Espresso.onView(ViewMatchers.withText("Remove"))
            .perform(ViewActions.click())
        Espresso.onView(ViewMatchers.withId(R.id.checkbox))
            .perform(ViewActions.click())
        Espresso.onView(ViewMatchers.withId(R.id.remove))
            .perform(ViewActions.click())
    }

    @Test
    @Throws(UiObjectNotFoundException::class)
    fun removeAllExceptionsTest() {
        addException(site)
        OpenExceptionsFragment()
        mDevice.waitForIdle()
        Espresso.onView(ViewMatchers.withId(R.id.removeAllExceptions))
            .perform(ViewActions.click())
        mDevice.waitForIdle()

        // Should be in main menu
        Espresso.onView(ViewMatchers.withText("Privacy & Security"))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))

        /* change locale to non-english in the setting, verify the locale is changed */
        val appViews = UiScrollable(UiSelector().scrollable(true))
        appViews.scrollIntoView(exceptionsTitle)
        Espresso.onView(ViewMatchers.withText("Exceptions"))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .check(ViewAssertions.matches(Matchers.not(ViewMatchers.isEnabled())))
    }
}
