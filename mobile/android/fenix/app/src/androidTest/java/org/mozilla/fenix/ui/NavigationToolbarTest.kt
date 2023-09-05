/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.runWithSystemLocaleChanged
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import java.util.Locale

/**
 *  Tests for verifying basic functionality of browser navigation and page related interactions
 *
 *  Including:
 *  - Visiting a URL
 *  - Back and Forward navigation
 *  - Refresh
 *  - Find in page
 */

class NavigationToolbarTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityTestRule.withDefaultSettingsOverrides()

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/987326
    // Swipes the nav bar left/right to switch between tabs
    @SmokeTest
    @Test
    fun swipeToSwitchTabTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            swipeNavBarRight(secondWebPage.url.toString())
            verifyUrl(firstWebPage.url.toString())
            swipeNavBarLeft(firstWebPage.url.toString())
            verifyUrl(secondWebPage.url.toString())
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/987327
    // Because it requires changing system prefs, this test will run only on Debug builds
    @Test
    fun swipeToSwitchTabInRTLTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)
        val arabicLocale = Locale("ar", "AR")

        runWithSystemLocaleChanged(arabicLocale, activityTestRule) {
            navigationToolbar {
            }.enterURLAndEnterToBrowser(firstWebPage.url) {
            }.openTabDrawer {
            }.openNewTab {
            }.submitQuery(secondWebPage.url.toString()) {
                swipeNavBarLeft(secondWebPage.url.toString())
                verifyUrl(firstWebPage.url.toString())
                swipeNavBarRight(firstWebPage.url.toString())
                verifyUrl(secondWebPage.url.toString())
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2265279
    @SmokeTest
    @Test
    fun verifySecurePageSecuritySubMenuTest() {
        val defaultWebPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val defaultWebPageTitle = "Login_form"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.toUri()) {
        }.openSiteSecuritySheet {
            verifyQuickActionSheet(defaultWebPage, true)
            openSecureConnectionSubMenu(true)
            verifySecureConnectionSubMenu(defaultWebPageTitle, defaultWebPage, true)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2265280
    @SmokeTest
    @Test
    fun verifyInsecurePageSecuritySubMenuTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            waitForPageToLoad()
        }.openSiteSecuritySheet {
            verifyQuickActionSheet(defaultWebPage.url.toString(), false)
            openSecureConnectionSubMenu(false)
            verifySecureConnectionSubMenu(defaultWebPage.title, defaultWebPage.url.toString(), false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1661318
    @SmokeTest
    @Test
    fun verifyClearCookiesFromQuickSettingsTest() {
        val helpPageUrl = "mozilla.org"

        homeScreen {
        }.openThreeDotMenu {
        }.openHelp {
        }.openSiteSecuritySheet {
            clickQuickActionSheetClearSiteData()
            verifyClearSiteDataPrompt(helpPageUrl)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1360555
    @SmokeTest
    @Test
    fun goToHomeScreenTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            mDevice.waitForIdle()
        }.goToHomescreen {
            verifyHomeScreen()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2256552
    @SmokeTest
    @Test
    fun goToHomeScreenInPrivateModeTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
            togglePrivateBrowsingModeOnOff()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            mDevice.waitForIdle()
        }.goToHomescreen {
            verifyHomeScreen()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2260552
    @SmokeTest
    @Test
    fun totalCookieProtectionLearnMoreLinkTest() {
        activityTestRule.applySettingsExceptions {
            it.isTCPCFREnabled = true
        }

        val genericPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
            verifyCookiesProtectionHintIsDisplayed(true)
            clickPageObject(itemContainingText(getStringResource(R.string.tcp_cfr_learn_more)))
            verifyUrl("support.mozilla.org/en-US/kb/enhanced-tracking-protection-firefox-android")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1913589
    @Test
    fun totalCookieProtectionHintTest() {
        activityTestRule.applySettingsExceptions {
            it.isTCPCFREnabled = true
        }

        val genericPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
            verifyCookiesProtectionHintIsDisplayed(true)
            clickPageObject(itemWithDescription(getStringResource(R.string.mozac_cfr_dismiss_button_content_description)))
            verifyCookiesProtectionHintIsDisplayed(false)
        }
    }
}
