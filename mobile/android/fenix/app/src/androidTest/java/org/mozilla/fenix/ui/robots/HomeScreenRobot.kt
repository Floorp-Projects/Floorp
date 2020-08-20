/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.graphics.Bitmap
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.NoMatchingViewException
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.longClick
import androidx.test.espresso.action.ViewActions.swipeLeft
import androidx.test.espresso.action.ViewActions.swipeRight
import androidx.test.espresso.assertion.ViewAssertions.doesNotExist
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions.actionOnItem
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import androidx.test.uiautomator.Until.findObject
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.CoreMatchers.not
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Search
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.helpers.matchers.hasItem
import org.mozilla.fenix.helpers.withBitmapDrawable

/**
 * Implementation of Robot Pattern for the home screen menu.
 */
class HomeScreenRobot {
    fun verifyNavigationToolbar() = assertNavigationToolbar()
    fun verifyHomeScreen() = assertHomeScreen()
    fun verifyHomePrivateBrowsingButton() = assertHomePrivateBrowsingButton()
    fun verifyHomeMenu() = assertHomeMenu()
    fun verifyTabButton() = assertTabButton()
    fun verifyCollectionsHeader() = assertCollectionsHeader()
    fun verifyNoCollectionsHeader() = assertNoCollectionsHeader()
    fun verifyNoCollectionsText() = assertNoCollectionsText()
    fun verifyHomeWordmark() = assertHomeWordmark()
    fun verifyHomeToolbar() = assertHomeToolbar()
    fun verifyHomeComponent() = assertHomeComponent()
    fun verifyDefaultSearchEngine(searchEngine: String) = verifySearchEngineIcon(searchEngine)
    fun verifyNoTabsOpened() = assertNoTabsOpened()

    // First Run elements
    fun verifyWelcomeHeader() = assertWelcomeHeader()

    fun verifyGetTheMostHeader() = assertGetTheMostHeader()
    fun verifyAccountsSignInButton() = assertAccountsSignInButton()
    fun verifyGetToKnowHeader() = assertGetToKnowHeader()
    fun verifyChooseThemeHeader() = assertChooseThemeHeader()
    fun verifyChooseThemeText() = assertChooseThemeText()
    fun verifyLightThemeToggle() = assertLightThemeToggle()
    fun verifyLightThemeDescription() = assertLightThemeDescription()
    fun verifyDarkThemeToggle() = assertDarkThemeToggle()
    fun verifyDarkThemeDescription() = assertDarkThemeDescription()
    fun verifyAutomaticThemeToggle() = assertAutomaticThemeToggle()
    fun verifyAutomaticThemeDescription() = assertAutomaticThemeDescription()
    fun verifyAutomaticPrivacyfHeader() = assertAutomaticPrivacyHeader()
    fun verifyTrackingProtectionToggle() = assertTrackingProtectionToggle()
    fun verifyAutomaticPrivacyText() = assertAutomaticPrivacyText()

    // What's new elements
    fun verifyWhatsNewHeader() = assertWhatsNewHeather()
    fun verifyWhatsNewLink() = assertWhatsNewLink()

    // Browse privately
    fun verifyBrowsePrivatelyHeader() = assertBrowsePrivatelyHeader()
    fun verifyBrowsePrivatelyText() = assertBrowsePrivatelyText()

    // Take a position
    fun verifyTakePositionHeader() = assertTakePositionheader()
    fun verifyTakePositionElements() {
        assertTakePositionBottomRadioButton()
        assertTakePositionTopRadioButton()
    }

    // Your privacy
    fun verifyYourPrivacyHeader() = assertYourPrivacyHeader()
    fun verifyYourPrivacyText() = assertYourPrivacyText()
    fun verifyPrivacyNoticeButton() = assertPrivacyNoticeButton()
    fun verifyStartBrowsingButton() = assertStartBrowsingButton()

    fun verifyPrivateSessionMessage() = assertPrivateSessionMessage()

    fun verifyExistingTopSitesList() = assertExistingTopSitesList()
    fun verifyNotExistingTopSitesList(title: String) = assertNotExistingTopSitesList(title)
    fun verifyExistingTopSitesTabs(title: String) = assertExistingTopSitesTabs(title)
    fun verifyTopSiteContextMenuItems() = assertTopSiteContextMenuItems()

