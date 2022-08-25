/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.content.res.Configuration
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.FeatureSettingsHelper
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.getLoremIpsumAsset
import org.mozilla.fenix.ui.SettingsBasicsTest.creditCard.MOCK_CREDIT_CARD_NUMBER
import org.mozilla.fenix.ui.SettingsBasicsTest.creditCard.MOCK_EXPIRATION_MONTH
import org.mozilla.fenix.ui.SettingsBasicsTest.creditCard.MOCK_EXPIRATION_YEAR
import org.mozilla.fenix.ui.SettingsBasicsTest.creditCard.MOCK_LAST_CARD_DIGITS
import org.mozilla.fenix.ui.SettingsBasicsTest.creditCard.MOCK_NAME_ON_CARD
import org.mozilla.fenix.ui.robots.checkTextSizeOnWebsite
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import java.time.LocalDate

/**
 *  Tests for verifying the General section of the Settings menu
 *
 */
class SettingsBasicsTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
    private lateinit var mockWebServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    object creditCard {
        const val MOCK_CREDIT_CARD_NUMBER = "5555555555554444"
        const val MOCK_LAST_CARD_DIGITS = "4444"
        const val MOCK_NAME_ON_CARD = "Mastercard"
        const val MOCK_EXPIRATION_MONTH = "February"
        val MOCK_EXPIRATION_YEAR = (LocalDate.now().year + 1).toString()
    }

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }

        featureSettingsHelper.setJumpBackCFREnabled(false)
        featureSettingsHelper.setTCPCFREnabled(false)
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()

        // resetting modified features enabled setting to default
        featureSettingsHelper.resetAllFeatureFlags()
    }

    private fun getUiTheme(): Boolean {
        val mode =
            activityIntentTestRule.activity.resources?.configuration?.uiMode?.and(Configuration.UI_MODE_NIGHT_MASK)

        return when (mode) {
            Configuration.UI_MODE_NIGHT_YES -> true // dark theme is set
            Configuration.UI_MODE_NIGHT_NO -> false // dark theme is not set, using light theme
            else -> false // default option is light theme
        }
    }

    @Test
    fun settingsGeneralItemsTests() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsToolbar()
            verifyGeneralHeading()
            verifySearchButton()
            verifyTabsButton()
            verifyHomepageButton()
            verifyCustomizeButton()
            verifyLoginsAndPasswordsButton()
            verifyAutofillButton()
            verifyAccessibilityButton()
            verifyLanguageButton()
            verifySetAsDefaultBrowserButton()
        }
    }

    @Test
    fun changeThemeSetting() {
        // Goes through the settings and changes the default search engine, then verifies it changes.
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openCustomizeSubMenu {
            verifyThemes()
            selectDarkMode()
            verifyDarkThemeApplied(getUiTheme())
            selectLightMode()
            verifyLightThemeApplied(getUiTheme())
        }
    }

    @Test
    fun changeAccessibiltySettings() {
        // Goes through the settings and changes the default text on a webpage, then verifies if the text has changed.
        val fenixApp = activityIntentTestRule.activity.applicationContext as FenixApplication
        val webpage = getLoremIpsumAsset(mockWebServer).url

        // This value will represent the text size percentage the webpage will scale to. The default value is 100%.
        val textSizePercentage = 180

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAccessibilitySubMenu {
            clickFontSizingSwitch()
            verifyEnabledMenuItems()
            changeTextSizeSlider(textSizePercentage)
            verifyTextSizePercentage(textSizePercentage)
        }.goBack {
        }.goBack {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(webpage) {
            checkTextSizeOnWebsite(textSizePercentage, fenixApp.components)
        }.openThreeDotMenu {
        }.openSettings {
        }.openAccessibilitySubMenu {
            clickFontSizingSwitch()
            verifyMenuItemsAreDisabled()
        }
    }

    @SmokeTest
    @Test
    fun verifyAddressAutofillTest() {
        val addressFormPage =
            TestAssetHelper.getAddressFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            clickAddAddressButton()
            fillAndSaveAddress(
                "Mozilla",
                "Fenix",
                "Firefox",
                "Harrison Street",
                "San Francisco",
                "Alaska",
                "94105",
                "United States",
                "555-5555",
                "foo@bar.com"
            )
        }.goBack {
        }.goBack {
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(addressFormPage.url) {
            clickStreetAddressTextBox()
            clickSelectAddressButton()
            clickAddressSuggestion("Harrison Street")
            verifyAutofilledAddress("Harrison Street")
        }
    }

    @SmokeTest
    @Test
    fun deleteSavedAddressTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            clickAddAddressButton()
            fillAndSaveAddress(
                "Mozilla",
                "Fenix",
                "Firefox",
                "Harrison Street",
                "San Francisco",
                "Alaska",
                "94105",
                "United States",
                "555-5555",
                "foo@bar.com"
            )
            clickManageAddressesButton()
            clickSavedAddress("Mozilla")
            clickDeleteAddressButton()
            clickCancelDeleteAddressButton()
            clickDeleteAddressButton()
            clickConfirmDeleteAddressButton()
            verifyAddAddressButton()
        }
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
            fillAndSaveCreditCard(MOCK_CREDIT_CARD_NUMBER, MOCK_NAME_ON_CARD, MOCK_EXPIRATION_MONTH, MOCK_EXPIRATION_YEAR)
            // Opening Manage saved cards to dismiss here the Secure your credit prompt
            clickManageSavedCardsButton()
            clickSecuredCreditCardsLaterButton()
        }.goBackToAutofillSettings {
        }.goBack {
        }.goBack {
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(creditCardFormPage.url) {
            clickCardNumberTextBox()
            clickSelectCreditCardButton()
            clickCreditCardSuggestion(MOCK_LAST_CARD_DIGITS)
            verifyAutofilledCreditCard(MOCK_CREDIT_CARD_NUMBER)
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
            fillAndSaveCreditCard(MOCK_CREDIT_CARD_NUMBER, MOCK_NAME_ON_CARD, MOCK_EXPIRATION_MONTH, MOCK_EXPIRATION_YEAR)
            clickManageSavedCardsButton()
            clickSecuredCreditCardsLaterButton()
            clickSavedCreditCard()
            clickDeleteCreditCardButton()
            clickConfirmDeleteCreditCardButton()
            verifyAddCreditCardsButton()
        }
    }
}
