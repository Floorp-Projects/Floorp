/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.core.net.toUri
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class SettingsHTTPSOnlyModeTest : TestSetup() {
    private val httpPageUrl = "http://example.com/"
    private val httpsPageUrl = "https://example.com/"
    private val insecureHttpPage = "http.badssl.com"

    // "HTTPs not supported" error page contents:
    private val httpsOnlyErrorTitle = "Secure site not available"
    private val httpsOnlyErrorMessage = "Most likely, the website simply does not support HTTPS."
    private val httpsOnlyErrorMessage2 = "However, it’s also possible that an attacker is involved. If you continue to the website, you should not enter any sensitive info. If you continue, HTTPS-Only mode will be turned off temporarily for the site."
    private val httpsOnlyContinueButton = "Continue to HTTP Site"
    private val httpsOnlyBackButton = "Go Back (Recommended)"

    @get:Rule
    val activityTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule.withDefaultSettingsOverrides(),
        ) { it.activity }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1724825
    @Test
    fun httpsOnlyModeMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openHttpsOnlyModeMenu {
            verifyHttpsOnlyModeMenuHeader()
            verifyHttpsOnlyModeSummary()
            verifyHttpsOnlyModeIsEnabled(false)
            verifyHttpsOnlyModeOptionsEnabled(false)
            verifyHttpsOnlyOptionSelected(
                allTabsOptionSelected = false,
                privateTabsOptionSelected = false,
            )
            clickHttpsOnlyModeSwitch()
            verifyHttpsOnlyModeIsEnabled(true)
            verifyHttpsOnlyModeOptionsEnabled(true)
            verifyHttpsOnlyOptionSelected(
                allTabsOptionSelected = true,
                privateTabsOptionSelected = false,
            )
        }.goBack {
            verifySettingsToolbar()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1724827
    @SmokeTest
    @Test
    fun httpsOnlyModeEnabledInNormalBrowsingTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openHttpsOnlyModeMenu {
            clickHttpsOnlyModeSwitch()
            verifyHttpsOnlyOptionSelected(
                allTabsOptionSelected = true,
                privateTabsOptionSelected = false,
            )
        }.goBack {
            verifySettingsOptionSummary("HTTPS-Only Mode", "On in all tabs")
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(httpPageUrl.toUri()) {
            waitForPageToLoad()
        }.openNavigationToolbar {
            verifyUrl(httpsPageUrl)
        }.enterURLAndEnterToBrowser(insecureHttpPage.toUri()) {
            verifyPageContent(httpsOnlyErrorTitle)
            verifyPageContent(httpsOnlyErrorMessage)
            verifyPageContent(httpsOnlyErrorMessage2)
            verifyPageContent(httpsOnlyBackButton)
            clickPageObject(itemContainingText(httpsOnlyBackButton))
            verifyPageContent("Example Domain")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(insecureHttpPage.toUri()) {
            clickPageObject(itemContainingText(httpsOnlyContinueButton))
            verifyPageContent("http.badssl.com")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2091057
    @Test
    fun httpsOnlyModeExceptionPersistsForCurrentSessionTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openHttpsOnlyModeMenu {
            clickHttpsOnlyModeSwitch()
            verifyHttpsOnlyOptionSelected(
                allTabsOptionSelected = true,
                privateTabsOptionSelected = false,
            )
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(insecureHttpPage.toUri()) {
            verifyPageContent(httpsOnlyErrorTitle)
            clickPageObject(itemContainingText(httpsOnlyContinueButton))
            verifyPageContent("http.badssl.com")
        }.openTabDrawer(activityTestRule) {
            closeTab()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(insecureHttpPage.toUri()) {
            verifyPageContent("http.badssl.com")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1724828
    @Test
    fun httpsOnlyModeEnabledOnlyInPrivateBrowsingTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openHttpsOnlyModeMenu {
            clickHttpsOnlyModeSwitch()
            selectHttpsOnlyModeOption(
                allTabsOptionSelected = false,
                privateTabsOptionSelected = true,
            )
        }.goBack {
            verifySettingsOptionSummary("HTTPS-Only Mode", "On in private tabs")
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(insecureHttpPage.toUri()) {
            verifyPageContent("http.badssl.com")
        }.goToHomescreen {
        }.togglePrivateBrowsingMode()
        navigationToolbar {
        }.enterURLAndEnterToBrowser(httpPageUrl.toUri()) {
            waitForPageToLoad()
        }.openNavigationToolbar {
            verifyUrl(httpsPageUrl)
        }.enterURLAndEnterToBrowser(insecureHttpPage.toUri()) {
            verifyPageContent(httpsOnlyErrorTitle)
            verifyPageContent(httpsOnlyErrorMessage)
            verifyPageContent(httpsOnlyErrorMessage2)
            verifyPageContent(httpsOnlyBackButton)
            clickPageObject(itemContainingText(httpsOnlyBackButton))
            verifyPageContent("Example Domain")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(insecureHttpPage.toUri()) {
            clickPageObject(itemContainingText(httpsOnlyContinueButton))
            verifyPageContent("http.badssl.com")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2091058
    @Test
    fun turnOffHttpsOnlyModeTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openHttpsOnlyModeMenu {
            clickHttpsOnlyModeSwitch()
            verifyHttpsOnlyOptionSelected(
                allTabsOptionSelected = true,
                privateTabsOptionSelected = false,
            )
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(httpPageUrl.toUri()) {
            waitForPageToLoad()
        }.openNavigationToolbar {
            verifyUrl(httpsPageUrl)
        }.goBackToBrowserScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openHttpsOnlyModeMenu {
            clickHttpsOnlyModeSwitch()
            verifyHttpsOnlyModeIsEnabled(false)
        }.goBack {
            verifySettingsOptionSummary("HTTPS-Only Mode", "Off")
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(httpPageUrl.toUri()) {
            waitForPageToLoad()
        }.openNavigationToolbar {
            verifyUrl(httpPageUrl)
        }
    }
}
