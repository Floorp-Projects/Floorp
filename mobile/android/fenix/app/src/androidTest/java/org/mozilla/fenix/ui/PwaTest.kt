package org.mozilla.fenix.ui

import androidx.core.net.toUri
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.Constants.PackageName.GMAIL_APP
import org.mozilla.fenix.helpers.Constants.PackageName.PHONE_APP
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.ui.robots.addToHomeScreen
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.pwaScreen
import org.mozilla.fenix.ui.robots.setPageObjectText

class PwaTest {
    /* Updated externalLinks.html to v2.0,
       changed the hypertext reference to mozilla-mobile.github.io/testapp/downloads for "External link"
     */
    private val externalLinksPWAPage = "https://mozilla-mobile.github.io/testapp/v2.0/externalLinks.html"
    private val emailLink = "mailto://example@example.com"
    private val phoneLink = "tel://1234567890"
    private val shortcutTitle = "TEST_APP"

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

    @SmokeTest
    @Test
    fun externalLinkPWATest() {
        val externalLinkURL = "https://mozilla-mobile.github.io/testapp/downloads"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(externalLinksPWAPage.toUri()) {
            waitForPageToLoad()
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            clickPageObject(itemContainingText("External link"))
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
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            clickPageObject(itemContainingText("Email link"))
            clickPageObject(itemWithResIdAndText("android:id/button1", "OPEN"))
            assertNativeAppOpens(GMAIL_APP, emailLink)
        }
    }

    @SmokeTest
    @Test
    fun telephoneLinkPWATest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(externalLinksPWAPage.toUri()) {
            waitForPageToLoad()
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            clickPageObject(itemContainingText("Telephone link"))
            clickPageObject(itemWithResIdAndText("android:id/button1", "OPEN"))
            assertNativeAppOpens(PHONE_APP, phoneLink)
        }
    }

    @SmokeTest
    @Test
    fun appLikeExperiencePWATest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(externalLinksPWAPage.toUri()) {
            waitForPageToLoad()
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
        }

        pwaScreen {
            verifyCustomTabToolbarIsNotDisplayed()
            verifyPwaActivityInCurrentTask()
        }
    }

    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1807273")
    @SmokeTest
    @Test
    fun saveLoginsInPWATest() {
        val pwaPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val shortcutTitle = "TEST_APP"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pwaPage.toUri()) {
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            mDevice.waitForIdle()
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
            TestHelper.openAppFromExternalLink(pwaPage)

            browserScreen {
            }.openThreeDotMenu {
            }.openSettings {
            }.openLoginsAndPasswordSubMenu {
            }.openSavedLogins {
                verifySecurityPromptForLogins()
                tapSetupLater()
                verifySavedLoginsSectionUsername("mozilla")
            }

            addToHomeScreen {
            }.searchAndOpenHomeScreenShortcut(shortcutTitle) {
                verifyPrefilledPWALoginCredentials("mozilla", shortcutTitle)
            }
        }
    }
}
