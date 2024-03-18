/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.net.Uri
import android.os.Build
import android.util.Log
import androidx.compose.ui.test.onNodeWithTag
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.IdlingResource
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.longClick
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.textContains
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers.allOf
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.MatcherHelper.assertItemTextEquals
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.SessionLoadedIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.waitForObjects
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.tabstray.TabsTrayTestTag

/**
 * Implementation of Robot Pattern for the URL toolbar.
 */
class NavigationToolbarRobot {
    fun verifyUrl(url: String) {
        Log.i(TAG, "verifyUrl: Trying to verify toolbar text matches $url")
        onView(withId(R.id.mozac_browser_toolbar_url_view)).check(matches(withText(url)))
        Log.i(TAG, "verifyUrl: Verified toolbar text matches $url")
    }

    fun verifyTabButtonShortcutMenuItems() {
        Log.i(TAG, "verifyTabButtonShortcutMenuItems: Trying to verify tab counter shortcut options")
        onView(withId(R.id.mozac_browser_menu_recyclerView))
            .check(matches(hasDescendant(withText("Close tab"))))
            .check(matches(hasDescendant(withText("New private tab"))))
            .check(matches(hasDescendant(withText("New tab"))))
        Log.i(TAG, "verifyTabButtonShortcutMenuItems: Verified tab counter shortcut options")
    }