    // Collections element
    fun clickCollectionThreeDotButton() {
        collectionThreeDotButton().click()
        mDevice.waitNotNull(findObject(text("Delete collection")), waitingTime)
    }

    fun selectOpenTabs() {
        onView(allOf(withText("Open tabs"))).click()
    }

    fun selectRenameCollection() {
        onView(allOf(withText("Rename collection"))).click()
        mDevice.waitNotNull(findObject(text("Rename collection")))
    }

    fun selectAddTabToCollection() {
        onView(allOf(withText("Add tab"))).click()
        mDevice.waitNotNull(findObject(text("Select Tabs")))
    }

    fun selectDeleteCollection() {
        onView(allOf(withText("Delete collection"))).click()
        mDevice.waitNotNull(findObject(By.res("android:id/message")), waitingTime)
    }

    fun confirmDeleteCollection() {
        onView(allOf(withText("DELETE"))).click()
        mDevice.waitNotNull(findObject(By.res("org.mozilla.fenix.debug:id/collections_header")), waitingTime)
    }

    fun typeCollectionName(name: String) {
        mDevice.wait(findObject(By.res("org.mozilla.fenix.debug:id/name_collection_edittext")), waitingTime)
        collectionNameTextField().perform(ViewActions.replaceText(name))
        collectionNameTextField().perform(ViewActions.pressImeActionButton())
        mDevice.waitNotNull(Until.gone(text("Name collection")))
    }

    fun saveTabsSelectedForCollection() = onView(withId(R.id.save_button)).click()

    fun verifyCollectionIsDisplayed(title: String) {
        mDevice.wait(findObject(text(title)), waitingTime)
        collectionTitle(title).check(matches(isDisplayed()))
    }

    fun verifyCollectionIcon() =
        onView(withId(R.id.collection_icon)).check(matches(isDisplayed()))

    fun expandCollection(title: String) {
        try {
            mDevice.waitNotNull(findObject(text(title)), waitingTime)
            collectionTitle(title).click()
        } catch (e: NoMatchingViewException) {
            scrollToElementByText(title)
        }
    }

    fun collapseCollection(title: String) {
        try {
            mDevice.waitNotNull(findObject(text(title)), waitingTime)
            onView(allOf(withId(R.id.chevron), hasSibling(withText(title)))).click()
        } catch (e: NoMatchingViewException) {
            scrollToElementByText(title)
        }
    }

    fun clickSaveCollectionButton() = saveCollectionButton().click()

    fun verifyItemInCollectionExists(title: String, visible: Boolean = true) {
        try {
            collectionItem(title)
                .check(
                    if (visible) matches(isDisplayed()) else doesNotExist()
                )
        } catch (e: NoMatchingViewException) {
            scrollToElementByText(title)
        }
    }

    fun verifyCollectionItemLogo() =
        onView(withId(R.id.list_item_favicon)).check(matches(isDisplayed()))

    fun verifyCollectionItemUrl() =
        onView(withId(R.id.list_item_url)).check(matches(isDisplayed()))

    fun verifyShareCollectionButtonIsVisible(visible: Boolean) {
        shareCollectionButton()
            .check(
                if (visible) matches(withEffectiveVisibility(Visibility.VISIBLE))
                else matches(withEffectiveVisibility(Visibility.GONE))
            )
    }

    fun verifyCollectionMenuIsVisible(visible: Boolean) {
        collectionThreeDotButton()
            .check(
                if (visible) matches(withEffectiveVisibility(Visibility.VISIBLE))
                else matches(withEffectiveVisibility(Visibility.GONE))
            )
    }

    fun verifyCollectionItemRemoveButtonIsVisible(title: String, visible: Boolean) {
        removeTabFromCollectionButton(title)
            .check(
                if (visible) matches(withEffectiveVisibility(Visibility.VISIBLE))
                else doesNotExist()
            )
    }

    fun verifySelectTabsView(vararg tabTitles: String) {
        onView(allOf(withId(R.id.back_button), withText("Select Tabs")))
            .check(matches(isDisplayed()))

        for (title in tabTitles)
            onView(withId(R.id.tab_list)).check(matches(hasItem(withText(title))))
    }

    fun verifyTabsSelectedCounterText(tabsSelected: Int) {
        when (tabsSelected) {
            0 -> onView(withId(R.id.bottom_bar_text)).check(matches(withText("Select tabs to save")))
            1 -> onView(withId(R.id.bottom_bar_text)).check(matches(withText("1 tab selected")))
            else -> onView(withId(R.id.bottom_bar_text)).check(matches(withText("$tabsSelected tabs selected")))
        }
    }

