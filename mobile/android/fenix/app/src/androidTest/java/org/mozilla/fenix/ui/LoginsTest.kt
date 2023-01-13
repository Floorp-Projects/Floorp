package org.mozilla.fenix.ui

import android.os.Build
import android.view.autofill.AutofillManager
import androidx.core.net.toUri
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class LoginsTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        TestHelper.appContext.settings().userOptOutOfReEngageCookieBannerDialog = true
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }

        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.R) {
            val autofillManager: AutofillManager =
                TestHelper.appContext.getSystemService(AutofillManager::class.java)
            autofillManager.disableAutofillServices()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun saveLoginFromPromptTest() {
        val saveLoginTest =
            TestAssetHelper.getSaveLoginAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            verifySaveLoginPromptIsShown()
            // Click save to save the login
            saveLoginFromPrompt("Save")
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openSettings {
            TestHelper.scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
            verifyDefaultView()
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that the login appears correctly
            verifySavedLoginFromPrompt("test@example.com")
        }
    }

    @SmokeTest
    @Test
    fun openWebsiteForSavedLoginTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val originWebsite = "mozilla-mobile.github.io"
        val userName = "test"
        val password = "pass"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            fillAndSubmitLoginCredentials(userName, password)
            saveLoginFromPrompt("Save")
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            viewSavedLoginDetails(userName)
        }.goToSavedWebsite {
            verifyUrl(originWebsite)
        }
    }

    @Test
    fun neverSaveLoginFromPromptTest() {
        val saveLoginTest = TestAssetHelper.getSaveLoginAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            verifySaveLoginPromptIsShown()
            // Don't save the login, add to exceptions
            saveLoginFromPrompt("Never save")
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
            verifyDefaultView()
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that the login list is empty
            verifyNotSavedLoginFromPrompt()
        }.goBack {
        }.openLoginExceptions {
            // Verify localhost was added to exceptions list
            verifyLocalhostExceptionAdded()
        }
    }

    @SmokeTest
    @Test
    fun updateSavedLoginTest() {
        val saveLoginTest =
            TestAssetHelper.getSaveLoginAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            verifySaveLoginPromptIsShown()
            // Click Save to save the login
            saveLoginFromPrompt("Save")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            enterPassword("test")
            verifyUpdateLoginPromptIsShown()
            // Click Update to change the saved password
            saveLoginFromPrompt("Update")
        }.openThreeDotMenu {
        }.openSettings {
            TestHelper.scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that the login appears correctly
            verifySavedLoginFromPrompt("test@example.com")
            viewSavedLoginDetails("test@example.com")
            revealPassword()
            verifyPasswordSaved("test") // failing here locally
        }
    }

    @SmokeTest
    @Test
    fun verifyMultipleLoginsSelectionsTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            fillAndSubmitLoginCredentials("mozilla", "firefox")
            saveLoginFromPrompt("Save")
            fillAndSubmitLoginCredentials("firefox", "mozilla")
            saveLoginFromPrompt("Save")
            clearUserNameLoginCredential()
            clickSuggestedLoginsButton()
            verifySuggestedUserName("firefox")
            verifySuggestedUserName("mozilla")
            clickLoginSuggestion("mozilla")
            verifyPrefilledLoginCredentials("mozilla")
        }
    }
}