    fun verifyReaderViewDetected(visible: Boolean = false) {
        Log.i(TAG, "verifyReaderViewDetected: Waiting for $waitingTime ms for reader view button to exist")
        mDevice.findObject(
            UiSelector()
                .description("Reader view"),
        ).waitForExists(waitingTime)
        Log.i(TAG, "verifyReaderViewDetected: Waited for $waitingTime ms for reader view button to exist")
        Log.i(TAG, "verifyReaderViewDetected: Trying to verify that the reader view button is visible")
        onView(
            allOf(
                withParent(withId(R.id.mozac_browser_toolbar_page_actions)),
                withContentDescription("Reader view"),
            ),
        ).check(
            if (visible) {
                matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))
            } else {
                ViewAssertions.doesNotExist()
            },
        )
        Log.i(TAG, "verifyReaderViewDetected: Verified that the reader view button is visible")
    }

    fun toggleReaderView() {
        Log.i(TAG, "toggleReaderView: Waiting for $waitingTime ms for reader view button to exist")
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/mozac_browser_toolbar_page_actions"),
        )
            .waitForExists(waitingTime)
        Log.i(TAG, "toggleReaderView: Waited for $waitingTime ms for reader view button to exist")
        Log.i(TAG, "toggleReaderView: Trying to click the reader view button")
        readerViewToggle().click()
        Log.i(TAG, "toggleReaderView: Clicked the reader view button")
    }

    fun verifyClipboardSuggestionsAreDisplayed(link: String = "", shouldBeDisplayed: Boolean) =
        assertUIObjectExists(
            itemWithResId("$packageName:id/fill_link_from_clipboard"),
            itemWithResIdAndText(
                "$packageName:id/clipboard_url",
                link,
            ),
            exists = shouldBeDisplayed,
        )

    fun longClickEditModeToolbar() {
        Log.i(TAG, "longClickEditModeToolbar: Trying to long click the edit mode toolbar")
        mDevice.findObject(By.res("$packageName:id/mozac_browser_toolbar_edit_url_view"))
            .click(LONG_CLICK_DURATION)
        Log.i(TAG, "longClickEditModeToolbar: Long clicked the edit mode toolbar")
    }

    fun clickContextMenuItem(item: String) {
        mDevice.waitNotNull(
            Until.findObject(By.text(item)),
            waitingTime,
        )
        Log.i(TAG, "clickContextMenuItem: Trying click context menu item: $item")
        mDevice.findObject(By.text(item)).click()
        Log.i(TAG, "clickContextMenuItem: Clicked context menu item: $item")
    }

    fun clickClearToolbarButton() {
        Log.i(TAG, "clickClearToolbarButton: Trying click the clear address button")
        clearAddressBarButton().click()
        Log.i(TAG, "clickClearToolbarButton: Clicked the clear address button")
    }

    fun verifyToolbarIsEmpty() =
        assertUIObjectExists(
            itemWithResIdContainingText(
                "$packageName:id/mozac_browser_toolbar_edit_url_view",
                getStringResource(R.string.search_hint),
            ),
        )

    // New unified search UI selector
    fun verifySearchBarPlaceholder(text: String) {
        Log.i(TAG, "verifySearchBarPlaceholder: Waiting for $waitingTime ms for the toolbar to exist")
        urlBar().waitForExists(waitingTime)
        Log.i(TAG, "verifySearchBarPlaceholder: Waited for $waitingTime ms for the toolbar to exist")
        assertItemTextEquals(urlBar(), expectedText = text)
    }

    // New unified search UI selector
    fun verifyDefaultSearchEngine(engineName: String) =
        assertUIObjectExists(
            searchSelectorButton().getChild(UiSelector().description(engineName)),
        )

    fun verifyTextSelectionOptions(vararg textSelectionOptions: String) {
        for (textSelectionOption in textSelectionOptions) {
            mDevice.waitNotNull(Until.findObject(textContains(textSelectionOption)), waitingTime)
        }
    }

    class Transition {
        private lateinit var sessionLoadedIdlingResource: SessionLoadedIdlingResource

        fun enterURLAndEnterToBrowser(
            url: Uri,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            sessionLoadedIdlingResource = SessionLoadedIdlingResource()

            openEditURLView()
            Log.i(TAG, "enterURLAndEnterToBrowser: Trying to set toolbar text to: $url")
            awesomeBar().setText(url.toString())
            Log.i(TAG, "enterURLAndEnterToBrowser: Toolbar text was set to: $url")
            Log.i(TAG, "enterURLAndEnterToBrowser: Trying to press device enter button")
            mDevice.pressEnter()
            Log.i(TAG, "enterURLAndEnterToBrowser: Pressed device enter button")

            runWithIdleRes(sessionLoadedIdlingResource) {
                Log.i(TAG, "enterURLAndEnterToBrowser: Trying to assert that home screen layout or download button or the total cookie protection contextual hint exist")
                assertTrue(
                    itemWithResId("$packageName:id/browserLayout").waitForExists(waitingTime) ||
                        itemWithResId("$packageName:id/download_button").waitForExists(waitingTime) ||
                        itemWithResId("cfr.dismiss").waitForExists(waitingTime),
                )
                Log.i(TAG, "enterURLAndEnterToBrowser: Asserted that home screen layout or download button or the total cookie protection contextual hint exist")
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun enterURLAndEnterToBrowserForTCPCFR(
            url: Uri,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            openEditURLView()
            Log.i(TAG, "enterURLAndEnterToBrowserForTCPCFR: Trying to set toolbar text to: $url")
            awesomeBar().setText(url.toString())
            Log.i(TAG, "enterURLAndEnterToBrowserForTCPCFR: Toolbar text was set to: $url")
            Log.i(TAG, "enterURLAndEnterToBrowserForTCPCFR: Trying to press device enter button")
            mDevice.pressEnter()
            Log.i(TAG, "enterURLAndEnterToBrowserForTCPCFR: Pressed device enter button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTabCrashReporter(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            val crashUrl = "about:crashcontent"

            sessionLoadedIdlingResource = SessionLoadedIdlingResource()

            openEditURLView()
            Log.i(TAG, "openTabCrashReporter: Trying to set toolbar text to: $crashUrl")
            awesomeBar().setText(crashUrl)
            Log.i(TAG, "openTabCrashReporter: Toolbar text was set to: $crashUrl")
            Log.i(TAG, "openTabCrashReporter: Trying to press device enter button")
            mDevice.pressEnter()
            Log.i(TAG, "openTabCrashReporter: Pressed device enter button")

            runWithIdleRes(sessionLoadedIdlingResource) {
                Log.i(TAG, "openTabCrashReporter: Trying to find the tab crasher image")
                mDevice.findObject(UiSelector().resourceId("$packageName:id/crash_tab_image"))
                Log.i(TAG, "openTabCrashReporter: Found the tab crasher image")
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.res("$packageName:id/mozac_browser_toolbar_menu")), waitingTime)
            Log.i(TAG, "openThreeDotMenu: Trying to click the main menu button")
            threeDotButton().click()
            Log.i(TAG, "openThreeDotMenu: Clicked the main menu button")

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openTabTray(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "openTabTray: Waiting for device to be idle for $waitingTime ms")
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "openTabTray: Waited for device to be idle for $waitingTime ms")
            Log.i(TAG, "openTabTray: Trying to click the tabs tray button")
            tabTrayButton().click()
            Log.i(TAG, "openTabTray: Clicked the tabs tray button")
            mDevice.waitNotNull(
                Until.findObject(By.res("$packageName:id/tab_layout")),
                waitingTime,
            )

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun openComposeTabDrawer(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            for (i in 1..Constants.RETRY_COUNT) {
                try {
                    Log.i(TAG, "openComposeTabDrawer: Started try #$i")
                    mDevice.waitForObjects(
                        mDevice.findObject(
                            UiSelector()
                                .resourceId("$packageName:id/mozac_browser_toolbar_browser_actions"),
                        ),
                        waitingTime,
                    )
                    Log.i(TAG, "openComposeTabDrawer: Trying to click the tabs tray button")
                    tabTrayButton().click()
                    Log.i(TAG, "openComposeTabDrawer: Clicked the tabs tray button")
                    Log.i(TAG, "openComposeTabDrawer: Trying to verify that the tabs tray exists")
                    composeTestRule.onNodeWithTag(TabsTrayTestTag.tabsTray).assertExists()
                    Log.i(TAG, "openComposeTabDrawer: Verified that the tabs tray exists")

                    break
                } catch (e: AssertionError) {
                    Log.i(TAG, "openComposeTabDrawer: AssertionError caught, executing fallback methods")
                    if (i == Constants.RETRY_COUNT) {
                        throw e
                    } else {
                        Log.i(TAG, "openComposeTabDrawer: Waiting for device to be idle")
                        mDevice.waitForIdle()
                        Log.i(TAG, "openComposeTabDrawer: Waited for device to be idle")
                    }
                }
            }
            Log.i(TAG, "openComposeTabDrawer: Trying to verify the tabs tray new tab FAB button exists")
            composeTestRule.onNodeWithTag(TabsTrayTestTag.fab).assertExists()
            Log.i(TAG, "openComposeTabDrawer: Verified the tabs tray new tab FAB button exists")

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun visitLinkFromClipboard(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "visitLinkFromClipboard: Waiting for $waitingTimeShort ms for clear address button to exist")
            if (clearAddressBarButton().waitForExists(waitingTimeShort)) {
                Log.i(TAG, "visitLinkFromClipboard: Waited for $waitingTimeShort ms for clear address button to exist")
                Log.i(TAG, "visitLinkFromClipboard: Trying to click the clear address button")
                clearAddressBarButton().click()
                Log.i(TAG, "visitLinkFromClipboard: Clicked the clear address button")
            }

            mDevice.waitNotNull(
                Until.findObject(By.res("$packageName:id/clipboard_title")),
                waitingTime,
            )

            // On Android 12 or above we don't SHOW the URL unless the user requests to do so.
            // See for mor information https://github.com/mozilla-mobile/fenix/issues/22271
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
                mDevice.waitNotNull(
                    Until.findObject(By.res("$packageName:id/clipboard_url")),
                    waitingTime,
                )
            }
            Log.i(TAG, "visitLinkFromClipboard: Trying to click the fill link from clipboard button")
            fillLinkButton().click()
            Log.i(TAG, "visitLinkFromClipboard: Clicked the fill link from clipboard button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goBackToHomeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            Log.i(TAG, "goBackToHomeScreen: Trying to click the device back button")
            mDevice.pressBack()
            Log.i(TAG, "goBackToHomeScreen: Clicked the device back button")
            Log.i(TAG, "goBackToHomeScreen: Waiting for $waitingTimeShort ms for $packageName window to be updated")
            mDevice.waitForWindowUpdate(packageName, waitingTimeShort)
            Log.i(TAG, "goBackToHomeScreen: Waited for $waitingTimeShort ms for $packageName window to be updated")

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun goBackToBrowserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "goBackToBrowserScreen: Trying to click the device back button")
            mDevice.pressBack()
            Log.i(TAG, "goBackToBrowserScreen: Clicked the device back button")
            Log.i(TAG, "goBackToBrowserScreen: Waiting for $waitingTimeShort ms for $packageName window to be updated")
            mDevice.waitForWindowUpdate(packageName, waitingTimeShort)
            Log.i(TAG, "goBackToBrowserScreen: Waited for $waitingTimeShort ms for $packageName window to be updated")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTabButtonShortcutsMenu(interact: NavigationToolbarRobot.() -> Unit): Transition {
            mDevice.waitNotNull(Until.findObject(By.res("$packageName:id/counter_root")))
            Log.i(TAG, "openTabButtonShortcutsMenu: Trying to long click the tab counter button")
            tabsCounter().perform(longClick())
            Log.i(TAG, "openTabButtonShortcutsMenu: Long clicked the tab counter button")

            NavigationToolbarRobot().interact()
            return Transition()
        }

        fun closeTabFromShortcutsMenu(interact: NavigationToolbarRobot.() -> Unit): Transition {
            Log.i(TAG, "closeTabFromShortcutsMenu: Waiting for device to be idle for $waitingTime ms")
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "closeTabFromShortcutsMenu: Waited for device to be idle for $waitingTime ms")
            Log.i(TAG, "closeTabFromShortcutsMenu: Trying to click the \"Close tab\" button")
            onView(withId(R.id.mozac_browser_menu_recyclerView))
                .perform(
                    RecyclerViewActions.actionOnItem<RecyclerView.ViewHolder>(
                        hasDescendant(
                            withText("Close tab"),
                        ),
                        ViewActions.click(),
                    ),
                )
            Log.i(TAG, "closeTabFromShortcutsMenu: Clicked the \"Close tab\" button")

            NavigationToolbarRobot().interact()
            return Transition()
        }

        fun openNewTabFromShortcutsMenu(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "openNewTabFromShortcutsMenu: Waiting for device to be idle for $waitingTime ms")
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "openNewTabFromShortcutsMenu: Waited for device to be idle for $waitingTime ms")
            Log.i(TAG, "openNewTabFromShortcutsMenu: Trying to click the \"New tab\" button")
            onView(withId(R.id.mozac_browser_menu_recyclerView))
                .perform(
                    RecyclerViewActions.actionOnItem<RecyclerView.ViewHolder>(
                        hasDescendant(
                            withText("New tab"),
                        ),
                        ViewActions.click(),
                    ),
                )
            Log.i(TAG, "openNewTabFromShortcutsMenu: Clicked the \"New tab\" button")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun openNewPrivateTabFromShortcutsMenu(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "openNewPrivateTabFromShortcutsMenu: Waiting for device to be idle for $waitingTime ms")
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "openNewPrivateTabFromShortcutsMenu: Waited for device to be idle for $waitingTime ms")
            Log.i(TAG, "openNewPrivateTabFromShortcutsMenu: Trying to click the \"New private tab\" button")
            onView(withId(R.id.mozac_browser_menu_recyclerView))
                .perform(
                    RecyclerViewActions.actionOnItem<RecyclerView.ViewHolder>(
                        hasDescendant(
                            withText("New private tab"),
                        ),
                        ViewActions.click(),
                    ),
                )
            Log.i(TAG, "openNewPrivateTabFromShortcutsMenu: Clicked the \"New private tab\" button")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clickUrlbar(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "clickUrlbar: Trying to click the toolbar")
            urlBar().click()
            Log.i(TAG, "clickUrlbar: Clicked the toolbar")
            Log.i(TAG, "clickUrlbar: Waiting for $waitingTime ms for the edit mode toolbar to exist")
            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_edit_url_view"),
            ).waitForExists(waitingTime)
            Log.i(TAG, "clickUrlbar: Waited for $waitingTime ms for the edit mode toolbar to exist")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clickSearchSelectorButton(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "clickSearchSelectorButton: Waiting for $waitingTime ms for the search selector button to exist")
            searchSelectorButton().waitForExists(waitingTime)
            Log.i(TAG, "clickSearchSelectorButton: Waited for $waitingTime ms for the search selector button to exist")
            Log.i(TAG, "clickSearchSelectorButton: Trying to click the search selector button")
            searchSelectorButton().click()
            Log.i(TAG, "clickSearchSelectorButton: Clicked the search selector button")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }
    }
}

