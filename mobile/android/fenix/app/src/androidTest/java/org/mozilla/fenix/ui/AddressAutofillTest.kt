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
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class AddressAutofillTest {
    private lateinit var mockWebServer: MockWebServer

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

    @Test
    fun verifyAddAddressViewTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            clickAddAddressButton()
            verifyAddAddressView()
        }.goBackToAutofillSettings {
            verifyAutofillToolbarTitle()
        }
    }

    @Test
    fun verifyEditAddressViewTest() {
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
            verifyEditAddressView()
        }
    }

    @Test
    fun verifyAddressAutofillToggleTest() {
        val addressFormPage =
            TestAssetHelper.getAddressFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyAddressAutofillSection(true, false)
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
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(addressFormPage.url) {
            clickStreetAddressTextBox()
            verifySelectAddressButtonExists(true)
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            clickSaveAndAutofillAddressesOption()
            verifyAddressAutofillSection(false, true)
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(addressFormPage.url) {
            clickStreetAddressTextBox()
            verifySelectAddressButtonExists(false)
        }
    }

    @Test
    fun verifyManageAddressesPromptOptionTest() {
        val addressFormPage =
            TestAssetHelper.getAddressFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyAddressAutofillSection(true, false)
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
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(addressFormPage.url) {
            clickStreetAddressTextBox()
            clickSelectAddressButton()
        }.clickManageAddressButton {
            verifyAutofillToolbarTitle()
        }.goBackToBrowser {
            verifySaveLoginPromptIsNotDisplayed()
        }
    }

    @Test
    fun verifyAddressAutofillSelectionTest() {
        val addressFormPage =
            TestAssetHelper.getAddressFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyAddressAutofillSection(true, false)
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
            clickAddAddressButton()
            fillAndSaveAddress(
                "Android",
                "Test",
                "Name",
                "Fort Street",
                "San Jose",
                "Arizona",
                "95141",
                "United States",
                "777-7777",
                "fuu@bar.org",
            )
            verifyManageAddressesToolbarTitle()
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(addressFormPage.url) {
            clickStreetAddressTextBox()
            clickSelectAddressButton()
            clickAddressSuggestion("Harrison Street")
            verifyAutofilledAddress("Harrison Street")
            clearAddressForm()
            clickStreetAddressTextBox()
            clickSelectAddressButton()
            clickAddressSuggestion("Fort Street")
            verifyAutofilledAddress("Fort Street")
        }
    }

    @Test
    fun verifySavedAddressCanBeEditedTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyAddressAutofillSection(true, false)
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
            fillAndSaveAddress(
                "Android",
                "Test",
                "Name",
                "Fort Street",
                "San Jose",
                "Arizona",
                "95141",
                "United States",
                "777-7777",
                "fuu@bar.org",
            )
            verifyManageAddressesToolbarTitle()
        }
    }

    @Test
    fun verifyStateFieldUpdatesInAccordanceWithCountryFieldTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyAddressAutofillSection(true, false)
            clickAddAddressButton()
            verifyCountryOption("United States")
            verifyStateOption("Alabama")
            verifyCountryOptions("Canada", "United States")
            clickCountryOption("Canada")
            verifyStateOption("Alberta")
        }
    }

    @Test
    fun verifyFormFieldCanBeFilledManuallyTest() {
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
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(addressFormPage.url) {
            clickStreetAddressTextBox()
            clickSelectAddressButton()
            clickAddressSuggestion("Harrison Street")
            verifyAutofilledAddress("Harrison Street")
            setTextForApartmentTextBox("Ap. 07")
            verifyManuallyFilledAddress("Ap. 07")
        }
    }

    @Test
    fun verifyAutofillAddressSectionTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAutofillSubMenu {
            verifyAddressAutofillSection(true, false)
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
            verifyAddressAutofillSection(true, true)
            clickManageAddressesButton()
            verifyManageAddressesSection(
                "Mozilla",
                "Fenix",
                "Firefox",
                "Harrison Street",
                "San Francisco",
                "Alaska",
                "94105",
                "US",
                "555-5555",
                "foo@bar.com",
            )
        }
    }
}
