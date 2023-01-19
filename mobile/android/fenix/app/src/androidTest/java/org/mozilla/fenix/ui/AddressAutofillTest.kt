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
}
