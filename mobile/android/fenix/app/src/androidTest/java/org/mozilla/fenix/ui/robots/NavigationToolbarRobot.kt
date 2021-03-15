/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.net.Uri
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.IdlingResource
import androidx.test.espresso.IdlingResourceTimeoutException
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.pressImeActionButton
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withResourceName
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.anyOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.CoreMatchers.not
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.SessionLoadedIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.assertions.AwesomeBarAssertion.Companion.suggestionsAreEqualTo
import org.mozilla.fenix.helpers.assertions.AwesomeBarAssertion.Companion.suggestionsAreGreaterThan
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the URL toolbar.
 */
class NavigationToolbarRobot {

    fun verifySearchSuggestionsAreMoreThan(suggestionSize: Int) =
        assertSuggestionsAreMoreThan(suggestionSize)

    fun verifySearchSuggestionsAreEqualTo(suggestionSize: Int) =
        assertSuggestionsAreEqualTo(suggestionSize)

    fun verifyNoHistoryBookmarks() = assertNoHistoryBookmarks()

    fun verifyTabButtonShortcutMenuItems() = assertTabButtonShortcutMenuItems()

    fun verifyReaderViewDetected(visible: Boolean = false) =
        assertReaderViewDetected(visible)

    fun verifyCloseReaderViewDetected(visible: Boolean = false) =
        assertCloseReaderViewDetected(visible)

    fun typeSearchTerm(searchTerm: String) = awesomeBar().perform(typeText(searchTerm))

    fun toggleReaderView() {
        mDevice.findObject(UiSelector()
            .resourceId("$packageName:id/mozac_browser_toolbar_page_actions"))
            .waitForExists(waitingTime)

        readerViewToggle().click()
    }

    class Transition {

