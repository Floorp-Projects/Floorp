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
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.waitUntilSnackBarGone
import org.mozilla.focus.testAnnotations.SmokeTest

// Tests the First run onboarding screens
@RunWith(AndroidJUnit4ClassRunner::class)
class FirstRunDialogueTest {
    private lateinit var webServer: MockWebServer

    @get: Rule
    val mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = true)

    @Before
    fun startWebServer() {
        webServer = MockWebServer()
        webServer.start()
    }

    @After
    fun stopWebServer() {
        webServer.shutdown()
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
    @Ignore("This test should be updated since kotlin extensions were migrated to view binding")
    // https://github.com/mozilla-mobile/focus-android/issues/5767
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
    @Ignore("This test should be updated since kotlin extensions were migrated to view binding")
    // https://github.com/mozilla-mobile/focus-android/issues/5767
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
    @Ignore("This test should be updated since kotlin extensions were migrated to view binding")
    // https://github.com/mozilla-mobile/focus-android/issues/5767
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
    @Ignore("This test should be updated since kotlin extensions were migrated to view binding")
    // https://github.com/mozilla-mobile/focus-android/issues/5767
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
    @Ignore("This test should be updated since kotlin extensions were migrated to view binding")
    // https://github.com/mozilla-mobile/focus-android/issues/5767
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
    @Ignore("This test should be updated since kotlin extensions were migrated to view binding")
    // https://github.com/mozilla-mobile/focus-android/issues/5767
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
