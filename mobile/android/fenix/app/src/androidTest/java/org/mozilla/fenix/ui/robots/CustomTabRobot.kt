/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithDescriptionExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdAndTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.waitForObjects
import org.mozilla.fenix.helpers.click

/**
 *  Implementation of the robot pattern for Custom tabs
 */
class CustomTabRobot {

    fun verifyCustomTabsSiteInfoButton() =
        assertItemWithResIdExists(
            itemWithResId("$packageName:id/mozac_browser_toolbar_security_indicator"),
        )

    fun verifyCustomTabsShareButton() =
        assertItemWithDescriptionExists(
            itemWithDescription(getStringResource(R.string.mozac_feature_customtabs_share_link)),
        )

    fun verifyMainMenuButton() = assertItemWithResIdExists(mainMenuButton)

    fun verifyDesktopSiteButtonExists() {
        desktopSiteButton().check(matches(isDisplayed()))
    }

    fun verifyFindInPageButtonExists() {
        findInPageButton().check(matches(isDisplayed()))
    }

    fun verifyPoweredByTextIsDisplayed() =
        assertItemContainingTextExists(itemContainingText("POWERED BY $appName"))

    fun verifyOpenInBrowserButtonExists() {
        openInBrowserButton().check(matches(isDisplayed()))
    }

    fun verifyBackButtonExists() = assertItemWithDescriptionExists(itemWithDescription("Back"))

    fun verifyForwardButtonExists() = assertItemWithDescriptionExists(itemWithDescription("Forward"))

    fun verifyRefreshButtonExists() = assertItemWithDescriptionExists(itemWithDescription("Refresh"))

    fun verifyCustomMenuItem(label: String) = assertItemContainingTextExists(itemContainingText(label))

    fun verifyCustomTabCloseButton() {
        closeButton().check(matches(isDisplayed()))
    }

    fun verifyCustomTabToolbarTitle(title: String) {
        waitForPageToLoad()

        mDevice.waitForObjects(
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/mozac_browser_toolbar_title_view")
                    .textContains(title),
            )
                .getFromParent(
                    UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_origin_view"),
                ),
            waitingTime,
        )

        assertItemWithResIdAndTextExists(
            itemWithResIdContainingText("$packageName:id/mozac_browser_toolbar_title_view", title),
        )
    }

    fun verifyCustomTabUrl(Url: String) {
        assertItemWithResIdAndTextExists(
            itemWithResIdContainingText("$packageName:id/mozac_browser_toolbar_url_view", Url.drop(7)),
        )
    }

    fun longCLickAndCopyToolbarUrl() {
        mDevice.waitForObjects(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar")),
            waitingTime,
        )
        customTabToolbar().click(LONG_CLICK_DURATION)
        clickContextMenuItem("Copy")
    }

    fun fillAndSubmitLoginCredentials(userName: String, password: String) {
        mDevice.waitForIdle(waitingTime)
        setPageObjectText(itemWithResId("username"), userName)
        setPageObjectText(itemWithResId("password"), password)
        clickPageObject(itemWithResId("submit"))
        mDevice.waitForObjects(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/save_confirm")),
            waitingTime,
        )
    }

    fun waitForPageToLoad() = progressBar.waitUntilGone(waitingTime)

    fun clickCustomTabCloseButton() = closeButton().click()

    fun verifyCustomTabActionButton(customTabActionButtonDescription: String) =
        assertItemWithDescriptionExists(itemWithDescription(customTabActionButtonDescription))

    fun verifyPDFReaderToolbarItems() =
        assertItemWithResIdAndTextExists(
            itemWithResIdAndText("download", "Download"),
            itemWithResIdAndText("openInApp", "Open in app"),
        )

    class Transition {
        fun openMainMenu(interact: CustomTabRobot.() -> Unit): Transition {
            mainMenuButton.also {
                it.waitForExists(waitingTime)
                it.click()
            }

            CustomTabRobot().interact()
            return Transition()
        }

        fun clickOpenInBrowserButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            openInBrowserButton().perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShareButton(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            itemWithDescription(getStringResource(R.string.mozac_feature_customtabs_share_link)).click()

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }
    }
}

fun customTabScreen(interact: CustomTabRobot.() -> Unit): CustomTabRobot.Transition {
    CustomTabRobot().interact()
    return CustomTabRobot.Transition()
}

private val mainMenuButton = itemWithResId("$packageName:id/mozac_browser_toolbar_menu")

private fun desktopSiteButton() = onView(withId(R.id.switch_widget))

private fun findInPageButton() = onView(withText("Find in page"))

private fun openInBrowserButton() = onView(withText("Open in $appName"))

private fun closeButton() = onView(withContentDescription("Return to previous app"))

private fun customTabToolbar() = mDevice.findObject(By.res("$packageName:id/toolbar"))

private val progressBar =
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_progress"),
    )