        private lateinit var sessionLoadedIdlingResource: SessionLoadedIdlingResource
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun goBackToWebsite(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            openEditURLView()
            clearAddressBar().click()
            awesomeBar().check((matches(withText(containsString("")))))
            goBackButton()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun enterURLAndEnterToBrowser(
            url: Uri,
            interact: BrowserRobot.() -> Unit
        ): BrowserRobot.Transition {
            sessionLoadedIdlingResource = SessionLoadedIdlingResource()

            openEditURLView()

            awesomeBar().perform(replaceText(url.toString()), pressImeActionButton())

            runWithIdleRes(sessionLoadedIdlingResource) {
                onView(
                    anyOf(
                        withResourceName("browserLayout"),
                        withResourceName("onboarding_message"), // Req ETP dialog
                        withResourceName("download_button")
                    )
                )
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTrackingProtectionTestPage(url: Uri, etpEnabled: Boolean, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            sessionLoadedIdlingResource = SessionLoadedIdlingResource()

            openEditURLView()

            awesomeBar().perform(replaceText(url.toString()), pressImeActionButton())

            runWithIdleRes(sessionLoadedIdlingResource) {
                when (etpEnabled) {
                    true ->
                        try {
                            onView(withId(R.id.onboarding_message))
                                .check(matches(isDisplayed()))
                        } catch (e: IdlingResourceTimeoutException) {
                            openThreeDotMenu {
                            }.stopPageLoad {
                                val onboardingDisplayed =
                                    mDevice.findObject(UiSelector().resourceId("$packageName:id/onboarding_message"))
                                        .waitForExists(waitingTime)

                                if (!onboardingDisplayed) {
                                    openThreeDotMenu {
                                    }.refreshPage {}
                                }
                            }
                        }

                    false ->
                        try {
                            onView(withResourceName("browserLayout")).check(matches(isDisplayed()))
                        } catch (e: IdlingResourceTimeoutException) {
                            openThreeDotMenu {
                            }.stopPageLoad {
                            }.openThreeDotMenu {
                            }.refreshPage {}
                        }
                }
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTabCrashReporter(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            val crashUrl = "about:crashcontent"

            sessionLoadedIdlingResource = SessionLoadedIdlingResource()

            openEditURLView()

            awesomeBar().perform(replaceText(crashUrl), pressImeActionButton())

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
            onView(withId(R.id.tab_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

            tabTrayButton().click()

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun openNewTabAndEnterToBrowser(
            url: Uri,
            interact: BrowserRobot.() -> Unit
        ): BrowserRobot.Transition {
            sessionLoadedIdlingResource = SessionLoadedIdlingResource()
            mDevice.waitNotNull(Until.findObject(By.res("$packageName:id/toolbar")), waitingTime)
            urlBar().click()
            awesomeBar().perform(replaceText(url.toString()), pressImeActionButton())

            runWithIdleRes(sessionLoadedIdlingResource) {
                onView(
                    anyOf(
                        ViewMatchers.withResourceName("browserLayout"),
                        ViewMatchers.withResourceName("onboarding_message") // Req for ETP dialog
                    )
                )
                    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun visitLinkFromClipboard(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(
                Until.findObject(By.res("org.mozilla.fenix.debug:id/mozac_browser_toolbar_clear_view")),
                waitingTime
            )
            clearAddressBar().click()

            mDevice.waitNotNull(
                Until.findObject(By.res("org.mozilla.fenix.debug:id/clipboard_title")),
                waitingTime
            )

            mDevice.waitNotNull(
                Until.findObject(By.res("org.mozilla.fenix.debug:id/clipboard_url")),
                waitingTime
            )
            fillLinkButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            goBackButton()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun closeTabFromShortcutsMenu(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
            mDevice.waitForIdle(waitingTime)

            onView(withId(R.id.mozac_browser_menu_recyclerView))
                .perform(
                    RecyclerViewActions.actionOnItem<RecyclerView.ViewHolder>(
                        hasDescendant(
                            withText("Close tab")
                        ), ViewActions.click()
                    )
                )

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openTabFromShortcutsMenu(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.waitForIdle(waitingTime)

            onView(withId(R.id.mozac_browser_menu_recyclerView))
                .perform(
                    RecyclerViewActions.actionOnItem<RecyclerView.ViewHolder>(
                        hasDescendant(
                            withText("New tab")
                        ), ViewActions.click()
                    )
                )

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openNewPrivateTabFromShortcutsMenu(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.waitForIdle(waitingTime)

            onView(withId(R.id.mozac_browser_menu_recyclerView))
                .perform(
                    RecyclerViewActions.actionOnItem<RecyclerView.ViewHolder>(
                        hasDescendant(
                            withText("New private tab")
                        ), ViewActions.click()
                    )
                )

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }
    }
}

fun navigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
    NavigationToolbarRobot().interact()
    return NavigationToolbarRobot.Transition()
}

fun clickUrlbar(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
    urlBar().click()
    SearchRobot().interact()
    return SearchRobot.Transition()
}

fun openEditURLView() {
    mDevice.waitNotNull(
        Until.findObject(By.res("$packageName:id/toolbar")),
        waitingTime
    )
    urlBar().click()
    mDevice.waitNotNull(
        Until.findObject(By.res("$packageName:id/mozac_browser_toolbar_edit_url_view")),
        waitingTime
    )
}

private fun assertSuggestionsAreEqualTo(suggestionSize: Int) {
    mDevice.waitForIdle()
    onView(withId(R.id.awesome_bar)).check(suggestionsAreEqualTo(suggestionSize))
}

private fun assertSuggestionsAreMoreThan(suggestionSize: Int) {
    mDevice.waitForIdle()
    onView(withId(R.id.awesome_bar)).check(suggestionsAreGreaterThan(suggestionSize))
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

private fun dismissOnboardingButton() = onView(withId(R.id.close_onboarding))
private fun urlBar() = onView(withId(R.id.toolbar))
private fun awesomeBar() = onView(withId(R.id.mozac_browser_toolbar_edit_url_view))
private fun threeDotButton() = onView(withId(R.id.mozac_browser_toolbar_menu))
private fun tabTrayButton() = onView(withId(R.id.tab_button))
private fun fillLinkButton() = onView(withId(R.id.fill_link_from_clipboard))
private fun clearAddressBar() = onView(withId(R.id.mozac_browser_toolbar_clear_view))
private fun goBackButton() = mDevice.pressBack()
private fun readerViewToggle() =
    onView(withParent(withId(R.id.mozac_browser_toolbar_page_actions)))

private fun assertReaderViewDetected(visible: Boolean) {
    mDevice.findObject(UiSelector()
        .description("Reader view"))
        .waitForExists(waitingTime)

    onView(
        allOf(
            withParent(withId(R.id.mozac_browser_toolbar_page_actions)),
            withContentDescription("Reader view")
        )
    ).check(
        if (visible) matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))
        else ViewAssertions.doesNotExist()
    )
}

private fun assertCloseReaderViewDetected(visible: Boolean) {
    mDevice.findObject(UiSelector()
        .description("Close reader view"))
        .waitForExists(waitingTime)

    onView(
        allOf(
            withParent(withId(R.id.mozac_browser_toolbar_page_actions)),
            withContentDescription("Close reader view")
        )
    ).check(
        if (visible) matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))
        else ViewAssertions.doesNotExist()
    )
}

inline fun runWithIdleRes(ir: IdlingResource?, pendingCheck: () -> Unit) {
    try {
        IdlingRegistry.getInstance().register(ir)
        pendingCheck()
    } finally {
        IdlingRegistry.getInstance().unregister(ir)
    }
}
