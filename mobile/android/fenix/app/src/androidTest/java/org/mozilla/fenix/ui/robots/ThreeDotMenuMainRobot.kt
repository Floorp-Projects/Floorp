/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasFocus
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withResourceName
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.share.ShareFragment

/**
 * Implementation of Robot Pattern for the three dot (main) menu.
 */
class ThreeDotMenuMainRobot {
    fun verifySettingsButton() = assertSettingsButton()
    fun verifyLibraryButton() = assertLibraryButton()
    fun verifyHelpButton() = assertHelpButton()
    fun verifyThreeDotMenuExists() = threeDotMenuRecyclerViewExists()
    fun verifyForwardButton() = assertForwardButton()
    fun verifyBackButton() = assertBackButton()
    fun verifyRefreshButton() = assertRefreshButton()
    fun verifyCloseAllTabsButton() = assertCloseAllTabsButton()
    fun verifyShareButton() = assertShareButton()
    fun clickShareButton() {
        shareButton().click()
        mDevice.waitNotNull(Until.findObject(By.text("SHARE A LINK")), waitingTime)
    }

    fun verifyShareTabButton() = assertShareTabButton()
    fun verifySaveCollection() = assertSaveCollectionButton()
    fun clickBrowserViewSaveCollectionButton() {
        browserViewSaveCollectionButton().click()
    }
    fun clickAddNewCollection() {
        addNewCollectionButton().click()
    }
    fun verifyCollectionNameTextField() = assertCollectionNameTextField()
    fun verifyFindInPageButton() = assertFindInPageButton()
    fun verifyShareScrim() = assertShareScrim()
    fun verifySendToDeviceTitle() = assertSendToDeviceTitle()
    fun verifyShareALinkTitle() = assertShareALinkTitle()
    fun verifyWhatsNewButton() = assertWhatsNewButton()

    class Transition {

        private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun openSettings(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Settings")), waitingTime)
            settingsButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openLibrary(interact: LibraryRobot.() -> Unit): LibraryRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Your Library")), waitingTime)
            libraryButton().click()

            LibraryRobot().interact()
            return LibraryRobot.Transition()
        }

        fun openHelp(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Help")), waitingTime)
            helpButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goForward(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.desc("Forward")), waitingTime)
            forwardButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goBack(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.desc("Back")), waitingTime)
            backButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun refreshPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.desc("Refresh")), waitingTime)
            refreshButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeAllTabs(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Close all tabs")), waitingTime)
            closeAllTabsButton().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openFindInPage(interact: FindInPageRobot.() -> Unit): FindInPageRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Find in page")), waitingTime)
            findInPageButton().click()

            FindInPageRobot().interact()
            return FindInPageRobot.Transition()
        }

        fun openWhatsNew(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("What's New")), waitingTime)
            whatsNewButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun typeCollectionName(name: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.wait(Until.findObject(By.res("org.mozilla.fenix.debug:id/name_collection_edittext")), waitingTime)

            collectionNameTextField().check(matches(hasFocus()))
            collectionNameTextField().perform(ViewActions.replaceText(name), ViewActions.pressImeActionButton())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private fun threeDotMenuRecyclerViewExists() {
    onView(withId(R.id.mozac_browser_menu_recyclerView)).check(matches(isDisplayed()))
}

private fun settingsButton() = onView(allOf(withText(R.string.settings)))
private fun assertSettingsButton() = settingsButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun libraryButton() = onView(allOf(withText(R.string.browser_menu_your_library)))
private fun assertLibraryButton() = libraryButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun helpButton() = onView(allOf(withText(R.string.browser_menu_help)))
private fun assertHelpButton() = helpButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun forwardButton() = onView(ViewMatchers.withContentDescription("Forward"))
private fun assertForwardButton() = forwardButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun backButton() = onView(ViewMatchers.withContentDescription("Back"))
private fun assertBackButton() = backButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun refreshButton() = onView(ViewMatchers.withContentDescription("Refresh"))
private fun assertRefreshButton() = refreshButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun closeAllTabsButton() = onView(allOf(withText("Close all tabs")))
private fun assertCloseAllTabsButton() = closeAllTabsButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun shareTabButton() = onView(allOf(withText("Share tabs")))
private fun assertShareTabButton() = shareTabButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun shareButton() = onView(allOf(withText("Share")))
private fun assertShareButton() = shareButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun browserViewSaveCollectionButton() = onView(allOf(withText("Save to Collection"),
    withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun saveCollectionButton() = onView(allOf(withText("Save to collection")))
private fun assertSaveCollectionButton() = saveCollectionButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun addNewCollectionButton() = onView(allOf(withText("Add new collection")))
private fun assertaddNewCollectionButton() = addNewCollectionButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun collectionNameTextField() = onView(allOf(withResourceName("name_collection_edittext")))
private fun assertCollectionNameTextField() = collectionNameTextField()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun findInPageButton() = onView(allOf(withText("Find in page")))
private fun assertFindInPageButton() = findInPageButton()

private fun shareScrim() = onView(withResourceName("closeSharingScrim"))
private fun assertShareScrim() =
    shareScrim().check(matches(ViewMatchers.withAlpha(ShareFragment.SHOW_PAGE_ALPHA)))

private fun SendToDeviceTitle() =
    onView(allOf(withText("SEND TO DEVICE"), withResourceName("accountHeaderText")))

private fun assertSendToDeviceTitle() = SendToDeviceTitle()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun ShareALinkTitle() =
    onView(allOf(withText("SHARE A LINK"), withResourceName("link_header")))

private fun assertShareALinkTitle() = ShareALinkTitle()

private fun whatsNewButton() = onView(allOf(withText("What's New")))
private fun assertWhatsNewButton() = whatsNewButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
