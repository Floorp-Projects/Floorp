/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.espresso.Espresso.closeSoftKeyboard
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.exitToTop
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.getTargetContext
import org.mozilla.focus.testAnnotations.SmokeTest

// This test visits each About page and checks whether some essential elements are being displayed
@RunWith(AndroidJUnit4ClassRunner::class)
class MozillaSupportPagesTest {
    private val featureSettingsHelper = FeatureSettingsHelper()
    private val firstTipText = getStringResource(R.string.tip_fresh_look)
    private val secondTipText = getStringResource(R.string.tip_about_shortcuts)
    private val thirdTipText = getStringResource(R.string.tip_disable_tracking_protection3)
    private val fourthTipText = getStringResource(R.string.tip_explain_allowlist3)
    private val fifthTipText = getStringResource(R.string.tip_request_desktop2)

    @get: Rule
    val mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
    }

    @After
    fun tearDown() {
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun openMenuWhatsNewPageTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.clickWhatsNewLink {
            verifyPageURL("support.mozilla.org")
        }
    }

    @SmokeTest
    @Test
    fun openMenuHelpPageTest() {
        homeScreen {
        }.openMainMenu {
        }.clickHelpPageLink {
            verifyPageURL("what-firefox-focus-android")
        }
    }

    @SmokeTest
    @Test
    fun openAboutPageTest() {
        // Go to settings "About" page
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openMozillaSettingsMenu {
        }.openAboutPage {
            verifyVersionNumbers()
        }.openAboutPageLearnMoreLink() {
            verifyPageURL("www.mozilla.org/en-US/about/manifesto/")
        }
    }

    @SmokeTest
    @Test
    fun openMozillaSettingsHelpLinkTest() {
        // Go to settings "About" page
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openMozillaSettingsMenu {
        }.openHelpLink {
            verifyPageURL("what-firefox-focus-android")
        }
    }

    @SmokeTest
    @Test
    fun openYourRightsPageTest() {
        val yourRightsString = getTargetContext.getString(
            R.string.your_rights_content1,
            getTargetContext.getString(R.string.app_name),
            "Mozilla Public License"
        )

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openMozillaSettingsMenu {
        }.openYourRightsPage {
            verifyPageContent(yourRightsString)
        }
    }

    @SmokeTest
    @Test
    fun openPrivacyNoticeTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openMozillaSettingsMenu {
        }.openPrivacyNotice {
            verifyPageURL("privacy/firefox-focus")
        }
    }

    @SmokeTest
    @Test
    fun turnOffAndOnHomeScreenTipsTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openMozillaSettingsMenu {
            switchHomeScreenTips()
            exitToTop()
        }
        homeScreen {
            verifyTipsCarouselIsDisplayed(false)
        }.openMainMenu {
        }.openSettings {
        }.openMozillaSettingsMenu {
            switchHomeScreenTips()
            exitToTop()
        }
        homeScreen {
            closeSoftKeyboard()
            verifyTipsCarouselIsDisplayed(true)
            verifyTipText(firstTipText)
            scrollLeftTipsCarousel()
            verifyTipText(secondTipText)
            scrollLeftTipsCarousel()
            verifyTipText(thirdTipText)
            scrollLeftTipsCarousel()
            verifyTipText(fourthTipText)
            scrollLeftTipsCarousel()
            verifyTipText(fifthTipText)
            scrollRightTipsCarousel()
            verifyTipText(fourthTipText)
            scrollRightTipsCarousel()
            verifyTipText(thirdTipText)
            scrollRightTipsCarousel()
            verifyTipText(secondTipText)
            scrollRightTipsCarousel()
            verifyTipText(firstTipText)
        }
    }
}
