/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions", "TooGenericExceptionCaught")

package org.mozilla.fenix.ui.robots

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.SystemClock
import android.widget.EditText
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.Espresso.pressBack
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.matcher.BundleMatchers
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.matcher.RootMatchers.isDialog
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withResourceName
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.Matchers.not
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.SessionLoadedIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

class BrowserRobot {
    private lateinit var sessionLoadedIdlingResource: SessionLoadedIdlingResource

    fun verifyCurrentPrivateSession(context: Context) {
        val selectedTab = context.components.core.store.state.selectedTab
        assertTrue("Current session is private", selectedTab?.content?.private ?: false)
    }

    fun verifyUrl(url: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/mozac_browser_toolbar_url_view")
                        .textContains(url.replace("http://", ""))
                ).waitForExists(waitingTime)
            )
        }
    }

    fun verifyHelpUrl() {
        verifyUrl("support.mozilla.org/")
    }

    fun verifyWhatsNewURL() {
        verifyUrl("support.mozilla.org/")
    }

    fun verifyRateOnGooglePlayURL() {
        verifyUrl("play.google.com/store/apps/details?id=org.mozilla.fenix")
    }

    /* Asserts that the text within DOM element with ID="testContent" has the given text, i.e.
    *  document.querySelector('#testContent').innerText == expectedText
    *
    */

    fun verifyPageContent(expectedText: String) {
        sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        mDevice.waitNotNull(
            Until.findObject(By.res("$packageName:id/engineView")),
            waitingTime
        )

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(mDevice.findObject(UiSelector().textContains(expectedText)).waitForExists(waitingTime))
        }
    }

    fun verifyTabCounter(expectedText: String) {
        onView(withId(R.id.counter_text))
            .check((matches(withText(containsString(expectedText)))))
    }

    fun verifySnackBarText(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(text(expectedText)), waitingTime)

        onView(withText(expectedText)).check(
            matches(isCompletelyDisplayed())
        )
    }

    fun verifyLinkContextMenuItems(containsURL: Uri) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(By.textContains(containsURL.toString())),
            waitingTime
        )
        mDevice.waitNotNull(
            Until.findObject(text("Open link in new tab")),
            waitingTime
        )
        mDevice.waitNotNull(
            Until.findObject(text("Open link in private tab")),
            waitingTime
        )
        mDevice.waitNotNull(Until.findObject(text("Copy link")), waitingTime)
        mDevice.waitNotNull(Until.findObject(text("Share link")), waitingTime)
    }

    fun verifyLinkImageContextMenuItems(containsURL: Uri) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.textContains(containsURL.toString())))
        mDevice.waitNotNull(
            Until.findObject(text("Open link in new tab")), waitingTime
        )
        mDevice.waitNotNull(
            Until.findObject(text("Open link in private tab")), waitingTime
        )
        mDevice.waitNotNull(Until.findObject(text("Copy link")), waitingTime)
        mDevice.waitNotNull(Until.findObject(text("Share link")), waitingTime)
        mDevice.waitNotNull(
            Until.findObject(text("Open image in new tab")), waitingTime
        )
        mDevice.waitNotNull(Until.findObject(text("Save image")), waitingTime)
        mDevice.waitNotNull(
            Until.findObject(text("Copy image location")), waitingTime
        )
    }

    fun verifyNavURLBar() = assertNavURLBar()

    fun verifyNavURLBarHidden() = assertNavURLBarHidden()

    fun verifySecureConnectionLockIcon() = assertSecureConnectionLockIcon()

    fun verifyEnhancedTrackingProtectionSwitch() = assertEnhancedTrackingProtectionSwitch()

    fun clickEnhancedTrackingProtectionSwitchOffOn() =
        onView(withResourceName("switch_widget")).click()

    fun verifyProtectionSettingsButton() = assertProtectionSettingsButton()

    fun verifyEnhancedTrackingOptions() {
        clickEnhancedTrackingProtectionPanel()
        verifyEnhancedTrackingProtectionSwitch()
        verifyProtectionSettingsButton()
    }

    fun verifyMenuButton() = assertMenuButton()

    fun verifyNavURLBarItems() {
        verifyEnhancedTrackingOptions()
        pressBack()
        waitingTime
        verifySecureConnectionLockIcon()
        verifyTabCounter("1")
        verifyNavURLBar()
        verifyMenuButton()
    }

    fun verifyNoLinkImageContextMenuItems(containsURL: Uri) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.textContains(containsURL.toString())))
        mDevice.waitNotNull(
            Until.findObject(text("Open image in new tab")),
            waitingTime
        )
        mDevice.waitNotNull(Until.findObject(text("Save image")), waitingTime)
        mDevice.waitNotNull(
            Until.findObject(text("Copy image location")), waitingTime
        )
    }

    fun verifyNotificationDotOnMainMenu() {
        assertTrue(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/notification_dot"))
                .waitForExists(waitingTime)
        )
    }

    fun dismissContentContextMenu(containsURL: Uri) {
        onView(withText(containsURL.toString()))
            .inRoot(isDialog())
            .check(matches(isDisplayed()))
            .perform(ViewActions.pressBack())
    }

    fun clickEnhancedTrackingProtectionPanel() = enhancedTrackingProtectionIndicator().click()

    fun verifyEnhancedTrackingProtectionPanelNotVisible() =
        assertEnhancedTrackingProtectionIndicatorNotVisible()

    fun clickContextOpenLinkInNewTab() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(text("Open link in new tab")),
            waitingTime
        )

        val menuOpenInNewTab = mDevice.findObject(text("Open link in new tab"))
        menuOpenInNewTab.click()
    }

    fun clickContextOpenLinkInPrivateTab() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(text("Open link in private tab")),
            waitingTime
        )

        val menuOpenInPrivateTab = mDevice.findObject(text("Open link in private tab"))
        menuOpenInPrivateTab.click()
    }

    fun clickContextCopyLink() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(text("Copy link")), waitingTime)

        val menuCopyLink = mDevice.findObject(text("Copy link"))
        menuCopyLink.click()
    }

    fun clickContextShareLink(url: Uri) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(text("Share link")), waitingTime)

        val menuShareLink = mDevice.findObject(text("Share link"))
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
            Until.findObject(text("Copy image location")),
            waitingTime
        )

        val menuCopyImageLocation = mDevice.findObject(text("Copy image location"))
        menuCopyImageLocation.click()
    }

    fun clickContextOpenImageNewTab() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(text("Open image in new tab")),
            waitingTime
        )

        val menuOpenImageNewTab = mDevice.findObject(text("Open image in new tab"))
        menuOpenImageNewTab.click()
    }

    fun clickContextSaveImage() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(text("Save image")), waitingTime)

        val menuSaveImage = mDevice.findObject(text("Save image"))
        menuSaveImage.click()
    }

    fun createBookmark(url: Uri) {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(url) {
            // needs to wait for the right url to load before saving a bookmark
            verifyUrl(url.toString())
        }.openThreeDotMenu {
        }.bookmarkPage { }
    }

    fun clickLinkMatchingText(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(text(expectedText)), waitingTime)

        val element = mDevice.findObject(text(expectedText))
        element.click()
    }

    fun longClickMatchingText(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(text(expectedText)), waitingTime)

        val element = mDevice.findObject(text(expectedText))
        element.click(LONG_CLICK_DURATION)
    }

    fun longClickAndCopyText(expectedText: String, selectAll: Boolean = false) {
        try {
            // Long click desired text
            mDevice.waitForWindowUpdate(packageName, waitingTime)
            mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
                .waitForExists(waitingTime)
            mDevice.findObject(UiSelector().textContains(expectedText)).waitForExists(waitingTime)
            val link = mDevice.findObject(By.textContains(expectedText))
            link.click(LONG_CLICK_DURATION)

            // Click Select all from the text selection toolbar
            if (selectAll) {
                mDevice.findObject(UiSelector().textContains("Select all")).waitForExists(waitingTime)
                val selectAllText = mDevice.findObject(By.textContains("Select all"))
                selectAllText.click()
            }

            // Click Copy from the text selection toolbar
            mDevice.findObject(UiSelector().textContains("Copy")).waitForExists(waitingTime)
            val copyText = mDevice.findObject(By.textContains("Copy"))
            copyText.click()
        } catch (e: NullPointerException) {
            println("Failed to long click desired text: ${e.localizedMessage}")

            // Refresh the page in case the first long click didn't succeed
            navigationToolbar {
            }.openThreeDotMenu {
            }.refreshPage {
                mDevice.waitForIdle()
            }

            // Long click again the desired text
            mDevice.waitForWindowUpdate(packageName, waitingTime)
            mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
                .waitForExists(waitingTime)
            mDevice.findObject(UiSelector().textContains(expectedText)).waitForExists(waitingTime)
            val link = mDevice.findObject(By.textContains(expectedText))
            link.click(LONG_CLICK_DURATION)

            // Click again Select all from the text selection toolbar
            if (selectAll) {
                mDevice.findObject(UiSelector().textContains("Select all")).waitForExists(waitingTime)
                val selectAllText = mDevice.findObject(By.textContains("Select all"))
                selectAllText.click()
            }

            // Click again Copy from the text selection toolbar
            mDevice.findObject(UiSelector().textContains("Copy")).waitForExists(waitingTime)
            val copyText = mDevice.findObject(By.textContains("Copy"))
            copyText.click()
        }
    }

    fun snackBarButtonClick(expectedText: String) {
        onView(allOf(withId(R.id.snackbar_btn), withText(expectedText))).check(
            matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))
        ).perform(ViewActions.click())
    }

    fun verifySaveLoginPromptIsShown() {
        mDevice.findObject(UiSelector().text("test@example.com")).waitForExists(waitingTime)
        val submitButton = mDevice.findObject(By.res("submit"))
        submitButton.clickAndWait(Until.newWindow(), waitingTime)
        // Click save to save the login
        mDevice.waitNotNull(Until.findObjects(text("Save")))
    }

    fun verifyUpdateLoginPromptIsShown() {
        val submitButton = mDevice.findObject(By.res("submit"))
        submitButton.clickAndWait(Until.newWindow(), waitingTime)

        mDevice.waitNotNull(Until.findObjects(text("Update")))
    }

    fun saveLoginFromPrompt(optionToSaveLogin: String) {
        mDevice.findObject(text(optionToSaveLogin)).click()
    }

    fun enterPassword(password: String) {
        val passwordField = mDevice.findObject(
            UiSelector()
                .resourceId("password")
                .className(EditText::class.java)
        )
        passwordField.waitForExists(waitingTime)
        passwordField.setText(password)
        // wait until the password is hidden
        assertTrue(mDevice.findObject(UiSelector().text(password)).waitUntilGone(waitingTime))
    }

    fun clickMediaPlayerPlayButton() {
        mediaPlayerPlayButton().waitForExists(waitingTime)
        mediaPlayerPlayButton().click()
    }

    /**
     * Get the current playback state of the currently selected tab.
     * The result may be null if there if the currently playing media tab cannot be found in [store]
     *
     * @param store [BrowserStore] from which to get data about the current tab's state.
     * @return nullable [MediaSession.PlaybackState] indicating the media playback state for the current tab.
     */
    private fun getCurrentPlaybackState(store: BrowserStore): MediaSession.PlaybackState? {
        return store.state.selectedTab?.mediaSessionState?.playbackState
    }

    /**
     * Asserts that in [waitingTime] the playback state of the current tab will be [expectedState].
     *
     * @param store [BrowserStore] from which to get data about the current tab's state.
     * @param expectedState [MediaSession.PlaybackState] the playback state that will be asserted
     * @param waitingTime maximum time the test will wait for the playback state to become [expectedState]
     * before failing the assertion.
     */
    fun assertPlaybackState(store: BrowserStore, expectedState: MediaSession.PlaybackState) {
        val startMills = SystemClock.uptimeMillis()
        var currentMills: Long = 0
        while (currentMills <= waitingTime) {
            if (expectedState == getCurrentPlaybackState(store)) return
            currentMills = SystemClock.uptimeMillis() - startMills
        }
        fail("Playback did not moved to state: $expectedState")
    }

    fun swipeNavBarRight(tabUrl: String) {
        // failing to swipe on Firebase sometimes, so it tries again
        try {
            navURLBar().perform(ViewActions.swipeRight())
            assertTrue(mDevice.findObject(UiSelector().text(tabUrl)).waitUntilGone(waitingTime))
        } catch (e: AssertionError) {
            navURLBar().perform(ViewActions.swipeRight())
            assertTrue(mDevice.findObject(UiSelector().text(tabUrl)).waitUntilGone(waitingTime))
        }
    }

    fun swipeNavBarLeft(tabUrl: String) {
        // failing to swipe on Firebase sometimes, so it tries again
        try {
            navURLBar().perform(ViewActions.swipeLeft())
            assertTrue(mDevice.findObject(UiSelector().text(tabUrl)).waitUntilGone(waitingTime))
        } catch (e: AssertionError) {
            navURLBar().perform(ViewActions.swipeLeft())
            assertTrue(mDevice.findObject(UiSelector().text(tabUrl)).waitUntilGone(waitingTime))
        }
    }

    class Transition {
        private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        private fun threeDotButton() = onView(
            allOf(
                ViewMatchers.withContentDescription(
                    "Menu"
                )
            )
        )

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            mDevice.waitForIdle(waitingTime)
            threeDotButton().perform(ViewActions.click())

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openNavigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
            mDevice.waitForIdle(waitingTime)
            navURLBar().click()

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openTabDrawer(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            mDevice.waitForIdle(waitingTime)
            tabsCounter().click()
            mDevice.waitNotNull(
                Until.findObject(By.res("$packageName:id/tab_layout")),
                waitingTime
            )

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun openTabButtonShortcutsMenu(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
            mDevice.waitForIdle(waitingTime)

            tabsCounter().perform(
                ViewActions.longClick()
            )

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openNotificationShade(interact: NotificationRobot.() -> Unit): NotificationRobot.Transition {
            mDevice.openNotification()

            NotificationRobot().interact()
            return NotificationRobot.Transition()
        }

        fun goToHomescreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            onView(withContentDescription("Home screen"))
                .check(matches(isDisplayed()))
                .click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun clickTabCrashedCloseButton(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {

            assertTrue(
                mDevice.findObject(UiSelector().resourceId("$packageName:id/closeTabButton"))
                    .waitForExists(waitingTime)
            )

            val tabCrashedCloseButton = mDevice.findObject(text("Close tab"))
            tabCrashedCloseButton.click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }
    }
}

fun browserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
    BrowserRobot().interact()
    return BrowserRobot.Transition()
}

fun dismissTrackingOnboarding() {
    mDevice.wait(Until.findObject(By.res("close_onboarding")), waitingTime)
    dismissOnboardingButton().click()
}

fun navURLBar() = onView(withId(R.id.toolbar))

private fun assertNavURLBar() = navURLBar()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNavURLBarHidden() = navURLBar()
    .check(matches(not(isDisplayed())))

fun enhancedTrackingProtectionIndicator() =
    onView(withId(R.id.mozac_browser_toolbar_tracking_protection_indicator))

private fun assertEnhancedTrackingProtectionIndicatorNotVisible() {
    enhancedTrackingProtectionIndicator().check(matches(not(isDisplayed())))
}

private fun assertEnhancedTrackingProtectionSwitch() {
    withText(R.id.trackingProtectionSwitch)
        .matches(withEffectiveVisibility(Visibility.VISIBLE))
}

private fun assertProtectionSettingsButton() {
    onView(withId(R.id.protection_settings))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertSecureConnectionLockIcon() {
    onView(withId(R.id.mozac_browser_toolbar_security_indicator))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun menuButton() = onView(withId(R.id.icon))

private fun assertMenuButton() {
    menuButton()
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun tabsCounter() = onView(withId(R.id.counter_box))

private fun mediaPlayerPlayButton() =
    mDevice.findObject(
        UiSelector()
            .className("android.widget.Button")
            .text("Play")
    )

fun clickTabCrashedRestoreButton() {

    assertTrue(
        mDevice.findObject(UiSelector().resourceId("$packageName:id/restoreTabButton"))
            .waitForExists(waitingTime)
    )

    val tabCrashRestoreButton = mDevice.findObject(UiSelector().resourceIdMatches("$packageName:id/restoreTabButton"))
    tabCrashRestoreButton.click()
}
