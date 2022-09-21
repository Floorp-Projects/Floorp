/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.IdlingResource
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.not
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.Constants.RETRY_COUNT
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.pageLoadingTime
import org.mozilla.focus.helpers.TestHelper.progressBar
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.waitingTimeShort
import org.mozilla.focus.idlingResources.SessionLoadedIdlingResource
import java.time.LocalDate

class BrowserRobot {

    val progressBar =
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/progress"),
        )

    fun verifyBrowserView() =
        assertTrue(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
                .waitForExists(waitingTime),
        )

    fun verifyPageContent(expectedText: String) {
        mDevice.wait(Until.findObject(By.textContains(expectedText)), waitingTime)
    }

    fun verifyTrackingProtectionAlert(expectedText: String) {
        mDevice.wait(Until.findObject(By.textContains(expectedText)), waitingTime)
        assertTrue(
            mDevice.findObject(UiSelector().textContains(expectedText))
                .waitForExists(waitingTime),
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
                    .waitForExists(waitingTime),
            )
        }
    }

    fun clickGetLocationButton() {
        mDevice.findObject(UiSelector().textContains("Get Location")).waitForExists(waitingTime)
        mDevice.findObject(UiSelector().textContains("Get Location")).click()
    }

    fun clickGetCameraButton() {
        mDevice.findObject(UiSelector().textContains("Open camera")).waitForExists(waitingTime)
        mDevice.findObject(UiSelector().textContains("Open camera")).click()
    }

    fun verifyCameraPermissionPrompt(url: String) {
        assertTrue(
            mDevice.findObject(UiSelector().text("Allow $url to use your camera?"))
                .waitForExists(waitingTime),
        )
    }

    fun verifyLocationPermissionPrompt(url: String) {
        assertTrue(
            mDevice.findObject(UiSelector().text("Allow $url to use your location?"))
                .waitForExists(waitingTime),
        )
    }

    fun allowSitePermissionRequest() {
        if (permissionAllowBtn.waitForExists(waitingTime)) {
            permissionAllowBtn.click()
        }
    }

    fun denySitePermissionRequest() {
        if (permissionDenyBtn.waitForExists(waitingTime)) {
            permissionDenyBtn.click()
        }
    }

    fun verifyEraseBrowsingButton(): ViewInteraction =
        eraseBrowsingButton.check(matches(isDisplayed()))

    fun longPressLink(linkText: String) {
        val link = mDevice.findObject(UiSelector().text(linkText))
        link.waitForExists(waitingTime)
        link.longClick()
    }

    fun openLinkInNewTab() {
        mDevice.findObject(
            UiSelector().textContains("Open link in private tab"),
        ).waitForExists(waitingTime)
        openLinkInPrivateTab.perform(click())
    }

    fun verifyNumberOfTabsOpened(tabsCount: Int) {
        assertTrue(
            mDevice.findObject(
                UiSelector().description("$tabsCount open tabs. Tap to switch tabs."),
            ).waitForExists(waitingTime),
        )
    }

    fun verifyTabsCounterNotShown() {
        assertFalse(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/counter_root"))
                .waitForExists(waitingTimeShort),
        )
    }

    fun verifyShareAppsListOpened() =
        assertTrue(shareAppsList.waitForExists(waitingTime))

    fun clickPlayButton() {
        val playButton =
            mDevice.findObject(UiSelector().text("Play"))
        playButton.waitForExists(pageLoadingTime)
        playButton.click()
    }

    fun clickPauseButton() {
        val pauseButton =
            mDevice.findObject(UiSelector().text("Pause"))
        pauseButton.waitForExists(pageLoadingTime)
        pauseButton.click()
    }

    fun waitForPlaybackToStart() {
        val playStateMessage = mDevice.findObject(UiSelector().text("Media file is playing"))

        for (i in 1..RETRY_COUNT) {
            try {
                assertTrue(playStateMessage.waitForExists(pageLoadingTime))
                break
            } catch (e: AssertionError) {
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    clickPlayButton()
                }
            }
        }

        // dismiss the js alert
        mDevice.findObject(UiSelector().textContains("ok")).click()
    }

    fun verifyPlaybackStopped() {
        val playStateMessage = mDevice.findObject(UiSelector().text("Media file is paused"))
        assertTrue(playStateMessage.waitForExists(waitingTime))
        // dismiss the js alert
        mDevice.findObject(UiSelector().textContains("ok")).click()
    }

    fun verifySiteTrackingProtectionIconShown() = assertTrue(securityIcon.waitForExists(waitingTime))

    fun verifySiteSecurityIndicatorShown() = assertTrue(site_security_indicator.waitForExists(waitingTime))

    fun verifyLinkContextMenu(linkAddress: String) {
        onView(withId(R.id.titleView)).check(matches(withText(linkAddress)))
        openLinkInPrivateTab.check(matches(isDisplayed()))
        copyLink.check(matches(isDisplayed()))
        shareLink.check(matches(isDisplayed()))
    }

    fun verifyImageContextMenu(hasLink: Boolean, linkAddress: String) {
        onView(withId(R.id.titleView)).check(matches(withText(linkAddress)))
        if (hasLink) {
            openLinkInPrivateTab.check(matches(isDisplayed()))
            downloadLink.check(matches(isDisplayed()))
        }
        copyLink.check(matches(isDisplayed()))
        shareLink.check(matches(isDisplayed()))
        shareImage.check(matches(isDisplayed()))
        openImageInNewTab.check(matches(isDisplayed()))
        saveImage.check(matches(isDisplayed()))
        copyImageLocation.check(matches(isDisplayed()))
    }

    fun clickContextMenuCopyLink(): ViewInteraction = copyLink.perform(click())

    fun clickShareImage(): ViewInteraction = shareImage.perform(click())

    fun clickShareLink(): ViewInteraction = shareLink.perform(click())

    fun clickCopyImageLocation(): ViewInteraction = copyImageLocation.perform(click())

    fun clickLinkMatchingText(expectedText: String) {
        mDevice.findObject(UiSelector().textContains(expectedText)).waitForExists(waitingTime)
        mDevice.findObject(UiSelector().textContains(expectedText)).also { it.click() }
    }

    fun verifyOpenLinksInAppsPrompt(openLinksInAppsEnabled: Boolean, link: String) = assertOpenLinksInAppsPrompt(openLinksInAppsEnabled, link)

    fun clickOpenLinksInAppsCancelButton() {
        for (i in 1..RETRY_COUNT) {
            try {
                openLinksInAppsCancelButton.click()
                assertTrue(openLinksInAppsMessage.waitUntilGone(waitingTime))

                break
            } catch (e: AssertionError) {
                if (i == RETRY_COUNT) {
                    throw e
                }
            }
        }
    }

    fun clickOpenLinksInAppsOpenButton() = openLinksInAppsOpenButton.click()

    fun clickDropDownForm() {
        mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
            .waitForExists(waitingTime)
        mDevice.findObject(UiSelector().textContains("Drop-down Form")).waitForExists(waitingTime)
        dropDownForm.click()
    }

    fun clickCalendarForm() {
        mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
            .waitForExists(waitingTime)
        mDevice.findObject(UiSelector().textContains("Calendar Form")).waitForExists(waitingTime)
        calendarForm.click()
        mDevice.waitForIdle(waitingTime)
    }

    fun selectDate() {
        mDevice.findObject(UiSelector().resourceId("android:id/month_view")).waitForExists(waitingTime)

        val monthViewerCurrentDay =
            mDevice.findObject(
                UiSelector()
                    .textContains("$currentDay")
                    .descriptionContains("$currentDay $currentMonth $currentYear"),
            )

        monthViewerCurrentDay.click()
    }

    fun clickFormViewButton(button: String) {
        val clockAndCalendarButton = mDevice.findObject(UiSelector().textContains(button))
        clockAndCalendarButton.click()
    }

    fun clickSubmitDateButton() {
        submitDateButton.waitForExists(waitingTime)
        submitDateButton.click()
    }

    fun verifySelectedDate() {
        mDevice.findObject(
            UiSelector()
                .textContains("Submit date")
                .resourceId("submitDate"),
        ).waitForExists(waitingTime)

        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .text("Selected date is: $currentDate"),
            ).waitForExists(waitingTime),
        )
    }

    fun clickAndWriteTextInInputBox(text: String) {
        mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
            .waitForExists(waitingTime)
        mDevice.findObject(UiSelector().textContains("Input Text")).waitForExists(waitingTime)
        textInputBox.click()
        textInputBox.setText(text)
    }

    fun longPressTextInputBox() {
        textInputBox.waitForExists(waitingTime)
        textInputBox.longClick()
    }

    fun longClickText(expectedText: String) {
        var currentTries = 0
        while (currentTries++ < 3) {
            try {
                mDevice.findObject(UiSelector().textContains(expectedText))
                    .waitForExists(waitingTime)
                mDevice.findObject(By.textContains(expectedText)).also { it.longClick() }
                break
            } catch (e: NullPointerException) {
                browserScreen {
                }.openMainMenu {
                }.clickReloadButton {}
            }
        }
    }

    fun longClickAndCopyText(expectedText: String) {
        var currentTries = 0
        while (currentTries++ < 3) {
            try {
                mDevice.findObject(UiSelector().textContains(expectedText)).waitForExists(waitingTime)
                mDevice.findObject(By.textContains(expectedText)).also { it.longClick() }

                mDevice.findObject(UiSelector().textContains("Copy")).waitForExists(waitingTime)
                mDevice.findObject(By.textContains("Copy")).also { it.click() }
                break
            } catch (e: NullPointerException) {
                browserScreen {
                }.openMainMenu {
                }.clickReloadButton {}
            }
        }
    }

    fun verifyCopyOptionDoesNotExist() =
        assertFalse(mDevice.findObject(UiSelector().textContains("Copy")).waitForExists(waitingTime))

    fun clickAndPasteTextInInputBox() {
        var currentTries = 0
        while (currentTries++ < 3) {
            try {
                mDevice.findObject(UiSelector().textContains("Paste")).waitForExists(waitingTime)
                mDevice.findObject(By.textContains("Paste")).also { it.click() }
                break
            } catch (e: NullPointerException) {
                longPressTextInputBox()
            }
        }
    }

    fun clickSubmitTextInputButton() {
        submitTextInputButton.waitForExists(waitingTime)
        submitTextInputButton.click()
    }

    fun selectDropDownOption(optionName: String) {
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/customPanel"),
        ).waitForExists(waitingTime)
        mDevice.findObject(UiSelector().textContains(optionName)).also { it.click() }
    }

    fun clickSubmitDropDownButton() {
        submitDropDownButton.waitForExists(waitingTime)
        submitDropDownButton.click()
    }

    fun verifySelectedDropDownOption(optionName: String) {
        mDevice.findObject(
            UiSelector()
                .textContains("Submit drop down option")
                .resourceId("submitOption"),
        ).waitForExists(waitingTime)

        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .text("Selected option is: $optionName"),
            ).waitForExists(waitingTime),
        )
    }

    fun enterFindInPageQuery(expectedText: String) {
        mDevice.wait(Until.findObject(By.res("$packageName:id/find_in_page_query_text")), waitingTime)
        findInPageQuery.perform(ViewActions.clearText())
        mDevice.wait(Until.gone(By.res("$packageName:id/find_in_page_result_text")), waitingTime)
        findInPageQuery.perform(ViewActions.typeText(expectedText))
        mDevice.wait(Until.findObject(By.res("$packageName:id/find_in_page_result_text")), waitingTime)
    }

    fun verifyFindNextInPageResult(ratioCounter: String) {
        mDevice.wait(Until.findObject(By.text(ratioCounter)), waitingTime)
        val resultsCounter = mDevice.findObject(By.text(ratioCounter))
        findInPageResult.check(matches(withText((ratioCounter))))
        findInPageNextButton.perform(click())
        resultsCounter.wait(Until.textNotEquals(ratioCounter), waitingTime)
    }

    fun verifyFindPrevInPageResult(ratioCounter: String) {
        mDevice.wait(Until.findObject(By.text(ratioCounter)), waitingTime)
        val resultsCounter = mDevice.findObject(By.text(ratioCounter))
        findInPageResult.check(matches(withText((ratioCounter))))
        findInPagePrevButton.perform(click())
        resultsCounter.wait(Until.textNotEquals(ratioCounter), waitingTime)
    }

    fun closeFindInPage() {
        findInPageCloseButton.perform(click())
        findInPageQuery.check(matches(not(isDisplayed())))
    }

    fun verifyCookiesEnabled(areCookiesEnabled: String) {
        mDevice.findObject(UiSelector().resourceId("detected_value")).waitForExists(waitingTime)
        assertTrue(
            mDevice.findObject(
                UiSelector().textContains(areCookiesEnabled),
            ).waitForExists(waitingTime),
        )
    }

    fun clickSetCookiesButton() {
        val setCookiesButton = mDevice.findObject(UiSelector().resourceId("setCookies"))
        setCookiesButton.waitForExists(waitingTime)
        setCookiesButton.click()
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

        fun openTabsTray(interact: TabsTrayRobot.() -> Unit): TabsTrayRobot.Transition {
            tabsCounter.perform(click())

            TabsTrayRobot().interact()
            return TabsTrayRobot.Transition()
        }

        fun goToPreviousPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.pressBack()
            progressBar.waitUntilGone(waitingTime)

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSaveImage(interact: DownloadRobot.() -> Unit): DownloadRobot.Transition {
            saveImage.perform(click())

            DownloadRobot().interact()
            return DownloadRobot.Transition()
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

private fun assertOpenLinksInAppsPrompt(openLinksInAppsEnabled: Boolean, link: String) {
    if (openLinksInAppsEnabled) {
        mDevice.findObject(UiSelector().resourceId("$packageName:id/parentPanel")).waitForExists(waitingTime)
        assertTrue(openLinksInAppsMessage.waitForExists(waitingTimeShort))
        assertTrue(openLinksInAppsLink(link).exists())
        assertTrue(openLinksInAppsCancelButton.waitForExists(waitingTimeShort))
        assertTrue(openLinksInAppsOpenButton.waitForExists(waitingTimeShort))
    } else {
        assertFalse(
            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/parentPanel"),
            ).waitForExists(waitingTimeShort),
        )
    }
}

private fun openLinksInAppsLink(link: String) = mDevice.findObject(UiSelector().textContains(link))

private val browserURLbar = mDevice.findObject(
    UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_url_view"),
)

private val eraseBrowsingButton = onView(withContentDescription("Erase browsing history"))

private val tabsCounter = onView(withId(R.id.counter_root))

private val mainMenu = onView(withId(R.id.mozac_browser_toolbar_menu))

private val shareAppsList =
    mDevice.findObject(UiSelector().resourceId("android:id/resolver_list"))

private val securityIcon =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/mozac_browser_toolbar_tracking_protection_indicator"),
    )

