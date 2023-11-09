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
import org.hamcrest.CoreMatchers.not
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdAndTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdExists
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
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
    fun verifyUrl(url: String) =
        onView(withId(R.id.mozac_browser_toolbar_url_view)).check(matches(withText(url)))

    fun verifyNoHistoryBookmarks() = assertNoHistoryBookmarks()

    fun verifyTabButtonShortcutMenuItems() = assertTabButtonShortcutMenuItems()

    fun verifyReaderViewDetected(visible: Boolean = false) =
        assertReaderViewDetected(visible)

    fun verifyCloseReaderViewDetected(visible: Boolean = false) =
        assertCloseReaderViewDetected(visible)

    fun toggleReaderView() {
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/mozac_browser_toolbar_page_actions"),
        )
            .waitForExists(waitingTime)

        readerViewToggle().click()
    }

    fun verifyClipboardSuggestionsAreDisplayed(link: String = "", shouldBeDisplayed: Boolean) {
        assertItemWithResIdExists(
            itemWithResId("$packageName:id/fill_link_from_clipboard"),
            exists = shouldBeDisplayed,
        )
        assertItemWithResIdAndTextExists(
            itemWithResIdAndText(
                "$packageName:id/clipboard_url",
                link,
            ),
            exists = shouldBeDisplayed,
        )
    }

    fun longClickEditModeToolbar() =
        mDevice.findObject(By.res("$packageName:id/mozac_browser_toolbar_edit_url_view")).click(LONG_CLICK_DURATION)

    fun clickContextMenuItem(item: String) {
        mDevice.waitNotNull(
            Until.findObject(By.text(item)),
            waitingTime,
        )
        mDevice.findObject(By.text(item)).click()
    }

    fun clickClearToolbarButton() = clearAddressBarButton().click()

    fun verifyToolbarIsEmpty() =
        itemWithResIdContainingText(
            "$packageName:id/mozac_browser_toolbar_edit_url_view",
            getStringResource(R.string.search_hint),
        )

    // New unified search UI selector
    fun verifySearchBarPlaceholder(text: String) {
        urlBar().waitForExists(waitingTime)
        assertTrue(
            urlBar().text == text,
        )
    }

    // New unified search UI selector
    fun verifyDefaultSearchEngine(engineName: String) =
        assertItemWithResIdExists(
            searchSelectorButton.getChild(UiSelector().description(engineName)),
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
            Log.i(TAG, "enterURLAndEnterToBrowser: Opened edit mode URL view")

            awesomeBar().setText(url.toString())
            Log.i(TAG, "enterURLAndEnterToBrowser: Set toolbar text to: $url")
            mDevice.pressEnter()
            Log.i(TAG, "enterURLAndEnterToBrowser: Clicked enter on keyboard, submitted query")

            runWithIdleRes(sessionLoadedIdlingResource) {
                assertTrue(
                    itemWithResId("$packageName:id/browserLayout").waitForExists(waitingTime) ||
                        itemWithResId("$packageName:id/download_button").waitForExists(waitingTime) ||
                        itemWithText(getStringResource(R.string.tcp_cfr_message)).waitForExists(waitingTime),
                )
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun enterURLAndEnterToBrowserForTCPCFR(
            url: Uri,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            openEditURLView()

            awesomeBar().setText(url.toString())
            mDevice.pressEnter()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTabCrashReporter(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            val crashUrl = "about:crashcontent"

            sessionLoadedIdlingResource = SessionLoadedIdlingResource()

            openEditURLView()

            awesomeBar().setText(crashUrl)
            mDevice.pressEnter()

            runWithIdleRes(sessionLoadedIdlingResource) {
                mDevice.findObject(UiSelector().resourceId("$packageName:id/crash_tab_image"))
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.res("$packageName:id/mozac_browser_toolbar_menu")), waitingTime)
            threeDotButton().click()

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openTabTray(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            mDevice.waitForIdle(waitingTime)
            tabTrayButton().click()
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
                    mDevice.waitForObjects(
                        mDevice.findObject(
                            UiSelector()
                                .resourceId("$packageName:id/mozac_browser_toolbar_browser_actions"),
                        ),
                        waitingTime,
                    )

                    tabTrayButton().click()

                    composeTestRule.onNodeWithTag(TabsTrayTestTag.tabsTray).assertExists()

                    break
                } catch (e: AssertionError) {
                    if (i == Constants.RETRY_COUNT) {
                        throw e
                    } else {
                        mDevice.waitForIdle()
                    }
                }
            }

            composeTestRule.onNodeWithTag(TabsTrayTestTag.fab).assertExists()

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun visitLinkFromClipboard(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            if (clearAddressBarButton().waitForExists(waitingTimeShort)) {
                clearAddressBarButton().click()
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

            fillLinkButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goBackToHomeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.pressBack()
            mDevice.waitForWindowUpdate(packageName, waitingTimeShort)

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openTabButtonShortcutsMenu(interact: NavigationToolbarRobot.() -> Unit): Transition {
            mDevice.waitNotNull(Until.findObject(By.res("$packageName:id/counter_root")))
            tabsCounter().click(LONG_CLICK_DURATION)
            Log.i(TAG, "Tabs counter long-click successful.")

            NavigationToolbarRobot().interact()
            return Transition()
        }

        fun closeTabFromShortcutsMenu(interact: NavigationToolbarRobot.() -> Unit): Transition {
            mDevice.waitForIdle(waitingTime)

            onView(withId(R.id.mozac_browser_menu_recyclerView))
                .perform(
                    RecyclerViewActions.actionOnItem<RecyclerView.ViewHolder>(
                        hasDescendant(
                            withText("Close tab"),
                        ),
                        ViewActions.click(),
                    ),
                )
            Log.i(TAG, "Clicked the tab shortcut Close tab button.")

            NavigationToolbarRobot().interact()
            return Transition()
        }

        fun openNewTabFromShortcutsMenu(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "Looking for tab shortcut New tab button.")
            onView(withId(R.id.mozac_browser_menu_recyclerView))
                .perform(
                    RecyclerViewActions.actionOnItem<RecyclerView.ViewHolder>(
                        hasDescendant(
                            withText("New tab"),
                        ),
                        ViewActions.click(),
                    ),
                )
            Log.i(TAG, "Clicked the tab shortcut New tab button.")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun openNewPrivateTabFromShortcutsMenu(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "Looking for tab shortcut New private tab button.")
            onView(withId(R.id.mozac_browser_menu_recyclerView))
                .perform(
                    RecyclerViewActions.actionOnItem<RecyclerView.ViewHolder>(
                        hasDescendant(
                            withText("New private tab"),
                        ),
                        ViewActions.click(),
                    ),
                )
            Log.i(TAG, "Clicked the tab shortcut New private tab button.")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clickUrlbar(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            urlBar().click()

            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_edit_url_view"),
            ).waitForExists(waitingTime)

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clickSearchSelectorButton(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            searchSelectorButton.waitForExists(waitingTime)
            searchSelectorButton.click()

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
    urlBar().waitForExists(waitingTime)
    urlBar().click()
    Log.i(TAG, "openEditURLView: URL bar clicked.")
    itemWithResId("$packageName:id/mozac_browser_toolbar_edit_url_view").waitForExists(waitingTime)
    Log.i(TAG, "openEditURLView: Edit URL bar displayed.")
}

private fun assertNoHistoryBookmarks() {
    onView(withId(R.id.container))
        .check(matches(not(hasDescendant(withText("Test_Page_1")))))
        .check(matches(not(hasDescendant(withText("Test_Page_2")))))
        .check(matches(not(hasDescendant(withText("Test_Page_3")))))
}

private fun assertTabButtonShortcutMenuItems() {
    onView(withId(R.id.mozac_browser_menu_recyclerView))
        .check(matches(hasDescendant(withText("Close tab"))))
        .check(matches(hasDescendant(withText("New private tab"))))
        .check(matches(hasDescendant(withText("New tab"))))
}

private fun urlBar() = mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
private fun awesomeBar() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_edit_url_view"))
private fun threeDotButton() = onView(withId(R.id.mozac_browser_toolbar_menu))
private fun tabTrayButton() = onView(withId(R.id.tab_button))
private fun tabsCounter() =
    mDevice.findObject(By.res("$packageName:id/counter_root"))
private fun fillLinkButton() = onView(withId(R.id.fill_link_from_clipboard))
private fun clearAddressBarButton() = itemWithResId("$packageName:id/mozac_browser_toolbar_clear_view")
private fun readerViewToggle() =
    onView(withParent(withId(R.id.mozac_browser_toolbar_page_actions)))

private fun assertReaderViewDetected(visible: Boolean) {
    mDevice.findObject(
        UiSelector()
            .description("Reader view"),
    )
        .waitForExists(waitingTime)

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
}

private fun assertCloseReaderViewDetected(visible: Boolean) {
    mDevice.findObject(
        UiSelector()
            .description("Close reader view"),
    )
        .waitForExists(waitingTime)

    onView(
        allOf(
            withParent(withId(R.id.mozac_browser_toolbar_page_actions)),
            withContentDescription("Close reader view"),
        ),
    ).check(
        if (visible) {
            matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))
        } else {
            ViewAssertions.doesNotExist()
        },
    )
}

private val searchSelectorButton =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/search_selector"))

inline fun runWithIdleRes(ir: IdlingResource?, pendingCheck: () -> Unit) {
    try {
        IdlingRegistry.getInstance().register(ir)
        pendingCheck()
    } finally {
        IdlingRegistry.getInstance().unregister(ir)
    }
}
