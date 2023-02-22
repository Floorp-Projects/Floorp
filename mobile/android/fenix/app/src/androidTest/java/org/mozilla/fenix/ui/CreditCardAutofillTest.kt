package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.bringAppToForeground
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.putAppToBackground
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import java.time.LocalDate

class CreditCardAutofillTest {
    private lateinit var mockWebServer: MockWebServer

    object MockCreditCard1 {
        const val MOCK_CREDIT_CARD_NUMBER = "5555555555554444"
        const val MOCK_LAST_CARD_DIGITS = "4444"
        const val MOCK_NAME_ON_CARD = "Mastercard"
        const val MOCK_EXPIRATION_MONTH = "February"
        val MOCK_EXPIRATION_YEAR = (LocalDate.now().year + 1).toString()
        val MOCK_EXPIRATION_MONTH_AND_YEAR = "02/${(LocalDate.now().year + 1)}"
    }

    object MockCreditCard2 {
        const val MOCK_CREDIT_CARD_NUMBER = "2720994326581252"
        const val MOCK_LAST_CARD_DIGITS = "1252"
        const val MOCK_NAME_ON_CARD = "Mastercard"
        const val MOCK_EXPIRATION_MONTH = "March"
        val MOCK_EXPIRATION_YEAR = (LocalDate.now().year + 2).toString()
        val MOCK_EXPIRATION_MONTH_AND_YEAR = "03/${(LocalDate.now().year + 2)}"
    }

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

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
    fun verifyCreditCardAutofillTest() {
        val creditCardFormPage = TestAssetHelper.getCreditCardFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
            // Opening Manage saved cards to dismiss here the Secure your credit prompt
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
        }.goBackToAutofillSettings {
        }.goBack {
        }.goBack {
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            clickCreditCardNumberTextBox()
            clickSelectCreditCardButton()
            clickCreditCardSuggestion(MockCreditCard1.MOCK_LAST_CARD_DIGITS)
            verifyAutofilledCreditCard(MockCreditCard1.MOCK_CREDIT_CARD_NUMBER)
        }
    }

    @SmokeTest
    @Test
    fun deleteSavedCreditCardTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
            clickSavedCreditCard()
            clickDeleteCreditCardButton()
            clickConfirmDeleteCreditCardButton()
            verifyAddCreditCardsButton()
        }
    }

    @Test
    fun verifyCreditCardsSectionTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
            verifySavedCreditCardsSection(
                MockCreditCard1.MOCK_LAST_CARD_DIGITS,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
        }
    }

    @Test
    fun verifyManageCreditCardsPromptOptionTest() {
        val creditCardFormPage = TestAssetHelper.getCreditCardFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            clickCreditCardNumberTextBox()
            clickSelectCreditCardButton()
        }.clickManageCreditCardsButton {
        }.goBackToBrowser {
            verifySelectCreditCardPromptExists(false)
        }
    }

    @Test
    fun verifyCreditCardsAutofillToggleTest() {
        val creditCardFormPage = TestAssetHelper.getCreditCardFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            clickCreditCardNumberTextBox()
            verifySelectCreditCardPromptExists(true)
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            clickSaveAndAutofillCreditCardsOption()
            verifyCreditCardsAutofillSection(false, true)
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            clickCreditCardNumberTextBox()
            verifySelectCreditCardPromptExists(false)
        }
    }

    @Test
    fun verifyEditCardsViewTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
            verifySavedCreditCardsSection(
                MockCreditCard1.MOCK_LAST_CARD_DIGITS,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
            clickSavedCreditCard()
            verifyEditCreditCardView(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
        }.goBackToSavedCreditCards {
            verifySavedCreditCardsSection(
                MockCreditCard1.MOCK_LAST_CARD_DIGITS,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
        }
    }

    @Test
    fun verifyEditedCardIsSavedTest() {
        val creditCardFormPage = TestAssetHelper.getCreditCardFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
            verifySavedCreditCardsSection(
                MockCreditCard1.MOCK_LAST_CARD_DIGITS,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
            clickSavedCreditCard()
            fillAndSaveCreditCard(
                MockCreditCard2.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard2.MOCK_NAME_ON_CARD,
                MockCreditCard2.MOCK_EXPIRATION_MONTH,
                MockCreditCard2.MOCK_EXPIRATION_YEAR,
            )
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            clickCreditCardNumberTextBox()
            clickSelectCreditCardButton()
            clickCreditCardSuggestion(MockCreditCard2.MOCK_LAST_CARD_DIGITS)
            verifyAutofilledCreditCard(MockCreditCard2.MOCK_CREDIT_CARD_NUMBER)
        }
    }

    @Test
    fun verifyCreditCardCannotBeSavedWithoutCardNumberTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
            verifySavedCreditCardsSection(
                MockCreditCard1.MOCK_LAST_CARD_DIGITS,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
            clickSavedCreditCard()
            clearCreditCardNumber()
            clickToolbarSaveCreditCardButton()
            verifyEditCreditCardToolbarTitle()
            verifyCreditCardNumberErrorMessage()
        }
    }

    @Test
    fun verifyCreditCardCannotBeSavedWithoutNameOnCardTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
            verifySavedCreditCardsSection(
                MockCreditCard1.MOCK_LAST_CARD_DIGITS,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
            clickSavedCreditCard()
            clearNameOnCreditCard()
            clickToolbarSaveCreditCardButton()
            verifyEditCreditCardToolbarTitle()
            verifyNameOnCreditCardErrorMessage()
        }
    }

    @Test
    fun verifyMultipleCreditCardsCanBeSavedTest() {
        val creditCardFormPage = TestAssetHelper.getCreditCardFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard2.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard2.MOCK_NAME_ON_CARD,
                MockCreditCard2.MOCK_EXPIRATION_MONTH,
                MockCreditCard2.MOCK_EXPIRATION_YEAR,
            )
            verifySavedCreditCardsSection(
                MockCreditCard1.MOCK_LAST_CARD_DIGITS,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
            verifySavedCreditCardsSection(
                MockCreditCard2.MOCK_LAST_CARD_DIGITS,
                MockCreditCard2.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            clickCreditCardNumberTextBox()
            clickSelectCreditCardButton()
            verifyCreditCardSuggestion(
                MockCreditCard1.MOCK_LAST_CARD_DIGITS,
                MockCreditCard2.MOCK_LAST_CARD_DIGITS,
            )
            clickCreditCardSuggestion(MockCreditCard2.MOCK_LAST_CARD_DIGITS)
            verifyAutofilledCreditCard(MockCreditCard2.MOCK_CREDIT_CARD_NUMBER)
        }
    }

    @Test
    fun verifyDoNotSaveCreditCardFromFormTest() {
        val creditCardFormPage = TestAssetHelper.getCreditCardFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
            clickCancelCreditCardPromptButton()
            verifyUpdateOrSaveCreditCardPromptExists(exists = false)
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
        }
    }

    @Test
    fun verifySaveCreditCardFromFormTest() {
        val creditCardFormPage = TestAssetHelper.getCreditCardFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
            clickUpdateOrSaveCreditCardPromptButton()
            verifyUpdateOrSaveCreditCardPromptExists(exists = false)
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, true)
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
            verifySavedCreditCardsSection(
                MockCreditCard1.MOCK_LAST_CARD_DIGITS,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
        }
    }

    @Test
    fun verifyCancelCreditCardUpdatePromptTest() {
        val creditCardFormPage = TestAssetHelper.getCreditCardFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard2.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard2.MOCK_NAME_ON_CARD,
                MockCreditCard2.MOCK_EXPIRATION_MONTH,
                MockCreditCard2.MOCK_EXPIRATION_YEAR,
            )
            // Opening Manage saved cards to dismiss here the Secure your credit prompt
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            clickCreditCardNumberTextBox()
            clickSelectCreditCardButton()
            clickCreditCardSuggestion(MockCreditCard2.MOCK_LAST_CARD_DIGITS)
            verifyAutofilledCreditCard(MockCreditCard2.MOCK_CREDIT_CARD_NUMBER)
            changeCreditCardExpiryDate(MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR)
            clickCreditCardFormSubmitButton()
            clickCancelCreditCardPromptButton()
            verifyUpdateOrSaveCreditCardPromptExists(false)
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, true)
            clickManageSavedCreditCardsButton()
            verifySavedCreditCardsSection(
                MockCreditCard2.MOCK_LAST_CARD_DIGITS,
                MockCreditCard2.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
        }
    }

    @Test
    fun verifyConfirmCreditCardUpdatePromptTest() {
        val creditCardFormPage = TestAssetHelper.getCreditCardFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard2.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard2.MOCK_NAME_ON_CARD,
                MockCreditCard2.MOCK_EXPIRATION_MONTH,
                MockCreditCard2.MOCK_EXPIRATION_YEAR,
            )
            // Opening Manage saved cards to dismiss here the Secure your credit prompt
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            clickCreditCardNumberTextBox()
            clickSelectCreditCardButton()
            clickCreditCardSuggestion(MockCreditCard2.MOCK_LAST_CARD_DIGITS)
            verifyAutofilledCreditCard(MockCreditCard2.MOCK_CREDIT_CARD_NUMBER)
            changeCreditCardExpiryDate(MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR)
            clickCreditCardFormSubmitButton()
            clickUpdateOrSaveCreditCardPromptButton()
            verifyUpdateOrSaveCreditCardPromptExists(false)
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, true)
            clickManageSavedCreditCardsButton()
            verifySavedCreditCardsSection(
                MockCreditCard2.MOCK_LAST_CARD_DIGITS,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
        }
    }

    @Test
    fun verifySavedCreditCardsRedirectionToAutofillAfterInterruptionTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
            verifySavedCreditCardsSection(
                MockCreditCard1.MOCK_LAST_CARD_DIGITS,
                MockCreditCard1.MOCK_EXPIRATION_MONTH_AND_YEAR,
            )
            putAppToBackground()
            bringAppToForeground()
            verifyAutofillToolbarTitle()
        }
    }

    @Test
    fun verifyEditCreditCardRedirectionToAutofillAfterInterruptionTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyCreditCardsAutofillSection(true, false)
            clickAddCreditCardButton()
            fillAndSaveCreditCard(
                MockCreditCard1.MOCK_CREDIT_CARD_NUMBER,
                MockCreditCard1.MOCK_NAME_ON_CARD,
                MockCreditCard1.MOCK_EXPIRATION_MONTH,
                MockCreditCard1.MOCK_EXPIRATION_YEAR,
            )
            clickManageSavedCreditCardsButton()
            clickSecuredCreditCardsLaterButton()
            clickSavedCreditCard()
            putAppToBackground()
            bringAppToForeground()
            verifyAutofillToolbarTitle()
        }
    }
}