private val site_security_indicator =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/mozac_browser_toolbar_security_indicator"),
    )

// Link long-tap context menu items
private val openLinkInPrivateTab = onView(withText("Open link in private tab"))

private val copyLink = onView(withText("Copy link"))

private val shareLink = onView(withText("Share link"))

// Image long-tap context menu items
private val openImageInNewTab = onView(withText("Open image in new tab"))

private val downloadLink = onView(withText("Download link"))

private val saveImage = onView(withText("Save image"))

private val copyImageLocation = onView(withText("Copy image location"))

private val shareImage = onView(withText("Share image"))

// Find in page toolbar
private val findInPageQuery = onView(withId(R.id.find_in_page_query_text))

private val findInPageResult = onView(withId(R.id.find_in_page_result_text))

private val findInPageNextButton = onView(withId(R.id.find_in_page_next_btn))

private val findInPagePrevButton = onView(withId(R.id.find_in_page_prev_btn))

private val findInPageCloseButton = onView(withId(R.id.find_in_page_close_btn))

private val openLinksInAppsMessage = mDevice.findObject(UiSelector().resourceId("$packageName:id/alertTitle"))

private val openLinksInAppsCancelButton = mDevice.findObject(UiSelector().textContains("CANCEL"))

private val openLinksInAppsOpenButton =
    mDevice.findObject(
        UiSelector()
            .index(1)
            .textContains("OPEN")
            .className("android.widget.Button")
            .packageName(packageName),
    )

private val currentDate = LocalDate.now()
private val currentDay = currentDate.dayOfMonth
private val currentMonth = currentDate.month
private val currentYear = currentDate.year

private val dropDownForm =
    mDevice.findObject(
        UiSelector()
            .resourceId("dropDown")
            .className("android.widget.Spinner")
            .packageName(packageName),
    )

private val submitDropDownButton =
    mDevice.findObject(
        UiSelector()
            .textContains("Submit drop down option")
            .resourceId("submitOption"),
    )

private val textInputBox =
    mDevice.findObject(
        UiSelector()
            .resourceId("textInput")
            .packageName(packageName),
    )

private val submitTextInputButton =
    mDevice.findObject(
        UiSelector()
            .textContains("Submit input")
            .resourceId("submitInput"),
    )

private val calendarForm =
    mDevice.findObject(
        UiSelector()
            .resourceId("calendar")
            .className("android.widget.Spinner")
            .packageName(packageName),
    )

val submitDateButton =
    mDevice.findObject(
        UiSelector()
            .textContains("Submit date")
            .resourceId("submitDate"),
    )

private val permissionAllowBtn = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/allow_button"),
)

private val permissionDenyBtn = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/deny_button"),
)
