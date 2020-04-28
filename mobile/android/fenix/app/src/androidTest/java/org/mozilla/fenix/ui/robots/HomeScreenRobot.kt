/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.graphics.Bitmap
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.doesNotExist
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.contrib.RecyclerViewActions.actionOnItem
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
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
    fun verifyOpenTabsHeader() = assertOpenTabsHeader()
    fun verifyAddTabButton() = assertAddTabButton()
    fun verifyNoTabsOpenedText() = assertNoTabsOpenedText()
    fun verifyCollectionsHeader() = assertCollectionsHeader()
    fun verifyNoCollectionsHeader() = assertNoCollectionsHeader()
    fun verifyNoCollectionsText() = assertNoCollectionsText()
    fun verifyNoCollectionsHeaderIsNotShown() = assertNoCollectionsHeaderIsNotVisible()
    fun verifyCollectionsHeaderIsNotShown() = assertCollectionsHeaderIsNotVisible()
    fun verifyNoCollectionsTextIsNotShown() = assertNoCollectionsTextIsNotVisible()
    fun verifyNoTabsOpenedHeader() = assertNoTabsOpenedHeader()
    fun verifyHomeWordmark() = assertHomeWordmark()
    fun verifyHomeToolbar() = assertHomeToolbar()
    fun verifyHomeComponent() = assertHomeComponent()
    fun verifyDefaultSearchEngine(searchEngine: String) = verifySearchEngineIcon(searchEngine)

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
    fun verifyProtectYourselfHeader() = assertProtectYourselfHeader()
    fun verifyTrackingProtectionToggle() = assertTrackingProtectionToggle()
    fun verifyProtectYourselfText() = assertProtectYourselfText()

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

    fun verifyPrivateSessionHeader() = assertPrivateSessionHeader()

    fun verifyPrivateSessionMessage(visible: Boolean = true) = assertPrivateSessionMessage(visible)
    fun verifyPrivateTabsCloseTabsButton() = assertPrivateTabsCloseTabsButton()

    fun verifyShareTabsButton(visible: Boolean = true) = assertShareTabsButton(visible)
    fun verifyCloseTabsButton(title: String) =
        assertCloseTabsButton(title)

    fun verifyExistingTabList() = assertExistingTabList()
    fun verifyExistingOpenTabs(title: String) = assertExistingOpenTabs(title)

    fun verifyExistingTopSitesList() = assertExistingTopSitesList()
    fun verifyNotExistingTopSitesList(title: String) = assertNotExistingTopSitesList(title)
    fun verifyExistingTopSitesTabs(title: String) = assertExistingTopSitesTabs(title)
    fun verifyTopSiteContextMenuItems() = assertTopSiteContextMenuItems()

    // Collections element
    fun clickCollectionThreeDotButton() {
        collectionThreeDotButton().click()
        mDevice.waitNotNull(Until.findObject(By.text("Delete collection")), waitingTime)
    }

    fun selectRenameCollection() {
        onView(allOf(ViewMatchers.withText("Rename collection"))).click()
        mDevice.waitNotNull(Until.findObject(By.res("name_collection_edittext")))
    }

    fun selectDeleteCollection() {
        onView(allOf(ViewMatchers.withText("Delete collection"))).click()
        mDevice.waitNotNull(Until.findObject(By.res("message")), waitingTime)
    }

    fun confirmDeleteCollection() {
        onView(allOf(ViewMatchers.withText("DELETE"))).click()
        mDevice.waitNotNull(Until.findObject(By.res("collections_header")), waitingTime)
    }

    fun typeCollectionName(name: String) {
        mDevice.wait(Until.findObject(By.res("name_collection_edittext")), waitingTime)
        collectionNameTextField().perform(ViewActions.replaceText(name))
        collectionNameTextField().perform(ViewActions.pressImeActionButton())
    }

    fun scrollToElementByText(text: String): UiScrollable {
        val appView = UiScrollable(UiSelector().scrollable(true))
        appView.scrollTextIntoView(text)
        return appView
    }

    fun swipeUpToDismissFirstRun() {
        scrollToElementByText("Start browsing")
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
        onView(allOf(withId(R.id.tab_title), withText(title))).perform(ViewActions.swipeRight())

    fun swipeTabLeft(title: String) =
        onView(allOf(withId(R.id.tab_title), withText(title))).perform(ViewActions.swipeLeft())

    fun closeTabViaXButton(title: String) = closeTabViaX(title)

    fun verifySnackBarText(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.text(expectedText)), TestAssetHelper.waitingTime)
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

        fun addNewTab() {
            openSearch { }.openBrowser { }.openHomeScreen { }
        }

        fun togglePrivateBrowsingMode() {
            onView(ViewMatchers.withResourceName("privateBrowsingButton"))
                .perform(click())
        }

        fun openTabsListThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            tabsListThreeDotButton().perform(click())

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
            onView(
                allOf(
                    withId(R.id.tab_title),
                    withText(title)
                )
            ).click()

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