fun navigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
    NavigationToolbarRobot().interact()
    return NavigationToolbarRobot.Transition()
}

fun openEditURLView() {
    Log.i(TAG, "openEditURLView: Waiting for $waitingTime ms for the toolbar to exist")
    urlBar().waitForExists(waitingTime)
    Log.i(TAG, "openEditURLView: Waited for $waitingTime ms for the toolbar to exist")
    Log.i(TAG, "openEditURLView: Trying to click the toolbar")
    urlBar().click()
    Log.i(TAG, "openEditURLView: Clicked the toolbar")
    Log.i(TAG, "openEditURLView: Waiting for $waitingTime ms for the edit mode toolbar to exist")
    itemWithResId("$packageName:id/mozac_browser_toolbar_edit_url_view").waitForExists(waitingTime)
    Log.i(TAG, "openEditURLView: Waited for $waitingTime ms for the edit mode toolbar to exist")
}

private fun urlBar() = mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
private fun awesomeBar() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_edit_url_view"))
private fun threeDotButton() = onView(withId(R.id.mozac_browser_toolbar_menu))
private fun tabTrayButton() = onView(withId(R.id.tab_button))
private fun tabsCounter() = onView(withId(R.id.mozac_browser_toolbar_browser_actions))
private fun fillLinkButton() = onView(withId(R.id.fill_link_from_clipboard))
private fun clearAddressBarButton() = itemWithResId("$packageName:id/mozac_browser_toolbar_clear_view")
private fun readerViewToggle() =
    onView(withParent(withId(R.id.mozac_browser_toolbar_page_actions)))

private fun searchSelectorButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/search_selector"))

inline fun runWithIdleRes(ir: IdlingResource?, pendingCheck: () -> Unit) {
    try {
        IdlingRegistry.getInstance().register(ir)
        pendingCheck()
    } finally {
        IdlingRegistry.getInstance().unregister(ir)
    }
}
