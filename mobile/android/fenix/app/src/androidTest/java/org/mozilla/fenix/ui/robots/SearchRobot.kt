/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.ComposeTimeoutException
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertAny
import androidx.compose.ui.test.assertCountEquals
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performScrollToNode
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.PositionAssertions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers.allOf
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.AppAndSystemHelper.grantSystemPermission
import org.mozilla.fenix.helpers.AppAndSystemHelper.isPackageInstalled
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_QUICK_SEARCH
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.Constants.SPEECH_RECOGNITION
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertItemTextContains
import org.mozilla.fenix.helpers.MatcherHelper.assertItemTextEquals
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectIsGone
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.SessionLoadedIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.waitForObjects

/**
 * Implementation of Robot Pattern for the search fragment.
 */
class SearchRobot {
    fun verifySearchView() = assertUIObjectExists(itemWithResId("$packageName:id/search_wrapper"))

    fun verifySearchToolbar(isDisplayed: Boolean) =
        assertUIObjectExists(
            itemWithResId("$packageName:id/mozac_browser_toolbar_edit_url_view"),
            exists = isDisplayed,
        )

    fun verifyScanButtonVisibility(visible: Boolean = true) =
        assertUIObjectExists(scanButton(), exists = visible)

    fun verifyVoiceSearchButtonVisibility(enabled: Boolean) =
        assertUIObjectExists(voiceSearchButton(), exists = enabled)

    // Device or AVD requires a Google Services Android OS installation
    fun startVoiceSearch() {
        Log.i(TAG, "startVoiceSearch: Trying to click the voice search button button")
        voiceSearchButton().click()
        Log.i(TAG, "startVoiceSearch: Clicked the voice search button button")
        grantSystemPermission()

        if (isPackageInstalled(GOOGLE_QUICK_SEARCH)) {
            Log.i(TAG, "startVoiceSearch: $GOOGLE_QUICK_SEARCH is installed")
            Log.i(TAG, "startVoiceSearch: Trying to verify the intent to: $GOOGLE_QUICK_SEARCH")
            Intents.intended(IntentMatchers.hasAction(SPEECH_RECOGNITION))
            Log.i(TAG, "startVoiceSearch: Verified the intent to: $GOOGLE_QUICK_SEARCH")
        }
    }