private fun assertOpenTabsHeader() =
    onView(allOf(withText("Open tabs")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAddTabButton() =
    onView(allOf(withId(R.id.add_tab_button), isDisplayed()))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNoTabsOpenedHeader() =
    onView(allOf(withText("No open tabs")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNoTabsOpenedText() {
    onView(allOf(withText("Your open tabs will be shown here.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertCollectionsHeader() =
    onView(allOf(withText("Collections")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNoCollectionsHeader() =
    onView(allOf(withText("No collections")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNoCollectionsHeaderIsNotVisible() =
    onView(allOf(withText("No collections")))
        .check(doesNotExist())

private fun assertCollectionsHeaderIsNotVisible() =
    onView(allOf(withText("Collections")))
        .check(doesNotExist())

private fun assertNoCollectionsText() =
    onView(
        allOf(
            withText("Collect the things that matter to you. To start, save open tabs to a new collection.")
        )
    )
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNoCollectionsTextIsNotVisible() =
    onView(
        allOf(
            withText("Collect the things that matter to you. To start, save open tabs to a new collection.")
        )
    )
        .check(doesNotExist())

private fun assertHomeComponent() =
    onView(ViewMatchers.withResourceName("sessionControlRecyclerView"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

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
    onView(allOf(withText("Get the most out of Firefox Preview.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAccountsSignInButton() =
    onView(ViewMatchers.withResourceName("turn_on_sync_button"))
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

private fun assertProtectYourselfHeader() =
    onView(allOf(withText("Protect yourself")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertTrackingProtectionToggle() = onView(
    allOf(ViewMatchers.withResourceName("tracking_protection_toggle"))
)
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertProtectYourselfText() {
    onView(
        allOf(
            withText(
                "Firefox Preview helps stop websites from tracking you online."
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

// Private mode elements
private fun assertPrivateSessionHeader() =
    onView(allOf(withText("Private tabs")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

const val PRIVATE_SESSION_MESSAGE =
    "Firefox Preview clears your search and browsing history from private tabs when you close them" +
            " or quit the app. While this doesn’t make you anonymous to websites or your internet" +
            " service provider, it makes it easier to keep what you do online private from anyone" +
            " else who uses this device."

private fun assertPrivateSessionMessage(visible: Boolean) =
    onView(withId(R.id.private_session_description))
        .check(
            if (visible) matches(withEffectiveVisibility(Visibility.VISIBLE)) else doesNotExist()
        )

private fun assertShareTabsButton(visible: Boolean) = onView(allOf(withId(R.id.share_tabs_button)))
    .check(
        if (visible) matches(withEffectiveVisibility(Visibility.VISIBLE)) else matches(
            withEffectiveVisibility(Visibility.INVISIBLE)
        )
    )

private fun assertCloseTabsButton(title: String) =
    onView(allOf(withId(R.id.close_tab_button), withContentDescription("Close tab $title")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun visibleOrGone(visibility: Boolean) =
    if (visibility) Visibility.VISIBLE else Visibility.GONE

private fun assertExistingTabList() =
    onView(allOf(withId(R.id.item_tab)))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertExistingOpenTabs(title: String) =
    onView(withId(R.id.sessionControlRecyclerView)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            ViewMatchers.hasDescendant(withText(title))
        )
    ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun tabsListThreeDotButton() = onView(allOf(withId(R.id.tabs_overflow_button)))

private fun collectionThreeDotButton() =
    onView(allOf(withId(R.id.collection_overflow_button)))

private fun collectionNameTextField() =
    onView(allOf(ViewMatchers.withResourceName("name_collection_edittext")))

private fun closeTabViaX(title: String) {
    val closeButton = onView(
        allOf(
            withId(R.id.close_tab_button),
            withContentDescription("Close tab $title")
        )
    )
    closeButton.perform(click())
}

private fun assertPrivateTabsCloseTabsButton() = onView(allOf(withId(R.id.close_tabs_button)))

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
        Until.findObject(By.text("Open in private tab")),
        waitingTime
    )
    mDevice.waitNotNull(
        Until.findObject(By.text("Remove")),
        waitingTime
    )
}

private fun tabMediaControlButton() = onView(withId(R.id.play_pause_button))
