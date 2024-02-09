/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.os.Build
import android.view.autofill.AutofillManager
import androidx.core.net.toUri
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.helpers.TestHelper.waitForAppWindowToBeUpdated
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clearTextFieldItem
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.setPageObjectText

/**
 * Tests for verifying:
 * - the Logins and Passwords menu and sub-menus.
 * - save login prompts.
 * - saving logins based on the user's preferences.
 */
class LoginsTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule =
        HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    @Before
    fun setUp() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2092713
    // Tests the Logins and passwords menu items and default values
    @Test
    fun loginsAndPasswordsSettingsItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            // Necessary to scroll a little bit for all screen sizes
            scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
            verifyDefaultView()
            verifyAutofillInFirefoxToggle(true)
            verifyAutofillLoginsInOtherAppsToggle(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/517816
    // Tests only for initial state without signing in.
    // For tests after signing in, see SyncIntegration test suite
    @Test
    fun verifySavedLoginsListTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            // Necessary to scroll a little bit for all screen sizes
            scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
            verifyDefaultView()
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that logins list is empty
            verifyEmptySavedLoginsListView()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2092925
    @Test
    fun verifySyncLoginsOptionsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            // Necessary to scroll a little bit for all screen sizes
            scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
        }.openSyncLogins {
            verifyReadyToScanOption()
            verifyUseEmailOption()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/523839
    @Test
    fun saveLoginFromPromptTest() {
        val saveLoginTest =
            TestAssetHelper.getSaveLoginAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSaveLoginsAndPasswordsOptions {
            verifySaveLoginsOptionsView()
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            clickSubmitLoginButton()
            verifySaveLoginPromptIsDisplayed()
            // Click save to save the login
            clickPageObject(itemWithText("Save"))
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openSettings {
            scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
            verifyDefaultView()
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that the login appears correctly
            verifySavedLoginsSectionUsername("test@example.com")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/960412
    @Test
    fun openLoginWebsiteInBrowserTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val originWebsite = "mozilla-mobile.github.io"
        val userName = "test"
        val password = "pass"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), userName)
            setPageObjectText(itemWithResId("password"), password)
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
            mDevice.waitForIdle()
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/517817
    @Test
    fun neverSaveLoginFromPromptTest() {
        val saveLoginTest = TestAssetHelper.getSaveLoginAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            clickSubmitLoginButton()
            // Don't save the login, add to exceptions
            clickPageObject(itemWithText("Never save"))
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
            verifyDefaultView()
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that the login list is empty
            verifyEmptySavedLoginsListView()
            verifyNotSavedLoginFromPrompt()
        }.goBack {
        }.openLoginExceptions {
            // Verify localhost was added to exceptions list
            verifyLocalhostExceptionAdded()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1508171
    @SmokeTest
    @Test
    fun verifyUpdatedLoginIsSavedTest() {
        val saveLoginTest =
            TestAssetHelper.getSaveLoginAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            clickSubmitLoginButton()
            verifySaveLoginPromptIsDisplayed()
            // Click Save to save the login
            clickPageObject(itemWithText("Save"))
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            enterPassword("test")
            mDevice.waitForIdle()
            clickSubmitLoginButton()
            verifySaveLoginPromptIsDisplayed()
            // Click Update to change the saved password
            clickPageObject(itemWithText("Update"))
        }.openThreeDotMenu {
        }.openSettings {
            scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that the login appears correctly
            verifySavedLoginsSectionUsername("test@example.com")
            viewSavedLoginDetails("test@example.com")
            revealPassword()
            verifyPasswordSaved("test") // failing here locally
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1049971
    @SmokeTest
    @Test
    fun verifyMultipleLoginsSelectionsTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/v2.0/loginForm.html"
        val firstUser = "mozilla"
        val firstPass = "firefox"
        val secondUser = "fenix"
        val secondPass = "pass"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), firstUser)
            waitForAppWindowToBeUpdated()
            setPageObjectText(itemWithResId("password"), firstPass)
            waitForAppWindowToBeUpdated()
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
            setPageObjectText(itemWithResId("username"), secondUser)
            waitForAppWindowToBeUpdated()
            setPageObjectText(itemWithResId("password"), secondPass)
            waitForAppWindowToBeUpdated()
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
            clearTextFieldItem(itemWithResId("username"))
            clickSuggestedLoginsButton()
            verifySuggestedUserName(firstUser)
            verifySuggestedUserName(secondUser)
            clickPageObject(
                itemWithResIdAndText(
                    "$packageName:id/username",
                    firstUser,
                ),
            )
            clickPageObject(itemWithResId("togglePassword"))
            verifyPrefilledLoginCredentials(firstUser, firstPass, true)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/875849
    @Test
    fun verifyEditLoginsViewTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val originWebsite = "mozilla-mobile.github.io"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            viewSavedLoginDetails(originWebsite)
            clickThreeDotButton(activityTestRule)
            clickEditLoginButton()
            setNewPassword("fenix")
            saveEditedLogin()
            revealPassword()
            verifyPasswordSaved("fenix")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/875851
    @Test
    fun verifyEditedLoginsAreSavedTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/v2.0/loginForm.html"
        val originWebsite = "mozilla-mobile.github.io"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            viewSavedLoginDetails(originWebsite)
            clickThreeDotButton(activityTestRule)
            clickEditLoginButton()
            setNewUserName("android")
            setNewPassword("fenix")
            saveEditedLogin()
        }

        exitMenu()

        browserScreen {
        }.openThreeDotMenu {
        }.refreshPage {
            waitForPageToLoad()
            clickPageObject(itemWithResId("togglePassword"))
            verifyPrefilledLoginCredentials("android", "fenix", true)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2266452
    @Test
    fun verifyLoginWithNoUserNameCanNotBeSavedTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val originWebsite = "mozilla-mobile.github.io"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            viewSavedLoginDetails(originWebsite)
            clickThreeDotButton(activityTestRule)
            clickEditLoginButton()
            clickClearUserNameButton()
            verifyUserNameRequiredErrorMessage()
            verifySaveLoginButtonIsEnabled(false)
            clickGoBackButton()
            verifyLoginItemUsername("mozilla")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2266453
    @Test
    fun verifyLoginWithoutPasswordCanNotBeSavedTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val originWebsite = "mozilla-mobile.github.io"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            viewSavedLoginDetails(originWebsite)
            clickThreeDotButton(activityTestRule)
            clickEditLoginButton()
            clickClearPasswordButton()
            verifyPasswordRequiredErrorMessage()
            verifySaveLoginButtonIsEnabled(false)
            clickGoBackButton()
            revealPassword()
            verifyPasswordSaved("firefox")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/876531
    @Test
    fun verifyEditModeDismissalDoesNotSaveLoginCredentialsTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val originWebsite = "mozilla-mobile.github.io"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            viewSavedLoginDetails(originWebsite)
            clickThreeDotButton(activityTestRule)
            clickEditLoginButton()
            setNewUserName("android")
            setNewPassword("fenix")
            clickGoBackButton()
            verifyLoginItemUsername("mozilla")
            revealPassword()
            verifyPasswordSaved("firefox")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/876532
    @Test
    fun verifyDeleteLoginButtonTest() {
        val loginPage = TestAssetHelper.getSaveLoginAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.url) {
            clickSubmitLoginButton()
            clickPageObject(itemWithText("Save"))
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            viewSavedLoginDetails("test@example.com")
            clickThreeDotButton(activityTestRule)
            clickDeleteLoginButton()
            verifyLoginDeletionPrompt()
            clickCancelDeleteLogin()
            verifyLoginItemUsername("test@example.com")
            viewSavedLoginDetails("test@example.com")
            clickThreeDotButton(activityTestRule)
            clickDeleteLoginButton()
            verifyLoginDeletionPrompt()
            clickConfirmDeleteLogin()
            // The account remains displayed, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1812431
            // verifyNotSavedLoginFromPrompt()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/517818
    @SmokeTest
    @Test
    fun verifyNeverSaveLoginOptionTest() {
        val loginPage = TestAssetHelper.getSaveLoginAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSaveLoginsAndPasswordsOptions {
            clickNeverSaveOption()
        }.goBack {
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.url) {
            clickSubmitLoginButton()
            verifySaveLoginPromptIsNotDisplayed()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/517819
    @Test
    fun verifyAutofillToggleTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/v2.0/loginForm.html"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }.openTabDrawer {
            closeTab()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            verifyPrefilledLoginCredentials("mozilla", "firefox", true)
        }.openTabDrawer {
            closeTab()
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
            verifyAutofillInFirefoxToggle(true)
            clickAutofillInFirefoxOption()
            verifyAutofillInFirefoxToggle(false)
        }.goBack {
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            verifyPrefilledLoginCredentials("mozilla", "firefox", false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/593768
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1812995")
    @Test
    fun doNotSaveOptionWillNotUpdateALoginTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/v2.0/loginForm.html"
        val originWebsite = "mozilla-mobile.github.io"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }.openTabDrawer {
            closeTab()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            clickPageObject(itemWithResId("togglePassword"))
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "fenix")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Don’t update"))
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            viewSavedLoginDetails(originWebsite)
            revealPassword()
            verifyPasswordSaved("firefox")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2090455
    @Test
    fun searchLoginsByUsernameTest() {
        val firstLoginPage = TestAssetHelper.getSaveLoginAsset(mockWebServer)
        val secondLoginPage = "https://mozilla-mobile.github.io/testapp/v2.0/loginForm.html"
        val originWebsite = "mozilla-mobile.github.io"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstLoginPage.url) {
            clickSubmitLoginButton()
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(secondLoginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "android")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            clickSearchLoginButton()
            searchLogin("ANDROID")
            viewSavedLoginDetails(originWebsite)
            verifyLoginItemUsername("android")
            revealPassword()
            verifyPasswordSaved("firefox")
        }.goBackToSavedLogins {
            searchLogin("android")
            viewSavedLoginDetails(originWebsite)
            verifyLoginItemUsername("android")
            revealPassword()
            verifyPasswordSaved("firefox")
        }.goBackToSavedLogins {
            searchLogin("AnDrOiD")
            viewSavedLoginDetails(originWebsite)
            verifyLoginItemUsername("android")
            revealPassword()
            verifyPasswordSaved("firefox")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/608834
    @Test
    fun searchLoginsByUrlTest() {
        val firstLoginPage = TestAssetHelper.getSaveLoginAsset(mockWebServer)
        val secondLoginPage = "https://mozilla-mobile.github.io/testapp/v2.0/loginForm.html"
        val originWebsite = "mozilla-mobile.github.io"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstLoginPage.url) {
            clickSubmitLoginButton()
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(secondLoginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "android")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            clickSearchLoginButton()
            searchLogin("MOZILLA")
            viewSavedLoginDetails(originWebsite)
            verifyLoginItemUsername("android")
            revealPassword()
            verifyPasswordSaved("firefox")
        }.goBackToSavedLogins {
            searchLogin("mozilla")
            viewSavedLoginDetails(originWebsite)
            verifyLoginItemUsername("android")
            revealPassword()
            verifyPasswordSaved("firefox")
        }.goBackToSavedLogins {
            searchLogin("MoZiLlA")
            viewSavedLoginDetails(originWebsite)
            verifyLoginItemUsername("android")
            revealPassword()
            verifyPasswordSaved("firefox")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2266441
    @Test
    fun verifyLastUsedLoginSortingOptionTest() {
        val firstLoginPage = TestAssetHelper.getSaveLoginAsset(mockWebServer)
        val secondLoginPage = "https://mozilla-mobile.github.io/testapp/v2.0/loginForm.html"
        val originWebsite = "mozilla-mobile.github.io"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstLoginPage.url) {
            clickSubmitLoginButton()
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(secondLoginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            clickSavedLoginsChevronIcon()
            verifyLoginsSortingOptions()
            clickLastUsedSortingOption()
            verifySortedLogin(0, originWebsite)
            verifySortedLogin(1, firstLoginPage.url.authority.toString())
        }.goBack {
        }.openSavedLogins {
            verifySortedLogin(0, originWebsite)
            verifySortedLogin(1, firstLoginPage.url.authority.toString())
        }

        restartApp(activityTestRule)

        browserScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            verifySortedLogin(0, originWebsite)
            verifySortedLogin(1, firstLoginPage.url.authority.toString())
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2266442
    @Test
    fun verifyAlphabeticalLoginSortingOptionTest() {
        val firstLoginPage = TestAssetHelper.getSaveLoginAsset(mockWebServer)
        val secondLoginPage = "https://mozilla-mobile.github.io/testapp/v2.0/loginForm.html"
        val originWebsite = "mozilla-mobile.github.io"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstLoginPage.url) {
            clickSubmitLoginButton()
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(secondLoginPage.toUri()) {
            setPageObjectText(itemWithResId("username"), "mozilla")
            setPageObjectText(itemWithResId("password"), "firefox")
            clickPageObject(itemWithResId("submit"))
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            verifySortedLogin(0, firstLoginPage.url.authority.toString())
            verifySortedLogin(1, originWebsite)
        }.goBack {
        }.openSavedLogins {
            verifySortedLogin(0, firstLoginPage.url.authority.toString())
            verifySortedLogin(1, originWebsite)
        }

        restartApp(activityTestRule)

        browserScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            verifySortedLogin(0, firstLoginPage.url.authority.toString())
            verifySortedLogin(1, originWebsite)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1518435
    @Test
    fun verifyAddLoginManuallyTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/v2.0/loginForm.html"

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            clickAddLoginButton()
            verifyAddNewLoginView()
            enterSiteCredential("mozilla")
            verifyHostnameErrorMessage()
            enterSiteCredential(loginPage)
            verifyHostnameClearButtonEnabled()
            setNewUserName("mozilla")
            setNewPassword("firefox")
            clickClearPasswordButton()
            verifyPasswordErrorMessage()
            setNewPassword("firefox")
            verifyPasswordClearButtonEnabled()
            saveEditedLogin()
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            clickPageObject(MatcherHelper.itemWithResId("username"))
            clickSuggestedLoginsButton()
            verifySuggestedUserName("mozilla")
            clickPageObject(itemWithResIdAndText("$packageName:id/username", "mozilla"))
            clickPageObject(itemWithResId("togglePassword"))
            verifyPrefilledLoginCredentials("mozilla", "firefox", true)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2068215
    @Test
    fun verifyCopyLoginCredentialsToClipboardTest() {
        val firstLoginPage = TestAssetHelper.getSaveLoginAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstLoginPage.url) {
            clickSubmitLoginButton()
            verifySaveLoginPromptIsDisplayed()
            clickPageObject(itemWithText("Save"))
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            tapSetupLater()
            viewSavedLoginDetails("test@example.com")
            clickCopyUserNameButton()
            verifySnackBarText("Username copied to clipboard")
            clickCopyPasswordButton()
            verifySnackBarText("Password copied to clipboard")
        }
    }
}