    fun selectAllTabsForCollection() {
        onView(withId(R.id.select_all_button))
            .check(matches(withText("Select All")))
            .click()
    }

    fun deselectAllTabsForCollection() {
        onView(withId(R.id.select_all_button))
            .check(matches(withText("Deselect All")))
            .click()
    }

    fun selectTabForCollection(title: String) {
        tab(title).click()
    }

    fun clickAddNewCollection() =
        onView(allOf(withText("Add new collection"))).click()

    fun verifyNameCollectionView() {
        onView(allOf(withId(R.id.back_button), withText("Name collection")))
            .check(matches(isDisplayed()))
    }

    fun verifyDefaultCollectionName(name: String) =
        onView(withId(R.id.name_collection_edittext)).check(matches(withText(name)))

    fun verifySelectCollectionView() {
        onView(allOf(withId(R.id.back_button), withText("Select collection")))
            .check(matches(isDisplayed()))
    }

    fun verifyShareTabsOverlay() = assertShareTabsOverlay()

    fun clickShareCollectionButton() = onView(withId(R.id.collection_share_button)).click()

    fun removeTabFromCollection(title: String) = removeTabFromCollectionButton(title).click()

    fun swipeCollectionItemRight(title: String) {
        try {
            collectionItem(title).perform(swipeRight())
        } catch (e: NoMatchingViewException) {
            scrollToElementByText(title)
        }
    }

    fun swipeCollectionItemLeft(title: String) {
        try {
            collectionItem(title).perform(swipeLeft())
        } catch (e: NoMatchingViewException) {
            scrollToElementByText(title)
        }
    }

    fun longTapSelectTab(title: String) {
        tab(title).perform(longClick())
    }

    fun goBackCollectionFlow() = collectionFlowBackButton().click()

    fun scrollToElementByText(text: String): UiScrollable {
        val appView = UiScrollable(UiSelector().scrollable(true))
        appView.scrollTextIntoView(text)
        return appView
    }

    fun closeTab() {
        closeTabButton().click()
    }

    fun togglePrivateBrowsingModeOnOff() {
        onView(ViewMatchers.withResourceName("privateBrowsingButton"))
            .perform(click())
    }

    fun swipeToBottom() = onView(withId(R.id.homeLayout)).perform(ViewActions.swipeUp())

    fun swipeToTop() =
        onView(withId(R.id.sessionControlRecyclerView)).perform(ViewActions.swipeDown())

    fun swipeTabRight(title: String) =
        tab(title).perform(ViewActions.swipeRight())

    fun swipeTabLeft(title: String) =
        tab(title).perform(ViewActions.swipeLeft())

    fun verifySnackBarText(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(findObject(By.text(expectedText)), TestAssetHelper.waitingTime)
    }

    fun snackBarButtonClick(expectedText: String) {
        onView(allOf(withId(R.id.snackbar_btn), withText(expectedText))).check(
            matches(withEffectiveVisibility(Visibility.VISIBLE))
        ).perform(click())
    }

    fun verifyTabMediaControlButtonState(action: String) {
        mDevice.waitNotNull(
            findObject(
                By
                    .res("org.mozilla.fenix.debug:id/play_pause_button")
                    .desc(action)
            ),
            waitingTime
        )

        tabMediaControlButton().check(matches(withContentDescription(action)))
    }

