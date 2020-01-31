/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Intent
import android.net.Uri
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.matcher.BundleMatchers
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.helpers.Constants.LongClickDuration

class BrowserRobot {

    fun verifyBrowserScreen() {
        onView(ViewMatchers.withResourceName("browserLayout"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifyUrl(url: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(By.res("org.mozilla.fenix.debug:id/mozac_browser_toolbar_url_view")),
            TestAssetHelper.waitingTime
        )
        onView(withId(R.id.mozac_browser_toolbar_url_view))
            .check(matches(withText(containsString(url))))
    }

    fun verifyHelpUrl() {
        verifyUrl("https://support.mozilla.org/")
    }

    fun verifyWhatsNewURL() {
        verifyUrl("https://support.mozilla.org/")
    }

    fun verifyRateOnGooglePlayURL() {
        verifyUrl("https://play.google.com/store/apps/details?id=org.mozilla.fenix")
    }

    /* Asserts that the text within DOM element with ID="testContent" has the given text, i.e.
    *  document.querySelector('#testContent').innerText == expectedText
    */
    fun verifyPageContent(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.text(expectedText)), TestAssetHelper.waitingTime)
    }

    fun verifyTabCounter(expectedText: String) {
        onView(withId(R.id.counter_text))
            .check((matches(withText(containsString(expectedText)))))
    }

