/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withChild
import androidx.test.espresso.matcher.ViewMatchers.withClassName
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers
import org.hamcrest.CoreMatchers.not
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.endsWith
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.hasCousin
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked
import org.mozilla.fenix.helpers.isEnabled

/**
 * Implementation of Robot Pattern for the settings search sub menu.
 */
class SettingsSubMenuSearchRobot {
    fun verifySearchSettingsToolbar() {
        onView(
            allOf(
                withId(R.id.navigationToolbar),
                hasDescendant(withContentDescription(R.string.action_bar_up_description)),
                hasDescendant(withText(R.string.preferences_search)),
            ),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifySearchEnginesSectionHeader() {
        onView(withText("Search Engines")).check(matches(isDisplayed()))
    }

    fun verifyDefaultSearchEngineHeader() {
        defaultSearchEngineHeader
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifyDefaultSearchEngineSummary(engineName: String) {
        defaultSearchEngineHeader.check(matches(hasSibling(withText(engineName))))
    }

    fun verifyManageSearchShortcutsHeader() {
        manageSearchShortcutsHeader.check(matches(isDisplayed()))
    }

    fun verifyManageShortcutsSummary() {
        manageSearchShortcutsHeader
            .check(matches(hasSibling(withText("Edit engines visible in the search menu"))))
    }

    fun verifyAddressBarSectionHeader() {
        onView(withText("Address bar")).check(matches(isDisplayed()))
    }

    fun verifySearchEngineList() {
        onView(withText("Google"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        onView(withText("Amazon.com"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        onView(withText("Bing"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        onView(withText("DuckDuckGo"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        onView(withText("Wikipedia"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        onView(withText("Add search engine"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifyAutocompleteURlsIsEnabled(enabled: Boolean) {
        autocompleteSwitchButton()
            .check(matches(hasCousin(allOf(withClassName(endsWith("Switch")), isChecked(enabled)))))
    }

    fun verifyShowSearchSuggestionsEnabled(enabled: Boolean) {
        showSearchSuggestionSwitchButton()
            .check(matches(hasCousin(allOf(withClassName(endsWith("Switch")), isChecked(enabled)))))
    }

    fun verifyShowSearchSuggestionsInPrivateEnabled(enabled: Boolean) {
        showSuggestionsInPrivateModeSwitch()
            .check(
                matches(
                    hasSibling(
                        withChild(
                            allOf(
                                withClassName(endsWith("CheckBox")),
                                isChecked(enabled),
                            ),
                        ),
                    ),
                ),
            )
    }

    fun verifyShowClipboardSuggestionsEnabled(enabled: Boolean) {
        showClipboardSuggestionSwitch()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            .check(matches(hasCousin(allOf(withClassName(endsWith("Switch")), isChecked(enabled)))))
    }

    fun verifySearchBrowsingHistoryEnabled(enabled: Boolean) {
        searchHistorySwitchButton()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            .check(matches(hasCousin(allOf(withClassName(endsWith("Switch")), isChecked(enabled)))))
    }

    fun verifySearchBookmarksEnabled(enabled: Boolean) {
        searchBookmarksSwitchButton()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            .check(matches(hasCousin(allOf(withClassName(endsWith("Switch")), isChecked(enabled)))))
    }

    fun verifySearchSyncedTabsEnabled(enabled: Boolean) {
        searchSyncedTabsSwitchButton()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            .check(matches(hasCousin(allOf(withClassName(endsWith("Switch")), isChecked(enabled)))))
    }

    fun verifyVoiceSearchEnabled(enabled: Boolean) {
        voiceSearchSwitchButton()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            .check(matches(hasCousin(allOf(withClassName(endsWith("Switch")), isChecked(enabled)))))
    }

    fun openDefaultSearchEngineMenu() {
        defaultSearchEngineHeader.click()
    }

    fun openManageShortcutsMenu() {
        manageSearchShortcutsHeader.click()
    }

    fun changeDefaultSearchEngine(searchEngineName: String) {
        onView(withText(searchEngineName))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            .perform(click())
    }

    fun toggleAutocomplete() = autocompleteSwitchButton().click()

    fun toggleShowSearchSuggestions() = showSearchSuggestionSwitchButton().click()

    fun toggleShowSearchShortcuts() =
        itemContainingText(getStringResource(R.string.preferences_show_search_engines))
            .also {
                it.waitForExists(waitingTimeShort)
                it.click()
            }

    fun toggleVoiceSearch() {
        voiceSearchSwitchButton().perform(click())
    }

    fun toggleClipboardSuggestion() {
        showClipboardSuggestionSwitch().click()
    }

    fun switchSearchHistoryToggle() = searchHistorySwitchButton().click()

    fun switchSearchBookmarksToggle() = searchBookmarksSwitchButton().click()

    fun switchShowSuggestionsInPrivateSessionsToggle() = showSuggestionsInPrivateModeSwitch().click()

    fun openAddSearchEngineMenu() = addSearchEngineButton().click()

    fun verifyEngineListContains(searchEngineName: String) {
        assertTrue(itemWithText(searchEngineName).waitForExists(waitingTimeShort))
    }

    fun verifyEngineListDoesNotContain(searchEngineName: String) = assertEngineListDoesNotContain(searchEngineName)

    fun verifyDefaultSearchEngine(searchEngineName: String) {
        onView(
            allOf(
                withId(R.id.radio_button),
                withParent(withChild(withText(searchEngineName))),
            ),
        ).check(matches(isChecked(true)))
    }

    fun verifyThreeDotButtonIsNotDisplayed(searchEngineName: String) = assertThreeDotButtonIsNotDisplayed(searchEngineName)

    fun verifyAddSearchEngineListContains(vararg searchEngines: String) {
        for (searchEngine in searchEngines) {
            onView(withId(R.id.search_engine_group)).check(matches(hasDescendant(withText(searchEngine))))
        }
    }

    fun verifySaveSearchEngineButtonEnabled(enabled: Boolean) {
        addSearchEngineSaveButton().check(matches(isEnabled(enabled)))
    }

    fun saveNewSearchEngine() {
        addSearchEngineSaveButton().click()
        assertTrue(
            mDevice.findObject(
                UiSelector().textContains("Default search engine"),
            ).waitForExists(waitingTime),
        )
    }

    fun typeCustomEngineDetails(engineName: String, engineURL: String) {
        try {
            mDevice.findObject(By.res("$packageName:id/edit_engine_name")).clear()
            mDevice.findObject(By.res("$packageName:id/edit_engine_name")).text = engineName
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/edit_engine_name")
                        .text(engineName),
                ).waitForExists(waitingTime),
            )

            mDevice.findObject(By.res("$packageName:id/edit_search_string")).clear()
            mDevice.findObject(By.res("$packageName:id/edit_search_string")).text = engineURL
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/edit_search_string")
                        .text(engineURL),
                ).waitForExists(waitingTime),
            )
        } catch (e: AssertionError) {
            println("The name or the search string were not set properly")
//
//            // Lets again set both name and search string
//            goBackButton().click()
//            openAddSearchEngineMenu()

            mDevice.findObject(By.res("$packageName:id/edit_engine_name")).clear()
            mDevice.findObject(By.res("$packageName:id/edit_engine_name")).setText(engineName)
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/edit_engine_name")
                        .text(engineName),
                ).waitForExists(waitingTime),
            )

            mDevice.findObject(By.res("$packageName:id/edit_search_string")).clear()
            mDevice.findObject(By.res("$packageName:id/edit_search_string")).setText(engineURL)
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/edit_search_string")
                        .text(engineURL),
                ).waitForExists(waitingTime),
            )
        }
    }

    fun openEngineOverflowMenu(searchEngineName: String) {
        mDevice.findObject(
            UiSelector().resourceId("org.mozilla.fenix.debug:id/overflow_menu"),
        ).waitForExists(waitingTime)
        threeDotMenu(searchEngineName).click()
    }

    fun deleteMultipleSearchEngines(vararg searchEngines: String) {
        for (searchEngine in searchEngines) {
            openEngineOverflowMenu(searchEngine)
            clickDeleteSearchEngine()
        }
    }

    fun clickEdit() = onView(withText("Edit")).click()

    fun clickDeleteSearchEngine() =
        mDevice.findObject(
            UiSelector().textContains(getStringResource(R.string.search_engine_delete)),
        ).click()

    fun clickUndoSnackBarButton() =
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/snackbar_btn"),
        ).click()

    fun saveEditSearchEngine() {
        onView(withId(R.id.save_button)).click()
        assertTrue(
            mDevice.findObject(
                UiSelector().textContains("Saved"),
            ).waitForExists(waitingTime),
        )
    }

    fun verifyShowSearchEnginesToggleState(enabled: Boolean) = assertShowSearchEnginesToggle(enabled)

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitForIdle()
            goBackButton().perform(click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

fun searchSettingsScreen(interact: SettingsSubMenuSearchRobot.() -> Unit): SettingsSubMenuSearchRobot.Transition {
    SettingsSubMenuSearchRobot().interact()
    return SettingsSubMenuSearchRobot.Transition()
}

private val defaultSearchEngineHeader =
    onView(withText("Default search engine"))

private val manageSearchShortcutsHeader = onView(withText("Manage search shortcuts"))

private fun searchHistorySwitchButton(): ViewInteraction {
    onView(withId(androidx.preference.R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Search browsing history")),
        ),
    )
    return onView(withText("Search browsing history"))
}

private fun searchBookmarksSwitchButton(): ViewInteraction {
    onView(withId(androidx.preference.R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Search bookmarks")),
        ),
    )
    return onView(withText("Search bookmarks"))
}

private fun searchSyncedTabsSwitchButton(): ViewInteraction {
    onView(withId(androidx.preference.R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Search synced tabs")),
        ),
    )
    return onView(withText("Search synced tabs"))
}

private fun voiceSearchSwitchButton(): ViewInteraction {
    onView(withId(androidx.preference.R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Show voice search")),
        ),
    )
    return onView(withText("Show voice search"))
}

private fun autocompleteSwitchButton(): ViewInteraction {
    onView(withId(androidx.preference.R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText(getStringResource(R.string.preferences_enable_autocomplete_urls))),
        ),
    )

    return onView(withText(getStringResource(R.string.preferences_enable_autocomplete_urls)))
}

private fun showSearchSuggestionSwitchButton(): ViewInteraction {
    onView(withId(androidx.preference.R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Show search suggestions")),
        ),
    )

    return onView(withText("Show search suggestions"))
}

private fun showClipboardSuggestionSwitch(): ViewInteraction {
    onView(withId(androidx.preference.R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText(getStringResource(R.string.preferences_show_clipboard_suggestions))),
        ),
    )
    return onView(withText(getStringResource(R.string.preferences_show_clipboard_suggestions)))
}

private fun showSuggestionsInPrivateModeSwitch(): ViewInteraction {
    onView(withId(androidx.preference.R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText(getStringResource(R.string.preferences_show_search_suggestions_in_private))),
        ),
    )
    return onView(withText(getStringResource(R.string.preferences_show_search_suggestions_in_private)))
}

private fun goBackButton() =
    onView(CoreMatchers.allOf(withContentDescription("Navigate up")))

private fun addSearchEngineButton() = onView(withText("Add search engine"))

private fun addSearchEngineSaveButton() = onView(withId(R.id.save_button))

private fun assertEngineListDoesNotContain(searchEngineName: String) {
    onView(withId(R.id.search_engine_group)).check(matches(not(hasDescendant(withText(searchEngineName)))))
}

private fun assertThreeDotButtonIsNotDisplayed(searchEngineName: String) =
    threeDotMenu(searchEngineName).check(matches(not(isDisplayed())))

private fun assertShowSearchEnginesToggle(enabled: Boolean) =
    if (enabled) {
        onView(withText(R.string.preferences_show_search_engines))
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            ViewMatchers.isChecked(),
                        ),
                    ),
                ),
            )
    } else {
        onView(withText(R.string.preferences_show_search_engines))
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            ViewMatchers.isNotChecked(),
                        ),
                    ),
                ),
            )
    }

private fun threeDotMenu(searchEngineName: String) =
    onView(
        allOf(
            withId(R.id.overflow_menu),
            withParent(withChild(withText(searchEngineName))),
        ),
    )
