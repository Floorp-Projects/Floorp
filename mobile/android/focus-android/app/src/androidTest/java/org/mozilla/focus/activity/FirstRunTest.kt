/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.espresso.Espresso.closeSoftKeyboard
import androidx.test.espresso.Espresso.pressBack
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.TestAssetHelper.getGenericTabAsset
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.waitUntilSnackBarGone
import org.mozilla.focus.testAnnotations.SmokeTest

// Tests the First run onboarding screens
@RunWith(AndroidJUnit4ClassRunner::class)
class FirstRunTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()
    private val firstTipText = getStringResource(R.string.tip_fresh_look)
    private val secondTipText = getStringResource(R.string.tip_about_shortcuts)
    private val thirdTipText = getStringResource(R.string.tip_explain_allowlist3)
    private val fourthTipText = getStringResource(R.string.tip_disable_tracking_protection3)
    private val fifthTipText = getStringResource(R.string.tip_request_desktop2)

    @get: Rule
    val mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = true)

    @Before
    fun startWebServer() {
        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
    }

    @After
    fun stopWebServer() {
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @Test
    fun firstRunOnboardingTest() {
        homeScreen {
            verifyOnboardingFirstSlide()
            clickOnboardingNextBtn()
            verifyOnboardingSecondSlide()
            clickOnboardingNextBtn()
            verifyOnboardingThirdSlide()
            clickOnboardingNextBtn()
            verifyOnboardingLastSlide()
            clickOnboardingFinishBtn()
            verifyEmptySearchBar()
        }
    }

    @Test
    fun skipFirstRunOnboardingTest() {
        homeScreen {
            verifyOnboardingFirstSlide()
            clickOnboardingNextBtn()
            verifyOnboardingSecondSlide()
            skipFirstRun()
            verifyEmptySearchBar()
        }
    }

    @SmokeTest
    @Test
    fun homeScreenTipsTest() {
        // Scrolling left/right through the tips carousel to check the tips displaying order
        homeScreen {
            skipFirstRun()
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

    @SmokeTest
    @Test
    fun verifyWhatsNewLinkFromTips() {
        homeScreen {
            skipFirstRun()
            closeSoftKeyboard()
            verifyTipsCarouselIsDisplayed(true)
            verifyTipText(firstTipText)
        }.clickLinkFromTips("Read more") {
            verifyPageURL("whats-new-firefox-focus-android")
        }
    }

    @SmokeTest
    @Test
    fun verifyAllowListLinkFromTips() {
        homeScreen {
            skipFirstRun()
            closeSoftKeyboard()
            verifyTipsCarouselIsDisplayed(true)
            scrollLeftTipsCarousel()
            scrollLeftTipsCarousel()
        }.clickLinkFromTips("Add it to the Allowlist in Settings") {
            verifyPageURL("add-trusted-websites-your-allow-list-firefox-focus")
        }
    }

    @SmokeTest
    @Test
    fun verifySwitchToDesktopSiteLinkFromTips() {
        homeScreen {
            skipFirstRun()
            closeSoftKeyboard()
            verifyTipsCarouselIsDisplayed(true)
            scrollLeftTipsCarousel()
            scrollLeftTipsCarousel()
            scrollLeftTipsCarousel()
            scrollLeftTipsCarousel()
        }.clickLinkFromTips("Switch on Desktop Site") {
            verifyPageURL("switch-desktop-view-firefox-focus-android")
        }
    }

    @SmokeTest
    @Test
    fun firstTipIsAlwaysDisplayedTest() {
        val pageUrl = getGenericTabAsset(webServer, 1).url

        homeScreen {
            skipFirstRun()
            closeSoftKeyboard()
            verifyTipsCarouselIsDisplayed(true)
            verifyTipText(firstTipText)
            scrollLeftTipsCarousel()
            verifyTipText(secondTipText)
        }
        searchScreen {
        }.loadPage(pageUrl) {
            pressBack()
        }
        homeScreen {
            closeSoftKeyboard()
            waitUntilSnackBarGone()
            verifyTipText(firstTipText)
        }
    }
}
