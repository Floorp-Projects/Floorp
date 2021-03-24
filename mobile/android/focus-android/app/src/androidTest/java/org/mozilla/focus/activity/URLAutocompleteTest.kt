/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiObjectNotFoundException
import org.hamcrest.Matchers
import org.hamcrest.core.AllOf
import org.junit.After
import org.junit.Assert
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.helpers.EspressoHelper.childAtPosition
import org.mozilla.focus.helpers.EspressoHelper.openSettings
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

@Suppress("TooManyFunctions")
// https://testrail.stage.mozaws.net/index.php?/cases/view/104577
@RunWith(AndroidJUnit4ClassRunner::class)
class URLAutocompleteTest {
    private val site = "680news.com"

    // From API 24 and above
    private val CustomURLRow = Espresso.onData(Matchers.anything())
        .inAdapterView(
            AllOf.allOf(
                ViewMatchers.withId(android.R.id.list),
                childAtPosition(
                    ViewMatchers.withId(android.R.id.list_container),
                    0
                )
            )
        )
        .atPosition(4)

    // From API 23 and below
    private val CustomURLRow_old = Espresso.onData(Matchers.anything())
        .inAdapterView(
            AllOf.allOf(
                ViewMatchers.withId(android.R.id.list),
                childAtPosition(
                    ViewMatchers.withClassName(Matchers.`is`("android.widget.LinearLayout")),
                    0
                )
            )
        )
        .atPosition(4)

    /* TO DO: Reenable after fixing AndroidX migration issues
    private ViewInteraction AutoCompleteDialog = onView(allOf(withId(R.id.recycler_view),
            childAtPosition(withId(android.R.id.list_container), 0)));
    */
    private val AutoCompleteDialog: ViewInteraction? = null

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @After
    fun tearDown() {
        mActivityTestRule.activity.finishAndRemoveTask()
    }

    @Test
    @Throws(UiObjectNotFoundException::class)
    fun CompletionTest() {
        /* type a partial url, and check it autocompletes*/
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.text = "mozilla"
        TestHelper.hint.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.text == "mozilla.org")

        /* press x to delete the both autocomplete and suggestion */TestHelper.cleartextField.click()
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.text == "Search or enter address")
        Assert.assertFalse(TestHelper.hint.exists())

        /* type a full url, and check it does not autocomplete */TestHelper.inlineAutocompleteEditText.text =
            "http://www.mozilla.org"
        TestHelper.hint.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.text == "http://www.mozilla.org")
    }

    // TO DO: Reenable after fixing AndroidX migration issues
    @Ignore
    @Test // Add custom autocomplete, and check to see it works
    @Throws(UiObjectNotFoundException::class)
    fun CustomCompletionTest() {
        OpenCustomCompleteDialog()

        // Enable URL autocomplete, and add URL
        addAutoComplete(site)
        exitToTop()

        // Check for custom autocompletion
        checkACOn(site)
        // Remove custom autocompletion site
        OpenCustomCompleteDialog()
        removeACSite()
        exitToTop()

        // Check autocompletion
        checkACOff(site.substring(0, 3))
    }

    // TO DO: Reenable after fixing AndroidX migration issues
    @Ignore
    @Test // add custom autocompletion site, but disable autocomplete
    @Throws(UiObjectNotFoundException::class)
    fun DisableCCwithSiteTest() {
        OpenCustomCompleteDialog()
        addAutoComplete(site)
        Espresso.pressBack()
        toggleCustomAC() // Custom autocomplete is now off
        Espresso.pressBack()
        Espresso.pressBack()
        Espresso.pressBack()
        checkACOff(site.substring(0, 3))

        // Now enable autocomplete
        OpenCustomCompleteDialog()
        toggleCustomAC() // Custom autocomplete is now on
        Espresso.pressBack()
        Espresso.pressBack()
        Espresso.pressBack()

        // Check autocompletion
        checkACOn(site)
        // Cleanup
        OpenCustomCompleteDialog()
        removeACSite()
    }

    // TO DO: Reenable after fixing AndroidX migration issues
    @Ignore
    @Test
    fun DuplicateACSiteTest() {
        OpenCustomCompleteDialog()

        // Enable URL autocomplete, and tap Custom URL add button
        addAutoComplete(site)
        exitToTop()

        // Try to add same site again
        OpenCustomCompleteDialog()
        addAutoComplete(site, false)

        // Espresso cannot detect the "Already exists" popup.  Instead, check that it's
        // still in the same page
        Espresso.onView(ViewMatchers.withId(R.id.domainView))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
        Espresso.onView(ViewMatchers.withId(R.id.save))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
        Espresso.pressBack()
        Espresso.pressBack()

        // Cleanup
        removeACSite()
        Espresso.pressBack()
    }

    // exit to the main view from custom autocomplete dialog
    private fun exitToTop() {
        Espresso.pressBack()
        Espresso.pressBack()
        Espresso.pressBack()
        Espresso.pressBack()
    }

    private fun toggleTopsiteAC() {
        Espresso.onView(ViewMatchers.withText("For Top sites"))
            .perform(ViewActions.click())
    }

    private fun toggleCustomAC() {
        Espresso.onView(ViewMatchers.withText("For Sites You Add"))
            .perform(ViewActions.click())
    }

    private fun OpenCustomCompleteDialog() {
        mDevice.waitForIdle()
        openSettings()
        Espresso.onView(ViewMatchers.withText("Search"))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
        Espresso.onView(ViewMatchers.withText("URL Autocomplete"))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
        mDevice.waitForIdle()
    }

    // Check autocompletion is turned off
    @Throws(UiObjectNotFoundException::class)
    private fun checkACOff(url: String) {
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.text = url
        TestHelper.hint.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.text == url)
        TestHelper.cleartextField.click()
    }

    // Check autocompletion is turned on
    @Throws(UiObjectNotFoundException::class)
    private fun checkACOn(url: String) {
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.text = url.substring(0, 1)
        TestHelper.hint.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.text == url)
        TestHelper.cleartextField.click()
    }

    private fun removeACSite() {
        AutoCompleteDialog!!.perform(
            RecyclerViewActions.actionOnItemAtPosition<RecyclerView.ViewHolder>(
                4,
                ViewActions.click()
            )
        )
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

    @Suppress("SENSELESS_COMPARISON") // build error on (checkSuccess == null) always returns false
    private fun addAutoComplete(sitename: String, vararg checkSuccess: Boolean?) {
        AutoCompleteDialog!!.perform(
            RecyclerViewActions.actionOnItemAtPosition<RecyclerView.ViewHolder>(
                4,
                ViewActions.click()
            )
        )
        mDevice.waitForIdle()
        Espresso.onView(ViewMatchers.withText("+ Add custom URL"))
            .perform(ViewActions.click())
        Espresso.onView(ViewMatchers.withId(R.id.domainView))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
        mDevice.waitForIdle()
        Espresso.onView(ViewMatchers.withId(R.id.domainView))
            .perform(ViewActions.replaceText(sitename), ViewActions.closeSoftKeyboard())
        Espresso.onView(ViewMatchers.withId(R.id.save))
            .perform(ViewActions.click())

        if (checkSuccess == null) { // Verify new entry appears in the list
            Espresso.onView(ViewMatchers.withText("Custom URLs"))
                .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            Espresso.onView(
                AllOf.allOf(
                    ViewMatchers.withText(sitename),
                    ViewMatchers.withId(R.id.domainView)
                )
            )
                .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            mDevice.waitForIdle()
        }
    }
}
