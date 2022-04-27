@file:Suppress("DEPRECATION")

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import androidx.test.rule.ActivityTestRule
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.createCustomTabIntent
import org.mozilla.fenix.helpers.TestHelper.openAppFromExternalLink
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.mDevice
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.notificationShade
import org.mozilla.fenix.ui.robots.openEditURLView
import org.mozilla.fenix.ui.robots.searchScreen

class CustomTabsTest {
    private lateinit var mockWebServer: MockWebServer
    private val customMenuItem = "TestMenuItem"
    private val externalLinksPWAPage = "https://mozilla-mobile.github.io/testapp/externalLinks.html"
    private val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule()

    @get: Rule
    val intentReceiverActivityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java, true, false
    )

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @SmokeTest
    @Test
    fun customTabsOpenExternalLinkTest() {

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                externalLinksPWAPage.toUri().toString(),
                customMenuItem
            )
        )

        customTabScreen {
            waitForPageToLoad()
            clickLinkMatchingText("External link")
            waitForPageToLoad()
            verifyCustomTabToolbarTitle("Google")
        }
    }

    @SmokeTest
    @Test
    fun customTabsSaveLoginTest() {

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                loginPage.toUri().toString(),
                customMenuItem
            )
        )

        customTabScreen {
            waitForPageToLoad()
            fillAndSubmitLoginCredentials("mozilla", "firefox")
        }

        browserScreen {
            verifySaveLoginPromptIsDisplayed()
            saveLoginFromPrompt("Save")
        }

        openAppFromExternalLink(loginPage)

        browserScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            verifySavedLoginFromPrompt("mozilla")
        }
    }

    @SmokeTest
    @Test
    fun customTabCopyToolbarUrlTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
                customMenuItem
            )
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
            verifyPastedToolbarText(customTabPage.url.toString())
        }
    }

    @SmokeTest
    @Test
    fun customTabShareTextTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
                customMenuItem
            )
        )

        customTabScreen {
            waitForPageToLoad()
        }

        browserScreen {
            longClickMatchingText("content")
        }.clickShareSelectedText {
            verifyAndroidShareLayout()
        }
    }

    @SmokeTest
    @Test
    fun customTabDownloadTest() {
        val customTabPage = "https://storage.googleapis.com/mobile_test_assets/test_app/downloads.html"
        val downloadFile = "web_icon.png"

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.toUri().toString(),
                customMenuItem
            )
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