    fun verifySnackBarText(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.text(expectedText)), TestAssetHelper.waitingTime)
    }

    fun verifyLinkContextMenuItems(containsURL: Uri) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(By.textContains(containsURL.toString())),
            TestAssetHelper.waitingTime
        )
        mDevice.waitNotNull(
            Until.findObject(By.text("Open link in new tab")),
            TestAssetHelper.waitingTime
        )
        mDevice.waitNotNull(
            Until.findObject(By.text("Open link in private tab")),
            TestAssetHelper.waitingTime
        )
        mDevice.waitNotNull(Until.findObject(By.text("Copy link")), TestAssetHelper.waitingTime)
        mDevice.waitNotNull(Until.findObject(By.text("Share link")), TestAssetHelper.waitingTime)
    }

    fun verifyLinkImageContextMenuItems(containsURL: Uri) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.textContains(containsURL.toString())))
        mDevice.waitNotNull(
            Until.findObject(By.text("Open link in new tab")), TestAssetHelper.waitingTime
        )
        mDevice.waitNotNull(
            Until.findObject(By.text("Open link in private tab")), TestAssetHelper.waitingTime
        )
        mDevice.waitNotNull(Until.findObject(By.text("Copy link")), TestAssetHelper.waitingTime)
        mDevice.waitNotNull(Until.findObject(By.text("Share link")), TestAssetHelper.waitingTime)
        mDevice.waitNotNull(
            Until.findObject(By.text("Open image in new tab")), TestAssetHelper.waitingTime
        )
        mDevice.waitNotNull(Until.findObject(By.text("Save image")), TestAssetHelper.waitingTime)
        mDevice.waitNotNull(
            Until.findObject(By.text("Copy image location")), TestAssetHelper.waitingTime
        )
    }

    fun verifyNoLinkImageContextMenuItems(containsTitle: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.textContains(containsTitle)))
        mDevice.waitNotNull(
            Until.findObject(By.text("Open image in new tab")),
            TestAssetHelper.waitingTime
        )
        mDevice.waitNotNull(Until.findObject(By.text("Save image")), TestAssetHelper.waitingTime)
        mDevice.waitNotNull(
            Until.findObject(By.text("Copy image location")), TestAssetHelper.waitingTime
        )
    }

    fun clickContextOpenLinkInNewTab() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(By.text("Open link in new tab")),
            TestAssetHelper.waitingTime
        )

        val menuOpenInNewTab = mDevice.findObject(By.text("Open link in new tab"))
        menuOpenInNewTab.click()
    }

    fun clickContextOpenLinkInPrivateTab() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(By.text("Open link in private tab")),
            TestAssetHelper.waitingTime
        )

        val menuOpenInPrivateTab = mDevice.findObject(By.text("Open link in private tab"))
        menuOpenInPrivateTab.click()
    }

    fun clickContextCopyLink() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.text("Copy link")), TestAssetHelper.waitingTime)

        val menuCopyLink = mDevice.findObject(By.text("Copy link"))
        menuCopyLink.click()
    }

    fun clickContextShareLink(url: Uri) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.text("Share link")), TestAssetHelper.waitingTime)

        val menuShareLink = mDevice.findObject(By.text("Share link"))
        menuShareLink.click()

        // verify share intent is launched and matched with associated passed in URL
        Intents.intended(
            allOf(
                IntentMatchers.hasAction(Intent.ACTION_CHOOSER),
                IntentMatchers.hasExtras(
                    allOf(
                        BundleMatchers.hasEntry(
                            Intent.EXTRA_INTENT,
                            allOf(
                                IntentMatchers.hasAction(Intent.ACTION_SEND),
                                IntentMatchers.hasType("text/plain"),
                                IntentMatchers.hasExtra(
                                    Intent.EXTRA_TEXT,
                                    url.toString()
                                )
                            )
                        )
                    )
                )
            )
        )
    }

    fun clickContextCopyImageLocation() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(By.text("Copy image location")),
            TestAssetHelper.waitingTime
        )

        val menuCopyImageLocation = mDevice.findObject(By.text("Copy image location"))
        menuCopyImageLocation.click()
    }

    fun clickContextOpenImageNewTab() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(By.text("Open image in new tab")),
            TestAssetHelper.waitingTime
        )

        val menuOpenImageNewTab = mDevice.findObject(By.text("Open image in new tab"))
        menuOpenImageNewTab.click()
    }

    fun clickContextSaveImage() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.text("Save image")), TestAssetHelper.waitingTime)

        val menuSaveImage = mDevice.findObject(By.text("Save image"))
        menuSaveImage.click()
    }

    fun waitForCollectionSavedPopup() {
        mDevice.wait(
            Until.findObject(By.text("Tab saved!")),
            TestAssetHelper.waitingTime
        )
    }

    fun createBookmark(url: Uri) {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(url) {
        }.openThreeDotMenu {
            clickAddBookmarkButton()
        }
    }

    fun clickLinkMatchingText(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.text(expectedText)), TestAssetHelper.waitingTime)

        val element = mDevice.findObject(By.text(expectedText))
        element.click()
    }

    fun longClickMatchingText(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.text(expectedText)), TestAssetHelper.waitingTime)

        val element = mDevice.findObject(By.text(expectedText))
        element.click(LongClickDuration.LONG_CLICK_DURATION)
    }

    fun snackBarButtonClick(expectedText: String) {
        onView(allOf(withId(R.id.snackbar_btn), withText(expectedText))).check(
            matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))
        ).perform(ViewActions.click())
    }

    class Transition {
        private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        private fun threeDotButton() = onView(
            CoreMatchers.allOf(
                ViewMatchers.withContentDescription(
                    "Menu"
                )
            )
        )

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            mDevice.waitForIdle()
            threeDotButton().perform(ViewActions.click())

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openNavigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {

            navURLBar().click()

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openHomeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.waitForIdle()

            tabsCounter().click()

            mDevice.waitNotNull(
                Until.findObject(By.res("org.mozilla.fenix.debug:id/header_text")),
                TestAssetHelper.waitingTime
            )

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }
    }
}

fun browserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
    BrowserRobot().interact()
    return BrowserRobot.Transition()
}

private fun dismissOnboardingButton() = onView(withId(R.id.close_onboarding))

fun dismissTrackingOnboarding() {
    mDevice.wait(Until.findObject(By.res("close_onboarding")), TestAssetHelper.waitingTime)
    dismissOnboardingButton().click()
}

fun navURLBar() = onView(withId(R.id.mozac_browser_toolbar_url_view))

private fun tabsCounter() = onView(withId(R.id.counter_box))
