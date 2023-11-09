/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.assert
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.hasAnySibling
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.closeSoftKeyboard
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.clearText
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.typeText
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
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.endsWith
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getAvailableSearchEngines
import org.mozilla.fenix.helpers.DataGenerationHelper.getRegionSearchEnginesList
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdAndTextExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
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
    fun verifyToolbarText(title: String) {
        onView(
            allOf(
                withId(R.id.navigationToolbar),
                hasDescendant(withContentDescription(R.string.action_bar_up_description)),
                hasDescendant(withText(title)),
            ),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifySearchEnginesSectionHeader() {
        onView(withText("Search engines")).check(matches(isDisplayed()))
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

    fun verifyEnginesShortcutsListHeader() =
        assertItemContainingTextExists(itemWithText("Engines visible on the search menu"))

    fun verifyAddressBarSectionHeader() {
        onView(withText("Address bar - Firefox Suggest")).check(matches(isDisplayed()))
    }

    fun verifyDefaultSearchEngineList() {
        defaultSearchEngineOption("Google")
            .check(matches(hasSibling(withId(R.id.engine_icon))))
            .check(matches(isDisplayed()))
        defaultSearchEngineOption("Bing")
            .check(matches(hasSibling(withId(R.id.engine_icon))))
            .check(matches(isDisplayed()))
        defaultSearchEngineOption("DuckDuckGo")
            .check(matches(hasSibling(withId(R.id.engine_icon))))
            .check(matches(isDisplayed()))
        assertItemContainingTextExists(addSearchEngineButton)
    }

    fun verifyManageShortcutsList(testRule: ComposeTestRule) {
        val availableShortcutsEngines = getRegionSearchEnginesList() + getAvailableSearchEngines()

        availableShortcutsEngines.forEach {
            testRule.onNodeWithText(it.name)
                .assert(hasAnySibling(hasContentDescription("${it.name} search engine")))
                .assertIsDisplayed()
        }

        assertItemContainingTextExists(addSearchEngineButton)
    }

    /**
     * Method that verifies the selected engines inside the Manage search shortcuts list.
     */
    fun verifySearchShortcutChecked(vararg engineShortcut: EngineShortcut) {
        engineShortcut.forEach {
            val shortcutIsChecked = mDevice.findObject(UiSelector().text(it.name))
                .getFromParent(
                    UiSelector().index(it.checkboxIndex),
                ).isChecked

            if (it.isChecked) {
                assertTrue(shortcutIsChecked)
            } else {
                assertFalse(shortcutIsChecked)
            }
        }
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

    fun selectSearchShortcut(shortcut: EngineShortcut) {
        mDevice.findObject(UiSelector().text(shortcut.name))
            .getFromParent(UiSelector().index(shortcut.checkboxIndex))
            .click()
    }

    fun toggleAutocomplete() = autocompleteSwitchButton().click()

    fun toggleShowSearchSuggestions() = showSearchSuggestionSwitchButton().click()

    fun toggleVoiceSearch() {
        voiceSearchSwitchButton().perform(click())
    }

    fun toggleClipboardSuggestion() {
        showClipboardSuggestionSwitch().click()
    }

    fun switchSearchHistoryToggle() = searchHistorySwitchButton().click()

    fun switchSearchBookmarksToggle() = searchBookmarksSwitchButton().click()

    fun switchShowSuggestionsInPrivateSessionsToggle() =
        showSuggestionsInPrivateModeSwitch().click()

    fun openAddSearchEngineMenu() = addSearchEngineButton.click()

    fun verifyEngineListContains(searchEngineName: String, shouldExist: Boolean) =
        assertItemContainingTextExists(itemWithText(searchEngineName), exists = shouldExist)

    fun verifyDefaultSearchEngineSelected(searchEngineName: String) {
        defaultSearchEngineOption(searchEngineName).check(matches(isChecked(true)))
    }

    fun verifySaveSearchEngineButtonEnabled(enabled: Boolean) {
        addSearchEngineSaveButton().check(matches(isEnabled(enabled)))
    }

    fun saveNewSearchEngine() {
        closeSoftKeyboard()
        addSearchEngineSaveButton().click()
    }

    fun typeCustomEngineDetails(engineName: String, engineURL: String) {
        try {
            mDevice.findObject(By.res("$packageName:id/edit_engine_name")).clear()
            mDevice.findObject(By.res("$packageName:id/edit_engine_name")).text = engineName
            assertItemWithResIdAndTextExists(
                itemWithResIdAndText("$packageName:id/edit_engine_name", engineName),
            )

            mDevice.findObject(By.res("$packageName:id/edit_search_string")).clear()
            mDevice.findObject(By.res("$packageName:id/edit_search_string")).text = engineURL
            assertItemWithResIdAndTextExists(
                itemWithResIdAndText("$packageName:id/edit_search_string", engineURL),
            )
        } catch (e: AssertionError) {
            println("The name or the search string were not set properly")

            mDevice.findObject(By.res("$packageName:id/edit_engine_name")).clear()
            mDevice.findObject(By.res("$packageName:id/edit_engine_name")).setText(engineName)
            assertItemWithResIdAndTextExists(
                itemWithResIdAndText("$packageName:id/edit_engine_name", engineName),
            )
            mDevice.findObject(By.res("$packageName:id/edit_search_string")).clear()
            mDevice.findObject(By.res("$packageName:id/edit_search_string")).setText(engineURL)
            assertItemWithResIdAndTextExists(
                itemWithResIdAndText("$packageName:id/edit_search_string", engineURL),
            )
        }
    }

    fun typeSearchEngineSuggestionString(searchSuggestionString: String) {
        onView(withId(R.id.edit_suggest_string))
            .click()
            .perform(clearText())
            .perform(typeText(searchSuggestionString))
    }

    // Used in the non-Compose Default search engines menu
    fun openEngineOverflowMenu(searchEngineName: String) {
        threeDotMenu(searchEngineName).waitForExists(waitingTimeShort)
        threeDotMenu(searchEngineName).click()
    }

    // Used in the composable Manage shortcuts menu, otherwise the overflow menu is not visible
    fun openCustomShortcutOverflowMenu(testRule: ComposeTestRule, searchEngineName: String) {
        testRule.onNode(overflowMenuWithSiblingText(searchEngineName)).performClick()
    }

    fun clickEdit() = onView(withText("Edit")).click()

    // Used in the Default search engine menu
    fun clickDeleteSearchEngine() =
        mDevice.findObject(
            UiSelector().textContains(getStringResource(R.string.search_engine_delete)),
        ).click()

    // Used in the composable Manage search shortcuts menu, otherwise the overflow menu is not visible
    fun clickDeleteSearchEngine(testRule: ComposeTestRule) =
        testRule.onNodeWithText("Delete").performClick()

    fun clickUndoSnackBarButton() =
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/snackbar_btn"),
        ).click()

    fun saveEditSearchEngine() {
        onView(withId(R.id.save_button)).click()
        assertItemContainingTextExists(itemContainingText("Saved"))
    }

    fun verifyInvalidTemplateSearchStringFormatError() {
        closeSoftKeyboard()
        onView(withText(getStringResource(R.string.search_add_custom_engine_error_missing_template)))
            .check(matches(isDisplayed()))
    }

    fun verifyErrorConnectingToSearchString(searchEngineName: String) {
        closeSoftKeyboard()
        onView(withText(getStringResource(R.string.search_add_custom_engine_error_cannot_reach, searchEngineName)))
            .check(matches(isDisplayed()))
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitForIdle()
            goBackButton().perform(click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun clickCustomSearchStringLearnMoreLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withId(R.id.custom_search_engines_learn_more)).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickCustomSearchSuggestionsLearnMoreLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withId(R.id.custom_search_suggestions_learn_more)).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

/**
 * Matches search shortcut items inside the 'Manage search shortcuts' menu
 * @param name, of type String, should be the name of the search engine.
 * @param checkboxIndex, of type Int, is the checkbox' index afferent to the search engine.
 * @param isChecked, of type Boolean, should show if the checkbox is expected to be checked.
 */
class EngineShortcut(
    val name: String,
    val checkboxIndex: Int,
    val isChecked: Boolean = true,
)

private val defaultSearchEngineHeader = onView(withText("Default search engine"))

private val manageSearchShortcutsHeader = onView(withText("Manage alternative search engines"))

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

private val addSearchEngineButton = mDevice.findObject(UiSelector().text("Add search engine"))

private fun addSearchEngineSaveButton() = onView(withId(R.id.save_button))

private fun threeDotMenu(searchEngineName: String) =
    mDevice.findObject(UiSelector().text(searchEngineName))
        .getFromParent(UiSelector().description("More options"))

private fun defaultSearchEngineOption(searchEngineName: String) =
    onView(
        allOf(
            withId(R.id.radio_button),
            hasSibling(withText(searchEngineName)),
        ),
    )

private fun overflowMenuWithSiblingText(text: String): SemanticsMatcher =
    hasAnySibling(hasText(text)) and hasContentDescription("More options")
