package org.mozilla.fenix.ui

import androidx.core.net.toUri
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.Constants.PackageName.GMAIL_APP
import org.mozilla.fenix.helpers.Constants.PackageName.PHONE_APP
import org.mozilla.fenix.helpers.FeatureSettingsHelper
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class PwaTest {
    private val featureSettingsHelper = FeatureSettingsHelper()
    /* Updated externalLinks.html to v2.0,
       changed the hypertext reference to mozilla-mobile.github.io/testapp/downloads for "External link"
     */
    private val externalLinksPWAPage = "https://mozilla-mobile.github.io/testapp/v2.0/externalLinks.html"
    private val emailLink = "mailto://example@example.com"
    private val phoneLink = "tel://1234567890"
    private val shortcutTitle = "TEST_APP"

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule()

    @Before
    fun setUp() {
        featureSettingsHelper.disablePwaCFR(true)
    }

    @After
    fun tearDown() {
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun externalLinkPWATest() {
        val externalLinkURL = "https://mozilla-mobile.github.io/testapp/downloads"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(externalLinksPWAPage.toUri()) {
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            clickLinkMatchingText("External link")
        }

        customTabScreen {
            verifyCustomTabToolbarTitle(externalLinkURL)
        }
    }

    @SmokeTest
    @Test
    fun emailLinkPWATest() {

        navigationToolbar {
        }.enterURLAndEnterToBrowser(externalLinksPWAPage.toUri()) {
            waitForPageToLoad()
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            clickLinkMatchingText("Email link")
            assertNativeAppOpens(GMAIL_APP, emailLink)
        }
    }

    @SmokeTest
    @Test
    fun telephoneLinkPWATest() {

        navigationToolbar {
        }.enterURLAndEnterToBrowser(externalLinksPWAPage.toUri()) {
            waitForPageToLoad()
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            clickLinkMatchingText("Telephone link")
            assertNativeAppOpens(PHONE_APP, phoneLink)
        }
    }
}
