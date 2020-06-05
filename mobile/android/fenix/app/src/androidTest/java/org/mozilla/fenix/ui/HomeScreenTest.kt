/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.uiautomator.UiDevice
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.HomeActivityTestRule
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Until
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.ui.robots.PRIVATE_SESSION_MESSAGE
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the presence of home screen and first-run homescreen elements
 *
 *  Note: For private browsing, navigation bar and tabs see separate test class
 *
 */

class HomeScreenTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Test
    fun homeScreenItemsTest() {
        homeScreen { }.dismissOnboarding()

        homeScreen {
            verifyHomeScreen()
            verifyNavigationToolbar()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenu()
            verifyHomeWordmark()
            verifyTabButton()
            verifyCollectionsHeader()
            verifyHomeToolbar()
            verifyHomeComponent()

            // Verify Top Sites
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs("Wikipedia")
            verifyExistingTopSitesTabs("YouTube")
            verifyExistingTopSitesTabs("Top Articles")
        }
    }

    @Test
    fun firstRunScreenTest() {
        homeScreen {
            verifyHomeScreen()
            verifyNavigationToolbar()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenu()
            verifyHomeWordmark()

            verifyWelcomeHeader()
            // Sign in to Firefox
            verifyGetTheMostHeader()
            verifyAccountsSignInButton()

            // Intro to other sections
            verifyGetToKnowHeader()

            // See What's new
            scrollToElementByText("See what’s new")
            verifyWhatsNewHeader()
            verifyWhatsNewLink()

            // Automatic privacy
            scrollToElementByText("Automatic privacy")
            verifyAutomaticPrivacyfHeader()
            verifyTrackingProtectionToggle()
            verifyAutomaticPrivacyText()

            /* Check disable due to Firebase failures on Pixel 2 API 28
            // Choose your theme
            verifyChooseThemeHeader()
            verifyChooseThemeText()
            verifyDarkThemeDescription()
            verifyDarkThemeToggle()
            verifyLightThemeDescription()
            verifyLightThemeToggle()

            // Browse privately
            scrollToElementByText("Open Settings")
            verifyBrowsePrivatelyHeader()
            verifyBrowsePrivatelyText()
             */

            swipeToBottom()

            // Take a position
            scrollToElementByText("Take a position")
            verifyTakePositionHeader()
            verifyTakePositionElements()

            // Your privacy
            scrollToElementByText("Your privacy")
            verifyYourPrivacyHeader()
            verifyYourPrivacyText()
            verifyPrivacyNoticeButton()

            // Start Browsing
            swipeToBottom()
            verifyStartBrowsingButton()
        }
    }

    @Test
    fun privateModeScreenItemsTest() {
        homeScreen { }.dismissOnboarding()
        homeScreen { }.togglePrivateBrowsingMode()

        homeScreen {
            verifyHomeScreen()
            verifyNavigationToolbar()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenu()
            verifyHomeWordmark()
            verifyTabButton()
            verifyPrivateSessionMessage()
            verifyHomeToolbar()
            verifyHomeComponent()
        }.openCommonMythsLink {
            verifyUrl("common-myths-about-private-browsing")
            mDevice.pressBack()
        }

        homeScreen {
            // To deal with the race condition where multiple "add tab" buttons are present,
            // we need to wait until previous HomeFragment View objects are gone.
            mDevice.waitNotNull(Until.gone(By.text(PRIVATE_SESSION_MESSAGE)), waitingTime)
            verifyHomeScreen()
            verifyNavigationToolbar()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenu()
            verifyHomeWordmark()
            verifyTabButton()
            verifyPrivateSessionMessage()
            verifyHomeToolbar()
            verifyHomeComponent()
        }
    }
}
