@file:Suppress("DEPRECATION")

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.FeatureSettingsHelperDelegate
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.createCustomTabIntent
import org.mozilla.fenix.helpers.TestHelper.openAppFromExternalLink
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.longClickPageObject
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.notificationShade
import org.mozilla.fenix.ui.robots.openEditURLView
import org.mozilla.fenix.ui.robots.searchScreen

class CustomTabsTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer
    private val customMenuItem = "TestMenuItem"

    /* Updated externalLinks.html to v2.0,
       changed the hypertext reference to mozilla-mobile.github.io/testapp/downloads for "External link"
     */
    private val externalLinksPWAPage = "https://mozilla-mobile.github.io/testapp/v2.0/externalLinks.html"
    private val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

    @get: Rule
    val intentReceiverActivityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java,
        true,
        false,
    )

    private val featureSettingsHelper = FeatureSettingsHelperDelegate()

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
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun customTabsOpenExternalLinkTest() {
        val externalLinkURL = "https://mozilla-mobile.github.io/testapp/downloads"

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                externalLinksPWAPage.toUri().toString(),
                customMenuItem,
            ),
        )

        customTabScreen {
            waitForPageToLoad()
            clickPageObject(itemContainingText("External link"))
            waitForPageToLoad()
            verifyCustomTabToolbarTitle(externalLinkURL)
        }
    }

    @SmokeTest
    @Test
    fun customTabsSaveLoginTest() {
        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                loginPage.toUri().toString(),
                customMenuItem,
            ),
        )

        customTabScreen {
            waitForPageToLoad()
            fillAndSubmitLoginCredentials("mozilla", "firefox")
        }

        browserScreen {
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }

        openAppFromExternalLink(loginPage)

        browserScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            verifySavedLoginsSectionUsername("mozilla")
        }
    }

    @SmokeTest
    @Test
    fun customTabCopyToolbarUrlTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
                customMenuItem,
            ),
        )

        customTabScreen {
            longCLickAndCopyToolbarUrl()
        }

        openAppFromExternalLink(customTabPage.url.toString())

        navigationToolbar {
            openEditURLView()
        }

        searchScreen {
            clickClearButton()
            longClickToolbar()
            clickPasteText()
            verifyTypedToolbarText(customTabPage.url.toString())
        }
    }

    @SmokeTest
    @Test
    fun customTabShareTextTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
                customMenuItem,
            ),
        )

        customTabScreen {
            waitForPageToLoad()
        }

        browserScreen {
            longClickPageObject(itemContainingText("content"))
        }.clickShareSelectedText {
            verifyAndroidShareLayout()
        }
    }

    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1807289")
    @SmokeTest
    @Test
    fun customTabDownloadTest() {
        val customTabPage = "https://storage.googleapis.com/mobile_test_assets/test_app/downloads.html"
        val downloadFile = "web_icon.png"

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.toUri().toString(),
                customMenuItem,
            ),
        )

        customTabScreen {
            waitForPageToLoad()
        }

        browserScreen {
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadNotificationPopup()
        }
        mDevice.openNotification()
        notificationShade {
            verifySystemNotificationExists("Download completed")
        }
    }
}
