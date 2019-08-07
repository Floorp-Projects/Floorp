/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.uiautomator.UiDevice
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.HomeActivityTestRule

import androidx.test.platform.app.InstrumentationRegistry
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
            verifyOpenTabsHeader()
            verifyAddTabButton()
            verifyNoTabsOpenedHeader()
            verifyNoTabsOpenedText()
            verifyCollectionsHeader()
            verifyNoCollectionsHeader()
            verifyNoCollectionsText()
            verifyHomeToolbar()
            verifyHomeComponent()
        }
    }

    @Test
    fun firstRunHomeScreenItemsTest() {
        homeScreen {
            verifyHomeScreen()
            verifyNavigationToolbar()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenu()
            verifyHomeWordmark()

            verifyWelcomeHeader()

            verifyGetTheMostHeader()
            verifyAccountsSignInButton()
            verifyGetToKnowHeader()

            swipeUpToDismissFirstRun()

            verifyChooseThemeHeader()
            verifyChooseThemeText()
            verifyLightThemeToggle()
            verifyLightThemeDescription()
            verifyDarkThemeToggle()
            verifyDarkThemeDescription()
            verifyAutomaticThemeToggle()
            verifyAutomaticThemeDescription()

            verifyProtectYourselfHeader()
            verifyTrackingProtectionToggle()
            verifyProtectYourselfText()

            verifyBrowsePrivatelyHeader()
            verifyBrowsePrivatelyText()
            verifyYourPrivacyHeader()
            verifyYourPrivacyText()

            verifyPrivacyNoticeButton()
            verifyStartBrowsingButton()
        }
    }
}