    fun clickTabMediaControlButton() = tabMediaControlButton().click()

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun openTabDrawer(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            mDevice.waitForIdle()

            tabsCounter().click()

            mDevice.waitNotNull(
                Until.findObject(By.res("org.mozilla.fenix.debug:id/tab_layout")),
                waitingTime
            )

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            threeDotButton().perform(click())

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openSearch(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            navigationToolbar().perform(click())

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun dismissOnboarding() {
            openThreeDotMenu { }.openSettings { }.goBack { }
        }

        fun togglePrivateBrowsingMode() {
            onView(ViewMatchers.withResourceName("privateBrowsingButton"))
                .perform(click())
        }

        fun openTabsListThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
//            tabsListThreeDotButton().perform(click())

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun closeAllPrivateTabs(interact: HomeScreenRobot.() -> Unit): Transition {
            onView(withId(R.id.close_tabs_button))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
                .perform(click())

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openNavigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {

            assertNavigationToolbar().perform(click())
            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openContextMenuOnTopSitesWithTitle(
            title: String,
            interact: HomeScreenRobot.() -> Unit
        ): Transition {
            onView(withId(R.id.top_sites_list)).perform(
                actionOnItem<RecyclerView.ViewHolder>(
                    hasDescendant(withText(title)),
                    ViewActions.longClick()
                )
            )

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openTopSiteTabWithTitle(
            title: String,
            interact: BrowserRobot.() -> Unit
        ): BrowserRobot.Transition {
            onView(withId(R.id.top_sites_list)).perform(
                actionOnItem<RecyclerView.ViewHolder>(hasDescendant(withText(title)), click())
            )

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun removeTopSite(interact: HomeScreenRobot.() -> Unit): Transition {
            onView(withText("Remove"))
                .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
                .perform(click())

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openTopSiteInPrivateTab(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withText("Open in private tab"))
                .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openCommonMythsLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withId(R.id.private_session_common_myths))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTab(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(findObject(text(title)))
            tab(title).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun homeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
    HomeScreenRobot().interact()
    return HomeScreenRobot.Transition()
}

val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
val appContext = InstrumentationRegistry.getInstrumentation().targetContext

private fun navigationToolbar() =
    onView(allOf(withText("Search or enter address")))

private fun closeTabButton() = onView(withId(R.id.close_tab_button))

private fun assertNavigationToolbar() =
    onView(allOf(withText("Search or enter address")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertHomeScreen() = onView(ViewMatchers.withResourceName("homeLayout"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertHomeMenu() = onView(ViewMatchers.withResourceName("menuButton"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertHomePrivateBrowsingButton() =
    onView(ViewMatchers.withResourceName("privateBrowsingButton"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertHomeWordmark() = onView(ViewMatchers.withResourceName("wordmark"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertHomeToolbar() = onView(ViewMatchers.withResourceName("toolbar"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertTabButton() =
    onView(allOf(withId(R.id.tab_button), isDisplayed()))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertCollectionsHeader() =
    onView(allOf(withText("Collections")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNoCollectionsHeader() =
    onView(allOf(withText("Collect the things that matter to you")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNoCollectionsText() =
    onView(
        allOf(
            withText("Group together similar searches, sites, and tabs for quick access later.")
        )
    )
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertHomeComponent() =
    onView(ViewMatchers.withResourceName("sessionControlRecyclerView"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNoTabsOpened() = onView(withId(R.id.counter_text)).check(matches(withText("0")))

private fun threeDotButton() = onView(allOf(withId(R.id.menuButton)))

private fun verifySearchEngineIcon(searchEngineIcon: Bitmap, searchEngineName: String) {
    onView(withId(R.id.search_engine_icon))
        .check(matches(withBitmapDrawable(searchEngineIcon, searchEngineName)))
}

private fun getSearchEngine(searchEngineName: String) =
    Search(appContext).searchEngineManager.getDefaultSearchEngine(appContext, searchEngineName)

private fun verifySearchEngineIcon(searchEngineName: String) {
    val ddgSearchEngine = getSearchEngine(searchEngineName)
    verifySearchEngineIcon(ddgSearchEngine.icon, ddgSearchEngine.name)
}

// First Run elements
private fun assertWelcomeHeader() =
    onView(allOf(withText("Welcome to Firefox Preview!")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertGetTheMostHeader() =
    onView(allOf(withText("Start syncing bookmarks, passwords, and more with your Firefox account.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAccountsSignInButton() =
    onView(ViewMatchers.withResourceName("fxa_sign_in_button"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertGetToKnowHeader() =
    onView(allOf(withText("Get to know Firefox Preview")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertChooseThemeHeader() =
    onView(allOf(withText("Choose your theme")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertChooseThemeText() =
    onView(allOf(withText("Save some battery and your eyesight by enabling dark mode.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertLightThemeToggle() =
    onView(ViewMatchers.withResourceName("theme_light_radio_button"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertLightThemeDescription() =
    onView(allOf(withText("Light theme")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertDarkThemeToggle() =
    onView(ViewMatchers.withResourceName("theme_dark_radio_button"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertDarkThemeDescription() =
    onView(allOf(withText("Dark theme")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAutomaticThemeToggle() =
    onView(withId(R.id.theme_automatic_radio_button))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAutomaticThemeDescription() =
    onView(allOf(withText("Automatic")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAutomaticPrivacyHeader() =
    onView(allOf(withText("Automatic privacy")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertTrackingProtectionToggle() = onView(
    allOf(ViewMatchers.withResourceName("tracking_protection_toggle"))
)
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAutomaticPrivacyText() {
    onView(
        allOf(
            withText(
                "Privacy and security settings block trackers, malware, and companies that follow you."
            )
        )
    )
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertBrowsePrivatelyHeader() =
    onView(allOf(withText("Browse privately")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertBrowsePrivatelyText() =
    onView(allOf(withText(containsString("Update your private browsing settings."))))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertYourPrivacyHeader() =
    onView(allOf(withText("Your privacy")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertYourPrivacyText() =
    onView(
        allOf(
            withText(
                "We’ve designed Firefox Preview to give you control over what you share online and what you share with us."
            )
        )
    )
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertPrivacyNoticeButton() =
    onView(allOf(withText("Read our privacy notice")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

// What's new elements
private fun assertWhatsNewHeather() = onView(allOf(withText("See what’s new")))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertWhatsNewLink() = onView(allOf(withText("Get answers here")))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertStartBrowsingButton() =
    onView(allOf(withText("Start browsing")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

// Take a position
private fun assertTakePositionheader() = onView(allOf(withText("Take a position")))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertTakePositionTopRadioButton() =
    onView(ViewMatchers.withResourceName("toolbar_top_radio_button"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertTakePositionBottomRadioButton() =
    onView(ViewMatchers.withResourceName("toolbar_bottom_radio_button"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

const val PRIVATE_SESSION_MESSAGE =
    "Firefox Preview clears your search and browsing history from private tabs when you close them" +
            " or quit the app. While this doesn’t make you anonymous to websites or your internet" +
            " service provider, it makes it easier to keep what you do online private from anyone" +
            " else who uses this device."

private fun assertPrivateSessionMessage() =
    onView(withId(R.id.private_session_description))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun collectionThreeDotButton() =
    onView(allOf(withId(R.id.collection_overflow_button)))

private fun collectionNameTextField() =
    onView(allOf(ViewMatchers.withResourceName("name_collection_edittext")))

private fun collectionTitle(title: String) =
    onView(allOf(withId(R.id.collection_title), withText(title)))

private fun assertExistingTopSitesList() =
    onView(allOf(withId(R.id.top_sites_list)))
        .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))

private fun assertExistingTopSitesTabs(title: String) =
    onView(allOf(withId(R.id.top_sites_list)))
        .check(matches(hasItem(hasDescendant(withText(title)))))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNotExistingTopSitesList(title: String) =
    onView(allOf(withId(R.id.top_sites_list)))
        .check(matches(not(hasItem(hasDescendant(withText(title))))))

private fun assertTopSiteContextMenuItems() {
    val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

    mDevice.waitNotNull(
        findObject(By.text("Open in private tab")),
        waitingTime
    )
    mDevice.waitNotNull(
        findObject(By.text("Remove")),
        waitingTime
    )
}

private fun assertShareTabsOverlay() {
    onView(withId(R.id.shared_site_list)).check(matches(isDisplayed()))
    onView(withId(R.id.share_tab_title)).check(matches(isDisplayed()))
    onView(withId(R.id.share_tab_favicon)).check(matches(isDisplayed()))
    onView(withId(R.id.share_tab_url)).check(matches(isDisplayed()))
}

private fun tabMediaControlButton() = onView(withId(R.id.play_pause_button))

private fun collectionItem(title: String) =
    onView(allOf(withId(R.id.list_element_title), withText(title)))

private fun saveCollectionButton() = onView(withId(R.id.save_tab_group_button))

private fun shareCollectionButton() = onView(withId(R.id.collection_share_button))

private fun removeTabFromCollectionButton(title: String) =
    onView(
        allOf(
            withId(R.id.list_item_action_button),
            hasSibling(withText(title))
        )
    )

private fun collectionFlowBackButton() = onView(withId(R.id.back_button))
private fun tabsCounter() = onView(withId(R.id.tab_button))

private fun tab(title: String) =
    onView(
        allOf(
            withId(R.id.tab_title),
            withText(title)
        )
    )
