/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.IdlingResource
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParentIndex
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.Matchers.allOf
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime
import org.mozilla.focus.idlingResources.SessionLoadedIdlingResource

class BrowserRobot {

    val progressBar =
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/progress")
        )

    fun verifyBrowserView() =
        assertTrue(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/webview"))
                .waitForExists(webPageLoadwaitingTime)
        )

    fun verifyPageContent(expectedText: String) {
        val sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        mDevice.findObject(UiSelector().resourceId("$packageName:id/webview"))
            .waitForExists(webPageLoadwaitingTime)

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                mDevice.findObject(UiSelector().textContains(expectedText))
                    .waitForExists(webPageLoadwaitingTime)
            )
        }
    }

    fun verifyPageURL(expectedText: String) {
        val sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        browserURLbar.waitForExists(webPageLoadwaitingTime)

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                mDevice.findObject(UiSelector().textContains(expectedText))
                    .waitForExists(webPageLoadwaitingTime)
            )
        }
    }

    fun clickGetLocationButton() {
        mDevice.findObject(UiSelector().textContains("Get Location")).click()
    }

    fun verifyFloatingEraseButton(): ViewInteraction =
        floatingEraseButton.check(matches(isDisplayed()))

    fun longPressLink(linkText: String) {
        val link = mDevice.findObject(UiSelector().text(linkText))
        link.waitForExists(webPageLoadwaitingTime)
        link.longClick()
    }

    fun openLinkInNewTab() {
        onView(withText(R.string.mozac_feature_contextmenu_open_link_in_private_tab))
            .perform(click())
    }

    fun verifyNumberOfTabsOpened(tabsCount: Int) {
        tabsCounter.check(matches(withContentDescription("Tabs open: $tabsCount")))
    }

    fun verifyTabsOrder(vararg tabTitle: String) {
        for (tab in tabTitle.indices) {
            onView(withId(R.id.sessions)).check(
                matches(
                    hasDescendant(
                        allOf(
                            withText(tabTitle[tab]),
                            withParentIndex(tab)
                        )
                    )
                )
            )
        }
    }

    fun openTabsTray(): ViewInteraction = tabsCounter.perform(click())

    fun selectTab(tabTitle: String): ViewInteraction = onView(withText(tabTitle)).perform(click())

    fun verifyShareAppsListOpened() =
        assertTrue(shareAppsList.waitForExists(webPageLoadwaitingTime))

    fun clickPlayButton() {
        val playButton =
            mDevice.findObject(UiSelector().text("Play"))
        playButton.waitForExists(webPageLoadwaitingTime)
        playButton.click()
    }

    fun clickPauseButton() {
        val pauseButton =
            mDevice.findObject(UiSelector().text("Pause"))
        pauseButton.waitForExists(webPageLoadwaitingTime)
        pauseButton.click()
    }

    fun waitForPlaybackToStart() {
        val playStateMessage = mDevice.findObject(UiSelector().text("Media file is playing"))
        assertTrue(playStateMessage.waitForExists(webPageLoadwaitingTime))
    }

    fun verifyPlaybackStopped() {
        val playStateMessage = mDevice.findObject(UiSelector().text("Media file is paused"))
        assertTrue(playStateMessage.waitForExists(webPageLoadwaitingTime))
    }

    fun dismissMediaPlayingAlert() {
        mDevice.findObject(UiSelector().textContains("OK")).click()
    }

    fun verifySiteSecurityIconShown(): ViewInteraction = securityIcon.check(matches(isDisplayed()))

    fun verifySiteConnectionInfoIsSecure(isSecure: Boolean) {
        securityIcon.perform(click())
        assertTrue(site_security_info.waitForExists(waitingTime))
        site_identity_title.check(matches(isDisplayed()))
        site_identity_Icon.check(matches(isDisplayed()))
        if (isSecure) {
            assertTrue(site_security_info.text.equals("Connection is secure"))
        } else {
            assertTrue(site_security_info.text.equals("Connection is not secure"))
        }
    }

    class Transition {
        fun openSearchBar(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            browserURLbar.waitForExists(webPageLoadwaitingTime)
            browserURLbar.click()

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clearBrowsingData(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            floatingEraseButton
                .check(matches(isCompletelyDisplayed()))
                .perform(click())

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun eraseBrowsingHistoryFromTabsTray(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            tabsCounter.perform(click())
            tabsTrayEraseHistoryButton.perform(click())

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openMainMenu(interact: ThreeDotMainMenuRobot.() -> Unit): ThreeDotMainMenuRobot.Transition {
            browserURLbar.waitForExists(webPageLoadwaitingTime)
            mainMenu
                .check(matches(isDisplayed()))
                .perform(click())

            ThreeDotMainMenuRobot().interact()
            return ThreeDotMainMenuRobot.Transition()
        }

        fun openThreeDotMenu(interact: ThreeDotMainMenuRobot.() -> Unit): ThreeDotMainMenuRobot.Transition {
            mainMenu
                .check(matches(isDisplayed()))
                .perform(click())

            ThreeDotMainMenuRobot().interact()
            return ThreeDotMainMenuRobot.Transition()
        }
    }
}

fun browserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
    BrowserRobot().interact()
    return BrowserRobot.Transition()
}

inline fun runWithIdleRes(ir: IdlingResource?, pendingCheck: () -> Unit) {
    try {
        IdlingRegistry.getInstance().register(ir)
        pendingCheck()
    } finally {
        IdlingRegistry.getInstance().unregister(ir)
    }
}

private val browserURLbar = mDevice.findObject(
    UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_url_view")
)

private val floatingEraseButton = onView(allOf(withId(R.id.erase), isDisplayed()))

private val tabsCounter = onView(withId(R.id.tabs))

private val tabsTrayEraseHistoryButton = onView(withText(R.string.tabs_tray_action_erase))

private val mainMenu = onView(withId(R.id.mozac_browser_toolbar_menu))

private val shareAppsList =
    mDevice.findObject(UiSelector().resourceId("android:id/resolver_list"))

private val securityIcon = onView(withId(R.id.mozac_browser_toolbar_tracking_protection_indicator))

private val site_security_info = mDevice.findObject(UiSelector().resourceId("$packageName:id/security_info"))

private val site_identity_title = onView(withId(R.id.site_title))

private val site_identity_Icon = onView(withId(R.id.site_favicon))
