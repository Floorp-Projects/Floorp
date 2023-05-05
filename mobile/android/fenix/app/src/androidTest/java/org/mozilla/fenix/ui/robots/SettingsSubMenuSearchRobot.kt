/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
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
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.hasCousin
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked

/**
 * Implementation of Robot Pattern for the settings search sub menu.
 */
class SettingsSubMenuSearchRobot {
    fun verifySearchToolbar() = assertSearchToolbar()
    fun verifyDefaultSearchEngineHeader() = assertDefaultSearchEngineHeader()
    fun verifySearchEngineList() = assertSearchEngineList()

    fun verifyShowSearchSuggestions() {
        onView(withId(androidx.preference.R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Show search suggestions")),
            ),
        )
        onView(withText("Show search suggestions"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifyShowSearchShortcuts() = assertShowSearchShortcuts()

    fun verifyShowClipboardSuggestionsDefault() {
        onView(withId(androidx.preference.R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText(getStringResource(R.string.preferences_show_clipboard_suggestions))),
            ),
        )
        onView(withText(getStringResource(R.string.preferences_show_clipboard_suggestions)))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            .check(matches(hasCousin(allOf(withClassName(endsWith("Switch")), isChecked(true)))))
    }

    fun toggleClipboardSuggestion() {
        onView(withText(getStringResource(R.string.preferences_show_clipboard_suggestions)))
            .click()
    }

    fun verifySearchBrowsingHistory() {
        onView(withId(androidx.preference.R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Search browsing history")),
            ),
        )
        searchHistoryToggle
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifySearchBookmarks() {
        onView(withId(androidx.preference.R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Search bookmarks")),
            ),
        )
        searchBookmarksToggle
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun changeDefaultSearchEngine(searchEngineName: String) =
        selectSearchEngine(searchEngineName)

    fun toggleAutocomplete() {
        onView(withId(androidx.preference.R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText(getStringResource(R.string.preferences_enable_autocomplete_urls))),
            ),
        )

        onView(withText(getStringResource(R.string.preferences_enable_autocomplete_urls)))
            .click()
    }

    fun toggleShowSearchSuggestions() {
        onView(withId(androidx.preference.R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Show search suggestions")),
            ),
        )

        onView(withText("Show search suggestions"))
            .perform(click())
    }

    fun toggleShowSearchShortcuts() =
        itemContainingText(getStringResource(R.string.preferences_show_search_engines))
            .also {
                it.waitForExists(waitingTimeShort)
                it.click()
            }

    fun toggleVoiceSearch() {
        onView(withId(androidx.preference.R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Show voice search")),
            ),
        )
        onView(withText("Show voice search")).perform(click())
    }

    fun switchSearchHistoryToggle() {
        onView(withId(androidx.preference.R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Search browsing history")),
            ),
        )
        searchHistoryToggle.click()
    }

    fun switchSearchBookmarksToggle() {
        onView(withId(androidx.preference.R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Search bookmarks")),
            ),
        )
        searchBookmarksToggle.click()
    }

    fun toggleShowSuggestionsInPrivateSessions() {
        onView(withId(androidx.preference.R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText(getStringResource(R.string.preferences_show_search_suggestions_in_private))),
            ),
        )
        onView(withText(getStringResource(R.string.preferences_show_search_suggestions_in_private)))
            .click()
    }

    fun openAddSearchEngineMenu() = addSearchEngineButton().click()

    fun verifyAddSearchEngineList() = assertAddSearchEngineList()

    fun verifyEngineListContains(searchEngineName: String) = assertEngineListContains(searchEngineName)

    fun verifyEngineListDoesNotContain(searchEngineName: String) = assertEngineListDoesNotContain(searchEngineName)

    fun verifyDefaultSearchEngine(searchEngineName: String) = assertDefaultSearchEngine(searchEngineName)

    fun verifyThreeDotButtonIsNotDisplayed(searchEngineName: String) = assertThreeDotButtonIsNotDisplayed(searchEngineName)

    fun verifyAddSearchEngineListContains(vararg searchEngines: String) {
        for (searchEngine in searchEngines) {
            assertEngineListContains(searchEngine)
        }
    }

    fun saveNewSearchEngine() {
        addSearchEngineSaveButton().click()
        assertTrue(
            mDevice.findObject(
                UiSelector().textContains("Default search engine"),
            ).waitForExists(waitingTime),
        )
    }

    fun addNewSearchEngine(searchEngineName: String) {
        selectSearchEngine(searchEngineName)
        saveNewSearchEngine()
    }

    fun selectAddCustomSearchEngine() = onView(withText("Other")).click()

    fun typeCustomEngineDetails(engineName: String, engineURL: String) {
        mDevice.findObject(By.res("$packageName:id/edit_engine_name")).clear()
        mDevice.findObject(By.res("$packageName:id/edit_engine_name")).setText(engineName)
        mDevice.findObject(By.res("$packageName:id/edit_search_string")).clear()
        mDevice.findObject(By.res("$packageName:id/edit_search_string")).setText(engineURL)

        try {
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/edit_engine_name")
                        .text(engineName),
                ).waitForExists(waitingTime),
            )

            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/edit_search_string")
                        .text(engineURL),
                ).waitForExists(waitingTime),
            )
        } catch (e: AssertionError) {
            println("The name or the search string were not set properly")

            // Lets again set both name and search string
            goBackButton().click()
            openAddSearchEngineMenu()
            selectAddCustomSearchEngine()

            mDevice.findObject(By.res("$packageName:id/edit_engine_name")).clear()
            mDevice.findObject(By.res("$packageName:id/edit_engine_name")).setText(engineName)
            mDevice.findObject(By.res("$packageName:id/edit_search_string")).clear()
            mDevice.findObject(By.res("$packageName:id/edit_search_string")).setText(engineURL)

            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/edit_engine_name")
                        .text(engineName),
                ).waitForExists(waitingTime),
            )

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

private fun assertSearchToolbar() =
    onView(
        allOf(
            withId(R.id.navigationToolbar),
            hasDescendant(withContentDescription(R.string.action_bar_up_description)),
            hasDescendant(withText(R.string.preferences_search)),
        ),
    ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertDefaultSearchEngineHeader() =
    onView(withText("Default search engine"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertSearchEngineList() {
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

private fun assertShowSearchShortcuts() {
    onView(withId(androidx.preference.R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Show search engines")),
        ),
    )
    onView(withText("Show search engines"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private val searchHistoryToggle = onView(withText("Search browsing history"))

private val searchBookmarksToggle = onView(withText("Search bookmarks"))

private fun selectSearchEngine(searchEngine: String) {
    onView(withText(searchEngine))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        .perform(click())
}

private fun goBackButton() =
    onView(CoreMatchers.allOf(withContentDescription("Navigate up")))

private fun addSearchEngineButton() = onView(withText("Add search engine"))

private fun assertAddSearchEngineList() {
    onView(withText("Reddit")).check(matches(isDisplayed()))
    onView(withText("YouTube")).check(matches(isDisplayed()))
    onView(withText("Other")).check(matches(isDisplayed()))
}

private fun addSearchEngineSaveButton() = onView(withId(R.id.add_search_engine))

private fun assertEngineListContains(searchEngineName: String) {
    onView(withId(R.id.search_engine_group)).check(matches(hasDescendant(withText(searchEngineName))))
}

private fun assertDefaultSearchEngine(searchEngineName: String) =
    onView(
        allOf(
            withId(R.id.radio_button),
            withParent(withChild(withText(searchEngineName))),
        ),
    ).check(matches(isChecked(true)))

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
