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
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.waitUntilSnackBarGone
import org.mozilla.focus.testAnnotations.SmokeTest

// Tests the First run onboarding screens
@RunWith(AndroidJUnit4ClassRunner::class)
class FirstRunTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    val mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = true)

    @Before
    fun startWebServer() {
        webServer = MockWebServer()
        webServer.start()
        featureSettingsHelper.setShieldIconCFREnabled(false)
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
            verifyTipText(getStringResource(R.string.tip_fresh_look))
            scrollLeftTipsCarousel()
            verifyTipText(getStringResource(R.string.tip_about_shortcuts))
            scrollLeftTipsCarousel()
            verifyTipText(getStringResource(R.string.tip_explain_allowlist3))
            scrollLeftTipsCarousel()
            verifyTipText(getStringResource(R.string.tip_disable_tracking_protection3))
            scrollLeftTipsCarousel()
            verifyTipText(getStringResource(R.string.tip_request_desktop2))
            scrollRightTipsCarousel()
            verifyTipText(getStringResource(R.string.tip_disable_tracking_protection3))
            scrollRightTipsCarousel()
            verifyTipText(getStringResource(R.string.tip_explain_allowlist3))
            scrollRightTipsCarousel()
            verifyTipText(getStringResource(R.string.tip_about_shortcuts))
            scrollRightTipsCarousel()
            verifyTipText(getStringResource(R.string.tip_fresh_look))
        }
    }

    @SmokeTest
    @Test
    fun verifyWhatsNewLinkFromTips() {
        homeScreen {
            skipFirstRun()
            closeSoftKeyboard()
            verifyTipsCarouselIsDisplayed(true)
            verifyTipText(getStringResource(R.string.tip_fresh_look))
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
        webServer.enqueue(createMockResponseFromAsset("tab1.html"))
        val pageUrl = webServer.url("tab1.html").toString()

        homeScreen {
            skipFirstRun()
            closeSoftKeyboard()
            verifyTipsCarouselIsDisplayed(true)
            verifyTipText(getStringResource(R.string.tip_fresh_look))
            scrollLeftTipsCarousel()
            verifyTipText(getStringResource(R.string.tip_about_shortcuts))
        }
        searchScreen {
        }.loadPage(pageUrl) {
            pressBack()
        }
        homeScreen {
            closeSoftKeyboard()
            waitUntilSnackBarGone()
            verifyTipText(getStringResource(R.string.tip_fresh_look))
        }
    }
}
