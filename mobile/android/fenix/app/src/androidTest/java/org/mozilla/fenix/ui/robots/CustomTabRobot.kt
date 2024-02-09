/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.ui.robots

import android.util.Log
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
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
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
        assertUIObjectExists(
            itemWithResId("$packageName:id/mozac_browser_toolbar_security_indicator"),
        )

    fun verifyCustomTabsShareButton() =
        assertUIObjectExists(
            itemWithDescription(getStringResource(R.string.mozac_feature_customtabs_share_link)),
        )

    fun verifyMainMenuButton() = assertUIObjectExists(mainMenuButton())

    fun verifyDesktopSiteButtonExists() {
        Log.i(TAG, "verifyDesktopSiteButtonExists: Trying to verify that the request desktop site button is displayed")
        desktopSiteButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyDesktopSiteButtonExists: Verified that the request desktop site button is displayed")
    }

    fun verifyFindInPageButtonExists() {
        Log.i(TAG, "verifyFindInPageButtonExists: Trying to verify that the find in page button is displayed")
        findInPageButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyFindInPageButtonExists: Verified that the find in page button is displayed")
    }

    fun verifyPoweredByTextIsDisplayed() =
        assertUIObjectExists(itemContainingText("POWERED BY $appName"))

    fun verifyOpenInBrowserButtonExists() {
        Log.i(TAG, "verifyOpenInBrowserButtonExists: Trying to verify that the \"Open in Firefox\" button is displayed")
        openInBrowserButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyOpenInBrowserButtonExists: Verified that the \"Open in Firefox\" button is displayed")
    }

    fun verifyBackButtonExists() = assertUIObjectExists(itemWithDescription("Back"))

    fun verifyForwardButtonExists() = assertUIObjectExists(itemWithDescription("Forward"))

    fun verifyRefreshButtonExists() = assertUIObjectExists(itemWithDescription("Refresh"))

    fun verifyCustomMenuItem(label: String) = assertUIObjectExists(itemContainingText(label))

    fun verifyCustomTabCloseButton() {
        Log.i(TAG, "verifyCustomTabCloseButton: Trying to verify that the close custom tab button is displayed")
        closeButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyCustomTabCloseButton: Verified that the close custom tab button is displayed")
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

        assertUIObjectExists(
            itemWithResIdContainingText("$packageName:id/mozac_browser_toolbar_title_view", title),
        )
    }

    fun verifyCustomTabUrl(Url: String) {
        assertUIObjectExists(
            itemWithResIdContainingText("$packageName:id/mozac_browser_toolbar_url_view", Url.drop(7)),
        )
    }

    fun longCLickAndCopyToolbarUrl() {
        mDevice.waitForObjects(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar")),
            waitingTime,
        )
        Log.i(TAG, "longCLickAndCopyToolbarUrl: Trying to long click the custom tab toolbar")
        customTabToolbar().click(LONG_CLICK_DURATION)
        Log.i(TAG, "longCLickAndCopyToolbarUrl: Long clicked the custom tab toolbar")
        clickContextMenuItem("Copy")
    }

    fun fillAndSubmitLoginCredentials(userName: String, password: String) {
        Log.i(TAG, "fillAndSubmitLoginCredentials: Waiting for device to be idle for $waitingTime ms")
        mDevice.waitForIdle(waitingTime)
        Log.i(TAG, "fillAndSubmitLoginCredentials: Waited for device to be idle for $waitingTime ms")
        setPageObjectText(itemWithResId("username"), userName)
        setPageObjectText(itemWithResId("password"), password)
        clickPageObject(itemWithResId("submit"))
        mDevice.waitForObjects(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/save_confirm")),
            waitingTime,
        )
    }

    fun waitForPageToLoad() {
        Log.i(TAG, "waitForPageToLoad: Waiting for $waitingTime ms until progress bar is gone")
        progressBar().waitUntilGone(waitingTime)
        Log.i(TAG, "waitForPageToLoad: Waited for $waitingTime ms until progress bar was gone")
    }

    fun clickCustomTabCloseButton() {
        Log.i(TAG, "clickCustomTabCloseButton: Trying to click close custom tab button")
        closeButton().click()
        Log.i(TAG, "clickCustomTabCloseButton: Clicked close custom tab button")
    }

    fun verifyCustomTabActionButton(customTabActionButtonDescription: String) =
        assertUIObjectExists(itemWithDescription(customTabActionButtonDescription))

    fun verifyPDFReaderToolbarItems() =
        assertUIObjectExists(
            itemWithResIdAndText("download", "Download"),
            itemWithResIdAndText("openInApp", "Open in app"),
        )

    class Transition {
        fun openMainMenu(interact: CustomTabRobot.() -> Unit): Transition {
            mainMenuButton().also {
                Log.i(TAG, "openMainMenu: Waiting for $waitingTime ms for the main menu button to exist")
                it.waitForExists(waitingTime)
                Log.i(TAG, "openMainMenu: Waited for $waitingTime ms for the main menu button to exist")
                Log.i(TAG, "openMainMenu: Trying to click the main menu button")
                it.click()
                Log.i(TAG, "openMainMenu: Clicked the main menu button")
            }

            CustomTabRobot().interact()
            return Transition()
        }

        fun clickOpenInBrowserButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickOpenInBrowserButton: Trying to click the \"Open in Firefox\" button")
            openInBrowserButton().perform(click())
            Log.i(TAG, "clickOpenInBrowserButton: Clicked the \"Open in Firefox\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShareButton(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            Log.i(TAG, "clickShareButton: Trying to click the share button")
            itemWithDescription(getStringResource(R.string.mozac_feature_customtabs_share_link)).click()
            Log.i(TAG, "clickShareButton: Clicked the share button")

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }
    }
}

fun customTabScreen(interact: CustomTabRobot.() -> Unit): CustomTabRobot.Transition {
    CustomTabRobot().interact()
    return CustomTabRobot.Transition()
}

private fun mainMenuButton() = itemWithResId("$packageName:id/mozac_browser_toolbar_menu")

private fun desktopSiteButton() = onView(withId(R.id.switch_widget))

private fun findInPageButton() = onView(withText("Find in page"))

private fun openInBrowserButton() = onView(withText("Open in $appName"))

private fun closeButton() = onView(withContentDescription("Return to previous app"))

private fun customTabToolbar() = mDevice.findObject(By.res("$packageName:id/toolbar"))

private fun progressBar() =
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_progress"),
    )