    fun verifySearchEngineSuggestionResults(
        rule: ComposeTestRule,
        vararg searchSuggestions: String,
        searchTerm: String,
        shouldEditKeyword: Boolean = false,
        numberOfDeletionSteps: Int = 0,
    ) {
        rule.waitForIdle()
        for (i in 1..RETRY_COUNT) {
            Log.i(TAG, "verifySearchEngineSuggestionResults: Started try #$i")
            try {
                for (searchSuggestion in searchSuggestions) {
                    mDevice.waitForObjects(mDevice.findObject(UiSelector().textContains(searchSuggestion)))
                    Log.i(TAG, "verifySearchEngineSuggestionResults: Trying to perform scroll action to $searchSuggestion search suggestion")
                    rule.onNodeWithTag("mozac.awesomebar.suggestions").performScrollToNode(hasText(searchSuggestion))
                    Log.i(TAG, "verifySearchEngineSuggestionResults: Performed scroll action to $searchSuggestion search suggestion")
                    Log.i(TAG, "verifySearchEngineSuggestionResults: Trying to verify that $searchSuggestion search suggestion exists")
                    rule.onNodeWithTag("mozac.awesomebar.suggestions").assertExists()
                    Log.i(TAG, "verifySearchEngineSuggestionResults: Verified that $searchSuggestion search suggestion exists")
                }
                break
            } catch (e: AssertionError) {
                Log.i(TAG, "verifySearchEngineSuggestionResults: AssertionError caught, executing fallback methods")
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    mDevice.pressBack()
                    homeScreen {
                    }.openSearch {
                        typeSearch(searchTerm)
                        if (shouldEditKeyword) {
                            deleteSearchKeywordCharacters(numberOfDeletionSteps = numberOfDeletionSteps)
                        }
                    }
                }
            }
        }
    }

    fun verifySuggestionsAreNotDisplayed(rule: ComposeTestRule, vararg searchSuggestions: String) {
        Log.i(TAG, "verifySuggestionsAreNotDisplayed: Waiting for compose test rule to be idle")
        rule.waitForIdle()
        Log.i(TAG, "verifySuggestionsAreNotDisplayed: Waited for compose test rule to be idle")
        for (searchSuggestion in searchSuggestions) {
            Log.i(TAG, "verifySuggestionsAreNotDisplayed: Trying to verify that there are no $searchSuggestion related search suggestions")
            rule.onAllNodesWithTag("mozac.awesomebar.suggestions")
                .assertAny(
                    hasText(searchSuggestion)
                        .not(),
                )
            Log.i(TAG, "verifySuggestionsAreNotDisplayed: Verified that there are no $searchSuggestion related search suggestions")
        }
    }

    @OptIn(ExperimentalTestApi::class)
    fun verifySearchSuggestionsCount(rule: ComposeTestRule, numberOfSuggestions: Int, searchTerm: String) {
        for (i in 1..RETRY_COUNT) {
            Log.i(TAG, "verifySearchSuggestionsCount: Started try #$i")
            try {
                Log.i(TAG, "verifySearchSuggestionsCount: Compose test rule is waiting for $waitingTime ms until the note count equals to: $numberOfSuggestions")
                rule.waitUntilNodeCount(hasTestTag("mozac.awesomebar.suggestion"), numberOfSuggestions, waitingTime)
                Log.i(TAG, "verifySearchSuggestionsCount: Compose test rule waited for $waitingTime ms until the note count equals to: $numberOfSuggestions")
                Log.i(TAG, "verifySearchSuggestionsCount: Trying to verify that the count of the search suggestions equals: $numberOfSuggestions")
                rule.onAllNodesWithTag("mozac.awesomebar.suggestion").assertCountEquals(numberOfSuggestions)
                Log.i(TAG, "verifySearchSuggestionsCount: Verified that the count of the search suggestions equals: $numberOfSuggestions")

                break
            } catch (e: ComposeTimeoutException) {
                Log.i(TAG, "verifySearchSuggestionsCount: ComposeTimeoutException caught, executing fallback methods")
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    Log.i(TAG, "verifySearchSuggestionsCount: Trying to click device back button")
                    mDevice.pressBack()
                    Log.i(TAG, "verifySearchSuggestionsCount: Clicked device back button")
                    homeScreen {
                    }.openSearch {
                        typeSearch(searchTerm)
                    }
                }
            }
        }
    }

    fun verifyAllowSuggestionsInPrivateModeDialog() =
        assertUIObjectExists(
            itemWithText(getStringResource(R.string.search_suggestions_onboarding_title)),
            itemWithText(getStringResource(R.string.search_suggestions_onboarding_text)),
            itemWithText("Learn more"),
            itemWithText(getStringResource(R.string.search_suggestions_onboarding_allow_button)),
            itemWithText(getStringResource(R.string.search_suggestions_onboarding_do_not_allow_button)),
        )

    fun denySuggestionsInPrivateMode() {
        Log.i(TAG, "denySuggestionsInPrivateMode: Trying to click the \"Don’t allow\" button")
        mDevice.findObject(
            UiSelector().text(getStringResource(R.string.search_suggestions_onboarding_do_not_allow_button)),
        ).click()
        Log.i(TAG, "denySuggestionsInPrivateMode: Clicked the \"Don’t allow\" button")
    }

    fun allowSuggestionsInPrivateMode() {
        Log.i(TAG, "allowSuggestionsInPrivateMode: Trying to click the \"Allow\" button")
        mDevice.findObject(
            UiSelector().text(getStringResource(R.string.search_suggestions_onboarding_allow_button)),
        ).click()
        Log.i(TAG, "allowSuggestionsInPrivateMode: Clicked the \"Allow\" button")
    }

    fun verifySearchSelectorButton() = assertUIObjectExists(searchSelectorButton())

    fun clickSearchSelectorButton() {
        Log.i(TAG, "clickSearchSelectorButton: Waiting for $waitingTime ms for search selector button to exist")
        searchSelectorButton().waitForExists(waitingTime)
        Log.i(TAG, "clickSearchSelectorButton: Waited for $waitingTime ms for search selector button to exist")
        Log.i(TAG, "clickSearchSelectorButton: Trying to click the search selector button")
        searchSelectorButton().click()
        Log.i(TAG, "clickSearchSelectorButton: Clicked the search selector button")
    }

    fun verifySearchEngineIcon(name: String) = assertUIObjectExists(itemWithDescription(name))

    fun verifySearchBarPlaceholder(text: String) {
        Log.i(TAG, "verifySearchBarPlaceholder: Waiting for $waitingTime ms for the edit mode toolbar to exist")
        browserToolbarEditView().waitForExists(waitingTime)
        Log.i(TAG, "verifySearchBarPlaceholder: Waited for $waitingTime ms for the edit mode toolbar to exist")
        assertItemTextEquals(browserToolbarEditView(), expectedText = text)
    }

    fun verifySearchShortcutListContains(vararg searchEngineName: String, shouldExist: Boolean = true) {
        searchEngineName.forEach {
            if (shouldExist) {
                assertUIObjectExists(
                    searchShortcutList().getChild(UiSelector().text(it)),
                )
            } else {
                assertUIObjectIsGone(searchShortcutList().getChild(UiSelector().text(it)))
            }
        }
    }

    // New unified search UI search selector.
    fun selectTemporarySearchMethod(searchEngineName: String) {
        Log.i(TAG, "selectTemporarySearchMethod: Trying to click the $searchEngineName search shortcut")
        searchShortcutList().getChild(UiSelector().text(searchEngineName)).click()
        Log.i(TAG, "selectTemporarySearchMethod: Clicked the $searchEngineName search shortcut")
    }

    fun clickScanButton() =
        scanButton().also {
            Log.i(TAG, "clickScanButton: Waiting for $waitingTime ms for the scan button to exist")
            it.waitForExists(waitingTime)
            Log.i(TAG, "clickScanButton: Waited for $waitingTime ms for the scan button to exist")
            Log.i(TAG, "clickScanButton: Trying to click the scan button")
            it.click()
            Log.i(TAG, "clickScanButton: Clicked the scan button")
        }

    fun clickDismissPermissionRequiredDialog() {
        Log.i(TAG, "clickDismissPermissionRequiredDialog: Waiting for $waitingTime ms for the \"Dismiss\" permission button to exist")
        dismissPermissionButton().waitForExists(waitingTime)
        Log.i(TAG, "clickDismissPermissionRequiredDialog: Waited for $waitingTime ms for the \"Dismiss\" permission button to exist")
        Log.i(TAG, "clickDismissPermissionRequiredDialog: Trying to click the \"Dismiss\" permission button")
        dismissPermissionButton().click()
        Log.i(TAG, "clickDismissPermissionRequiredDialog: Clicked the \"Dismiss\" permission button")
    }

    fun clickGoToPermissionsSettings() {
        Log.i(TAG, "clickGoToPermissionsSettings: Waiting for $waitingTime ms for the \"Go To Settings\" permission button to exist")
        goToPermissionsSettingsButton().waitForExists(waitingTime)
        Log.i(TAG, "clickGoToPermissionsSettings: Waited for $waitingTime ms for the \"Go To Settings\" permission button to exist")
        Log.i(TAG, "clickGoToPermissionsSettings: Trying to click the \"Go To Settings\" permission button")
        goToPermissionsSettingsButton().click()
        Log.i(TAG, "clickGoToPermissionsSettings: Clicked the \"Go To Settings\" permission button")
    }

    fun verifyScannerOpen() {
        Log.i(TAG, "verifyScannerOpen: Trying to verify that the device camera is opened or that the camera app error message exist")
        assertTrue(
            "$TAG: Neither the device camera was opened nor the camera app error message was displayed",
            mDevice.findObject(UiSelector().resourceId("$packageName:id/view_finder"))
                .waitForExists(waitingTime) ||
                // In case there is no camera available, an error will be shown.
                mDevice.findObject(UiSelector().resourceId("$packageName:id/camera_error"))
                    .exists(),
        )
        Log.i(TAG, "verifyScannerOpen: Verified that the device camera is opened or that the camera app error message exist")
    }

    fun typeSearch(searchTerm: String) {
        Log.i(TAG, "typeSearch: Waiting for $waitingTime ms for the edit mode toolbar to exist")
        browserToolbarEditView().waitForExists(waitingTime)
        Log.i(TAG, "typeSearch: Waited for $waitingTime ms for the edit mode toolbar to exist")
        Log.i(TAG, "typeSearch: Trying to set the edit mode toolbar text to $searchTerm")
        browserToolbarEditView().setText(searchTerm)
        Log.i(TAG, "typeSearch: Edit mode toolbar text was set to $searchTerm")
        Log.i(TAG, "typeSearch: Waiting for device to be idle")
        mDevice.waitForIdle()
        Log.i(TAG, "typeSearch: Waited for device to be idle")
    }

    fun clickClearButton() {
        Log.i(TAG, "clickClearButton: Trying to click the clear button")
        clearButton().click()
        Log.i(TAG, "clickClearButton: Clicked the clear button")
    }

    fun tapOutsideToDismissSearchBar() {
        Log.i(TAG, "tapOutsideToDismissSearchBar: Trying to click the search wrapper")
        itemWithResId("$packageName:id/search_wrapper").click()
        Log.i(TAG, "tapOutsideToDismissSearchBar: Clicked the search wrapper")
        Log.i(TAG, "tapOutsideToDismissSearchBar: Waiting for $waitingTime ms for the edit mode toolbar to be gone")
        browserToolbarEditView().waitUntilGone(waitingTime)
        Log.i(TAG, "tapOutsideToDismissSearchBar: Waited for $waitingTime ms for the edit mode toolbar to be gone")
    }

    fun longClickToolbar() {
        Log.i(TAG, "longClickToolbar: Waiting for $waitingTime ms for $packageName window to be updated")
        mDevice.waitForWindowUpdate(packageName, waitingTime)
        Log.i(TAG, "longClickToolbar: Waited for $waitingTime ms for $packageName window to be updated")
        Log.i(TAG, "longClickToolbar: Waiting for $waitingTime ms for the awesome bar to exist")
        mDevice.findObject(UiSelector().resourceId("$packageName:id/awesomeBar"))
            .waitForExists(waitingTime)
        Log.i(TAG, "longClickToolbar: Waited for $waitingTime ms for the awesome bar to exist")
        Log.i(TAG, "longClickToolbar: Waiting for $waitingTime ms for the toolbar to exist")
        mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
            .waitForExists(waitingTime)
        Log.i(TAG, "longClickToolbar: Waited for $waitingTime ms for the toolbar to exist")
        Log.i(TAG, "longClickToolbar: Trying to perform long click on the toolbar")
        mDevice.findObject(By.res("$packageName:id/toolbar")).click(LONG_CLICK_DURATION)
        Log.i(TAG, "longClickToolbar: Performed long click on the toolbar")
    }

    fun clickPasteText() {
        Log.i(TAG, "clickPasteText: Waiting for $waitingTime ms for the \"Paste\" option to exist")
        mDevice.findObject(UiSelector().textContains("Paste")).waitForExists(waitingTime)
        Log.i(TAG, "clickPasteText: Waited for $waitingTime ms for the \"Paste\" option to exist")
        Log.i(TAG, "clickPasteText: Trying to click the \"Paste\" button")
        mDevice.findObject(By.textContains("Paste")).click()
        Log.i(TAG, "clickPasteText: Clicked the \"Paste\" button")
    }

    fun verifyTranslatedFocusedNavigationToolbar(toolbarHintString: String) =
        assertItemTextContains(browserToolbarEditView(), itemText = toolbarHintString)

    fun verifyTypedToolbarText(expectedText: String) {
        Log.i(TAG, "verifyTypedToolbarText: Waiting for $waitingTime ms for the toolbar to exist")
        mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
            .waitForExists(waitingTime)
        Log.i(TAG, "verifyTypedToolbarText: Waited for $waitingTime ms for the toolbar to exist")
        Log.i(TAG, "verifyTypedToolbarText: Waiting for $waitingTime ms for the edit mode toolbar to exist")
        browserToolbarEditView().waitForExists(waitingTime)
        Log.i(TAG, "verifyTypedToolbarText: Waited for $waitingTime ms for the edit mode toolbar to exist")
        Log.i(TAG, "verifyTypedToolbarText: Trying to verify that $expectedText is visible in the toolbar")
        onView(
            allOf(
                withText(expectedText),
                withId(R.id.mozac_browser_toolbar_edit_url_view),
            ),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyTypedToolbarText: Verified that $expectedText is visible in the toolbar")
    }

    fun verifySearchBarPosition(bottomPosition: Boolean) {
        Log.i(TAG, "verifySearchBarPosition: Trying to verify that the search bar is set to bottom: $bottomPosition")
        onView(withId(R.id.toolbar))
            .check(
                if (bottomPosition) {
                    PositionAssertions.isCompletelyBelow(withId(R.id.keyboard_divider))
                } else {
                    PositionAssertions.isCompletelyAbove(withId(R.id.keyboard_divider))
                },
            )
        Log.i(TAG, "verifySearchBarPosition: Verified that the search bar is set to bottom: $bottomPosition")
    }

    fun deleteSearchKeywordCharacters(numberOfDeletionSteps: Int) {
        for (i in 1..numberOfDeletionSteps) {
            Log.i(TAG, "deleteSearchKeywordCharacters: Trying to click keyboard delete button $i times")
            mDevice.pressDelete()
            Log.i(TAG, "deleteSearchKeywordCharacters: Clicked keyboard delete button $i times")
            Log.i(TAG, "deleteSearchKeywordCharacters: Waiting for $waitingTimeShort ms for $appName window to be updated")
            mDevice.waitForWindowUpdate(appName, waitingTimeShort)
            Log.i(TAG, "deleteSearchKeywordCharacters: Waited for $waitingTimeShort ms for $appName window to be updated")
        }
    }

    class Transition {
        private lateinit var sessionLoadedIdlingResource: SessionLoadedIdlingResource

        fun dismissSearchBar(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            try {
                Log.i(TAG, "dismissSearchBar: Waiting for $waitingTime ms for the search wrapper to exist")
                searchWrapper().waitForExists(waitingTime)
                Log.i(TAG, "dismissSearchBar: Waited for $waitingTime ms for the search wrapper to exist")
                Log.i(TAG, "dismissSearchBar: Trying to click device back button")
                mDevice.pressBack()
                Log.i(TAG, "dismissSearchBar: Clicked device back button")
                assertUIObjectIsGone(searchWrapper())
            } catch (e: AssertionError) {
                Log.i(TAG, "dismissSearchBar: AssertionError caught, executing fallback methods")
                Log.i(TAG, "dismissSearchBar: Trying to click device back button")
                mDevice.pressBack()
                Log.i(TAG, "dismissSearchBar: Clicked device back button")
                assertUIObjectIsGone(searchWrapper())
            }

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openBrowser: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "openBrowser: Waited for device to be idle")
            Log.i(TAG, "openBrowser: Trying to set the edit mode toolbar text to: mozilla")
            browserToolbarEditView().setText("mozilla\n")
            Log.i(TAG, "openBrowser: Edit mode toolbar text was set to: mozilla")
            Log.i(TAG, "openBrowser: Trying to click device enter button")
            mDevice.pressEnter()
            Log.i(TAG, "openBrowser: Clicked device enter button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun submitQuery(query: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            sessionLoadedIdlingResource = SessionLoadedIdlingResource()
            Log.i(TAG, "submitQuery: Waiting for $waitingTime ms for the search wrapper to exist")
            searchWrapper().waitForExists(waitingTime)
            Log.i(TAG, "submitQuery: Waited for $waitingTime ms for the search wrapper to exist")
            Log.i(TAG, "submitQuery: Trying to set the edit mode toolbar text to: $query")
            browserToolbarEditView().setText(query)
            Log.i(TAG, "submitQuery: Edit mode toolbar text was set to: $query")
            Log.i(TAG, "submitQuery: Trying to click device enter button")
            mDevice.pressEnter()
            Log.i(TAG, "submitQuery: Clicked device enter button")

            runWithIdleRes(sessionLoadedIdlingResource) {
                assertUIObjectExists(itemWithResId("$packageName:id/browserLayout"))
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSearchEngineSettings(interact: SettingsSubMenuSearchRobot.() -> Unit): SettingsSubMenuSearchRobot.Transition {
            Log.i(TAG, "clickSearchEngineSettings: Trying to click the \"Search settings\" button")
            searchShortcutList().getChild(UiSelector().text("Search settings")).click()
            Log.i(TAG, "clickSearchEngineSettings: Clicked the \"Search settings\" button")

            SettingsSubMenuSearchRobot().interact()
            return SettingsSubMenuSearchRobot.Transition()
        }

        fun clickSearchSuggestion(searchSuggestion: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.findObject(UiSelector().textContains(searchSuggestion)).also {
                Log.i(TAG, "clickSearchSuggestion: Waiting for $waitingTime ms for search suggestion: $searchSuggestion to exist")
                it.waitForExists(waitingTime)
                Log.i(TAG, "clickSearchSuggestion: Waited for $waitingTime ms for search suggestion: $searchSuggestion to exist")
                Log.i(TAG, "clickSearchSuggestion: Trying to click search suggestion: $searchSuggestion and wait for $waitingTimeShort ms for a new window")
                it.clickAndWaitForNewWindow(waitingTimeShort)
                Log.i(TAG, "clickSearchSuggestion: Clicked search suggestion: $searchSuggestion and waited for $waitingTimeShort ms for a new window")
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun searchScreen(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
    SearchRobot().interact()
    return SearchRobot.Transition()
}

private fun browserToolbarEditView() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_edit_url_view"))

private fun dismissPermissionButton() =
    mDevice.findObject(UiSelector().text("DISMISS"))

private fun goToPermissionsSettingsButton() =
    mDevice.findObject(UiSelector().text("GO TO SETTINGS"))

private fun scanButton() = itemWithDescription("Scan")

private fun clearButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_clear_view"))

private fun searchWrapper() = mDevice.findObject(UiSelector().resourceId("$packageName:id/search_wrapper"))

private fun searchSelectorButton() = itemWithResId("$packageName:id/search_selector")

private fun searchShortcutList() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_menu_recyclerView"))

private fun voiceSearchButton() = mDevice.findObject(UiSelector().description("Voice search"))
