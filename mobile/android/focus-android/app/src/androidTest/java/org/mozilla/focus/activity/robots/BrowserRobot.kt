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
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParentIndex
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.idlingResources.SessionLoadedIdlingResource

class BrowserRobot {

    val progressBar =
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/progress")
        )

    fun verifyBrowserView() =
        assertTrue(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
                .waitForExists(waitingTime)
        )

    fun verifyPageContent(expectedText: String) {
        val sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
            .waitForExists(waitingTime)

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                mDevice.findObject(UiSelector().textContains(expectedText))
                    .waitForExists(waitingTime)
            )
        }
    }

    fun verifyTrackingProtectionAlert(expectedText: String) {
        mDevice.wait(Until.findObject(By.textContains(expectedText)), waitingTime)
        assertTrue(
            mDevice.findObject(UiSelector().textContains(expectedText))
                .waitForExists(waitingTime)
        )
        // close the JavaScript alert
        mDevice.pressBack()
    }

    fun verifyPageURL(expectedText: String) {
        val sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        browserURLbar.waitForExists(waitingTime)

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                mDevice.findObject(UiSelector().textContains(expectedText))
                    .waitForExists(waitingTime)
            )
        }
    }

    fun clickGetLocationButton() {
        mDevice.findObject(UiSelector().textContains("Get Location")).click()
    }

    fun verifyEraseBrowsingButton(): ViewInteraction =
        eraseBrowsingButton.check(matches(isDisplayed()))

    fun longPressLink(linkText: String) {
        val link = mDevice.findObject(UiSelector().text(linkText))
        link.waitForExists(waitingTime)
        link.longClick()
    }

    fun openLinkInNewTab() {
        openLinkInPrivateTab.perform(click())
    }

    fun verifyNumberOfTabsOpened(tabsCount: Int) {
        tabsCounter.check(matches(withContentDescription("$tabsCount open tabs. Tap to switch tabs.")))
    }

    fun verifyTabsOrder(vararg tabTitle: String) {
        for (tab in tabTitle.indices) {
            onView(withId(R.id.sessions)).check(
                matches(
                    hasDescendant(
                        allOf(
                            hasDescendant(
                                withText(tabTitle[tab])
                            ),
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
        assertTrue(shareAppsList.waitForExists(waitingTime))

    fun clickPlayButton() {
        val playButton =
            mDevice.findObject(UiSelector().text("Play"))
        playButton.waitForExists(waitingTime)
        playButton.click()
    }

    fun clickPauseButton() {
        val pauseButton =
            mDevice.findObject(UiSelector().text("Pause"))
        pauseButton.waitForExists(waitingTime)
        pauseButton.click()
    }

    fun waitForPlaybackToStart() {
        val playStateMessage = mDevice.findObject(UiSelector().text("Media file is playing"))
        assertTrue(playStateMessage.waitForExists(waitingTime))
    }

    fun verifyPlaybackStopped() {
        val playStateMessage = mDevice.findObject(UiSelector().text("Media file is paused"))
        assertTrue(playStateMessage.waitForExists(waitingTime))
    }

    fun dismissMediaPlayingAlert() {
        mDevice.findObject(UiSelector().textContains("OK")).click()
    }

    fun verifySiteTrackingProtectionIconShown() = assertTrue(securityIcon.waitForExists(waitingTime))

    fun verifySiteSecurityIndicatorShown() = assertTrue(site_security_indicator.waitForExists(waitingTime))

    fun verifyLinkContextMenu(linkAddress: String) {
        onView(withId(R.id.titleView)).check(matches(withText(linkAddress)))
        openLinkInPrivateTab.check(matches(isDisplayed()))
        copyLink.check(matches(isDisplayed()))
        shareLink.check(matches(isDisplayed()))
    }

    class Transition {
        fun openSearchBar(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            browserURLbar.waitForExists(waitingTime)
            browserURLbar.click()

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clearBrowsingData(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            eraseBrowsingButton
                .check(matches(isDisplayed()))
                .perform(click())

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openMainMenu(interact: ThreeDotMainMenuRobot.() -> Unit): ThreeDotMainMenuRobot.Transition {
            browserURLbar.waitForExists(waitingTime)
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

        fun openSiteSettingsMenu(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            securityIcon.click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openSiteSecurityInfoSheet(interact: SiteSecurityInfoSheetRobot.() -> Unit): SiteSecurityInfoSheetRobot.Transition {
            if (securityIcon.exists()) {
                securityIcon.click()
            } else {
                site_security_indicator.click()
            }

            SiteSecurityInfoSheetRobot().interact()
            return SiteSecurityInfoSheetRobot.Transition()
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

private val eraseBrowsingButton = onView(withContentDescription("Erase browsing history"))

private val tabsCounter = onView(withId(R.id.counter_root))

private val mainMenu = onView(withId(R.id.mozac_browser_toolbar_menu))

private val shareAppsList =
    mDevice.findObject(UiSelector().resourceId("android:id/resolver_list"))

private val securityIcon =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/mozac_browser_toolbar_tracking_protection_indicator")
    )

private val site_security_indicator =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/mozac_browser_toolbar_security_indicator")
    )

// Link long-tap context menu items
private val openLinkInPrivateTab = onView(withText("Open link in private tab"))

private val copyLink = onView(withText("Copy link"))

private val shareLink = onView(withText("Share link"))
