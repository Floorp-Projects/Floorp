/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.getTargetContext
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SearchSettingsRobot {

    fun verifySearchSettingsItems() {
        assertTrue(searchEngineSubMenu.exists())
        assertTrue(searchEngineDefaultOption.exists())
        assertTrue(searchSuggestionsHeading.exists())
        assertTrue(searchSuggestionDescription.exists())
        verifySearchSuggestionsSwitchState()
        assertTrue(searchSuggestionLearnMoreLink.exists())
        assertTrue(urlAutocompleteSubMenu.exists())
        assertTrue(urlAutocompleteDefaultOption.exists())
    }

    fun openSearchEngineSubMenu() {
        searchEngineSubMenu.waitForExists(waitingTime)
        searchEngineSubMenu.click()
    }

    fun selectSearchEngine(engineName: String) {
        searchEngineList.waitForExists(waitingTime)
        searchEngineList
            .getChild(UiSelector().text(engineName))
            .click()
    }

    fun clickSearchSuggestionsSwitch() {
        searchSuggestionsSwitch.waitForExists(waitingTime)
        searchSuggestionsSwitch.click()
    }

    fun verifySearchSuggestionsSwitchState(enabled: Boolean = false) {
        if (enabled) {
            assertTrue(searchSuggestionsSwitch.isChecked)
        } else {
            assertFalse(searchSuggestionsSwitch.isChecked)
        }
    }

    fun openUrlAutocompleteSubMenu() {
        urlAutocompleteSubMenu.waitForExists(waitingTime)
        urlAutocompleteSubMenu.click()
    }

    fun openManageSitesSubMenu() {
        manageSitesSubMenu.check(matches(isDisplayed()))
        manageSitesSubMenu.perform(click())
    }

    fun openAddCustomUrlDialog() {
        addCustomUrlButton.check(matches(isDisplayed()))
        addCustomUrlButton.perform(click())
    }

    fun enterCustomUrl(url: String) {
        customUrlText.check(matches(isDisplayed()))
        customUrlText.perform(typeText(url))
    }

    fun saveCustomUrl() = saveButton.perform(click())

    fun verifySavedCustomURL(url: String) {
        customUrlText.check(matches(withText(url)))
    }

    fun removeCustomUrl() {
        Espresso.openActionBarOverflowOrOptionsMenu(getTargetContext)
        onView(withText(R.string.preference_autocomplete_menu_remove)).perform(click())
        customUrlText.perform(click())
        onView(withId(R.id.remove)).perform(click())
    }

    fun verifyCustomUrlDialogNotClosed() {
        saveButton.check(matches(isDisplayed()))
    }

    fun toggleCustomAutocomplete() {
        onView(withText(R.string.preference_switch_autocomplete_user_list)).perform(click())
    }

    fun toggleTopSitesAutocomplete() {
        onView(withText(R.string.preference_switch_autocomplete_topsites)).perform(click())
    }

    class Transition
}

private val searchEngineSubMenu =
    UiScrollable(UiSelector().resourceId("$packageName:id/recycler_view"))
        .getChild(UiSelector().text(getStringResource(R.string.preference_search_engine_label)))

private val searchEngineDefaultOption =
    mDevice.findObject(UiSelector().textContains("Google"))

private val searchEngineList = UiScrollable(
    UiSelector()
        .resourceId("$packageName:id/search_engine_group").enabled(true),
)

private val searchSuggestionsHeading: UiObject =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.preference_show_search_suggestions)),
    )

private val searchSuggestionDescription: UiObject =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.preference_show_search_suggestions_summary)),
    )

private val searchSuggestionLearnMoreLink: UiObject =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/link"),
    )

private val searchSuggestionsSwitch: UiObject =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/switchWidget"),
    )

private val urlAutocompleteSubMenu =
    UiScrollable(UiSelector().resourceId("$packageName:id/recycler_view"))
        .getChildByText(
            UiSelector().text(getStringResource(R.string.preference_subitem_autocomplete)),
            getStringResource(R.string.preference_subitem_autocomplete),
            true,
        )

private val urlAutocompleteDefaultOption =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.preference_state_on)),
    )

private val manageSitesSubMenu = onView(withText(R.string.preference_autocomplete_subitem_manage_sites))

private val addCustomUrlButton = onView(withText(R.string.preference_autocomplete_action_add))

private val customUrlText = onView(withId(R.id.domainView))

private val saveButton = onView(withId(R.id.save))
