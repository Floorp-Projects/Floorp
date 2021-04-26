/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.annotation.IdRes
import androidx.test.espresso.Espresso
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import org.hamcrest.Matchers
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.helpers.EspressoHelper.openMenu
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.idlingResources.SessionLoadedIdlingResource

// This test visits each page and checks whether some essential elements are being displayed
// https://testrail.stage.mozaws.net/index.php?/cases/view/81664
@RunWith(AndroidJUnit4ClassRunner::class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
class AccessAboutAndYourRightsPagesTest {
    @get: Rule
    val mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    private var loadingIdlingResource: SessionLoadedIdlingResource? = null
    @Before
    fun setUp() {
        loadingIdlingResource = SessionLoadedIdlingResource()
        IdlingRegistry.getInstance().register(loadingIdlingResource)
    }

    @After
    fun tearDown() {
        IdlingRegistry.getInstance().unregister(loadingIdlingResource)
        mActivityTestRule.activity.finishAndRemoveTask()
    }

    @Test
    fun AboutAndYourRightsPagesTest() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext

        // What's new page
        openMenu()
        clickMenuItem(R.id.whats_new)
        assertWebsiteUrlContains("support.mozilla.org")
        Espresso.pressBack()

        // Help page
        openMenu()
        clickMenuItem(R.id.help)
        assertWebsiteUrlContains("what-firefox-focus-android")
        Espresso.pressBack()

        // Go to settings "About" page
        openMenu()
        clickMenuItem(R.id.settings)
        val mozillaMenuLabel = context.getString(
            R.string.preference_category_mozilla,
            context.getString(R.string.app_name)
        )
        Espresso.onView(ViewMatchers.withText(mozillaMenuLabel))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
        val aboutLabel =
            context.getString(R.string.preference_about, context.getString(R.string.app_name))
        Espresso.onView(ViewMatchers.withText(aboutLabel))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
        Espresso.onView(ViewMatchers.withId(R.id.infofragment))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
        Espresso.pressBack() // This takes to Mozilla Submenu of settings

        // "Your rights" page
        Espresso.onView(ViewMatchers.withText(context.getString(R.string.your_rights)))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
        Espresso.onView(ViewMatchers.withId(R.id.infofragment))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
    }

    private fun clickMenuItem(@IdRes id: Int) {
        Espresso.onView(ViewMatchers.withId(id))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
    }

    private fun assertWebsiteUrlContains(substring: String) {
        Espresso.onView(ViewMatchers.withId(R.id.display_url))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .check(ViewAssertions.matches(ViewMatchers.withText(Matchers.containsString(substring))))
    }
}
