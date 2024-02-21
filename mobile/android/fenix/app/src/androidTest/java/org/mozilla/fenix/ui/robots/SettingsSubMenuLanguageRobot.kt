/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

class SettingsSubMenuLanguageRobot {
    fun selectLanguage(language: String) {
        Log.i(TAG, "selectLanguage: Waiting for $waitingTime ms for language list to exist")
        languagesList().waitForExists(waitingTime)
        Log.i(TAG, "selectLanguage: Waited for $waitingTime ms for language list to exist")
        Log.i(TAG, "selectLanguage: Trying to click language: $language")
        languagesList()
            .getChildByText(UiSelector().text(language), language)
            .click()
        Log.i(TAG, "selectLanguage: Clicked language: $language")
    }

    fun selectLanguageSearchResult(languageName: String) {
        Log.i(TAG, "selectLanguageSearchResult: Waiting for $waitingTime ms for language list to exist")
        language(languageName).waitForExists(waitingTime)
        Log.i(TAG, "selectLanguageSearchResult: Waited for $waitingTime ms for language list to exist")
        Log.i(TAG, "selectLanguageSearchResult: Trying to click language: $languageName")
        language(languageName).click()
        Log.i(TAG, "selectLanguageSearchResult: Clicked language: $languageName")
    }

    fun verifyLanguageHeaderIsTranslated(translation: String) = assertUIObjectExists(itemWithText(translation))

    fun verifySelectedLanguage(language: String) {
        Log.i(TAG, "verifySelectedLanguage: Waiting for $waitingTime ms for language list to exist")
        languagesList().waitForExists(waitingTime)
        Log.i(TAG, "verifySelectedLanguage: Waited for $waitingTime ms for language list to exist")
        assertUIObjectExists(
            languagesList()
                .getChildByText(UiSelector().text(language), language, true)
                .getFromParent(UiSelector().resourceId("$packageName:id/locale_selected_icon")),
        )
    }

    fun openSearchBar() {
        Log.i(TAG, "openSearchBar: Trying to click the search bar")
        onView(withId(R.id.search)).click()
        Log.i(TAG, "openSearchBar: Clicked the search bar")
    }

    fun typeInSearchBar(text: String) {
        Log.i(TAG, "typeInSearchBar: Waiting for $waitingTime ms for search bar to exist")
        searchBar().waitForExists(waitingTime)
        Log.i(TAG, "typeInSearchBar: Waited for $waitingTime ms for search bar to exist")
        Log.i(TAG, "typeInSearchBar: Trying to set search bar text to: $text")
        searchBar().text = text
        Log.i(TAG, "typeInSearchBar: Search bar text was set to: $text")
    }

    fun verifySearchResultsContains(languageName: String) =
        assertUIObjectExists(language(languageName))

    fun clearSearchBar() {
        Log.i(TAG, "clearSearchBar: Trying to click the clear search bar button")
        onView(withId(R.id.search_close_btn)).click()
        Log.i(TAG, "clearSearchBar: Clicked the clear search bar button")
    }

    fun verifyLanguageListIsDisplayed() = assertUIObjectExists(languagesList())

    class Transition {

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "goBack: Waited for device to be idle")
            Log.i(TAG, "goBack: Trying to click the navigate up button")
            goBackButton().perform(ViewActions.click())
            Log.i(TAG, "goBack: Clicked the navigate up button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun goBackButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun languagesList() =
    UiScrollable(
        UiSelector()
            .resourceId("$packageName:id/locale_list")
            .scrollable(true),
    )

private fun language(name: String) = mDevice.findObject(UiSelector().text(name))

private fun searchBar() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/search_src_text"))
