/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.getLoremIpsumAsset
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.registerAndCleanupIdlingResources
import org.mozilla.fenix.helpers.TestHelper.runWithSystemLocaleChanged
import org.mozilla.fenix.ui.SettingsBasicsTest.CreditCard.MOCK_CREDIT_CARD_NUMBER
import org.mozilla.fenix.ui.SettingsBasicsTest.CreditCard.MOCK_EXPIRATION_MONTH
import org.mozilla.fenix.ui.SettingsBasicsTest.CreditCard.MOCK_EXPIRATION_YEAR
import org.mozilla.fenix.ui.SettingsBasicsTest.CreditCard.MOCK_LAST_CARD_DIGITS
import org.mozilla.fenix.ui.SettingsBasicsTest.CreditCard.MOCK_NAME_ON_CARD
import org.mozilla.fenix.ui.robots.checkTextSizeOnWebsite
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.util.FRENCH_LANGUAGE_HEADER
import org.mozilla.fenix.ui.util.FRENCH_SYSTEM_LOCALE_OPTION
import org.mozilla.fenix.ui.util.FR_SETTINGS
import org.mozilla.fenix.ui.util.ROMANIAN_LANGUAGE_HEADER
import java.time.LocalDate
import java.util.Locale

/**
 *  Tests for verifying the General section of the Settings menu
 *
 */
class SettingsBasicsTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
    private lateinit var mockWebServer: MockWebServer

    object CreditCard {
        const val MOCK_CREDIT_CARD_NUMBER = "5555555555554444"
        const val MOCK_LAST_CARD_DIGITS = "4444"
        const val MOCK_NAME_ON_CARD = "Mastercard"
        const val MOCK_EXPIRATION_MONTH = "February"
        val MOCK_EXPIRATION_YEAR = (LocalDate.now().year + 1).toString()
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

    @Ignore("Failing after updating settings screen summaries. See: https://github.com/mozilla-mobile/fenix/issues/28208")
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
                "foo@bar.com",
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
                "foo@bar.com",
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

    @SmokeTest
    @Test
    fun switchLanguageTest() {
        val enLanguageHeaderText = getStringResource(R.string.preferences_language)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLanguageSubMenu {
            registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(
                    activityIntentTestRule.activity.findViewById(R.id.locale_list),
                    2,
                ),
            ) {
                selectLanguage("Romanian")
                verifyLanguageHeaderIsTranslated(ROMANIAN_LANGUAGE_HEADER)
                selectLanguage("Français")
                verifyLanguageHeaderIsTranslated(FRENCH_LANGUAGE_HEADER)
                selectLanguage(FRENCH_SYSTEM_LOCALE_OPTION)
                verifyLanguageHeaderIsTranslated(enLanguageHeaderText)
            }
        }
    }

    @Test
    fun searchInLanguagesListTest() {
        val systemLocaleDefault = getStringResource(R.string.default_locale_text)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLanguageSubMenu {
            verifyLanguageListIsDisplayed()
            openSearchBar()
            typeInSearchBar("French")
            verifySearchResultsContains(systemLocaleDefault)
            clearSearchBar()
            typeInSearchBar("French")
            selectLanguageSearchResult("Français")
            verifyLanguageHeaderIsTranslated(FRENCH_LANGUAGE_HEADER)
            // Add this step when https://github.com/mozilla-mobile/fenix/issues/26733 is fixed
            // verifyLanguageListIsDisplayed()
        }
    }

    // Because it requires changing system prefs, this test will run only on Debug builds
    @Ignore("Failing due to app translation bug, see: https://github.com/mozilla-mobile/fenix/issues/26729")
    @Test
    fun frenchSystemLocaleTest() {
        val frenchLocale = Locale("fr", "FR")

        runWithSystemLocaleChanged(frenchLocale, activityIntentTestRule) {
            mDevice.waitForIdle(waitingTimeLong)

            homeScreen {
            }.openThreeDotMenu {
            }.openSettings(localizedText = FR_SETTINGS) {
            }.openLanguageSubMenu(localizedText = FRENCH_LANGUAGE_HEADER) {
                verifyLanguageHeaderIsTranslated(FRENCH_LANGUAGE_HEADER)
                verifySelectedLanguage(FRENCH_SYSTEM_LOCALE_OPTION)
            }
        }
    }
}
