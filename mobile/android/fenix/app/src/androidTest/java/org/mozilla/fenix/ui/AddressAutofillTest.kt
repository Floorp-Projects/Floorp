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
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.ui.robots.clickPageObject
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836845
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
            clickPageObject(itemWithResId("streetAddress"))
            clickSelectAddressButton()
            clickPageObject(
                itemWithResIdContainingText(
                    "$packageName:id/address_name",
                    "Harrison Street",
                ),
            )
            verifyAutofilledAddress("Harrison Street")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836856
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836840
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836841
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836839
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
            clickPageObject(itemWithResId("streetAddress"))
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
            clickPageObject(itemWithResId("streetAddress"))
            verifySelectAddressButtonExists(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836847
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
            clickPageObject(itemWithResId("streetAddress"))
            clickSelectAddressButton()
        }.clickManageAddressButton {
            verifyAutofillToolbarTitle()
        }.goBackToBrowser {
            verifySaveLoginPromptIsNotDisplayed()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836849
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1814032")
    @Test
    fun verifyMultipleAddressesSelectionTest() {
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
            clickPageObject(itemWithResId("streetAddress"))
            clickSelectAddressButton()
            clickPageObject(
                itemWithResIdContainingText(
                    "$packageName:id/address_name",
                    "Harrison Street",
                ),
            )
            verifyAutofilledAddress("Harrison Street")
            clearAddressForm()
            clickPageObject(itemWithResId("streetAddress"))
            clickSelectAddressButton()
            clickPageObject(
                itemWithResIdContainingText(
                    "$packageName:id/address_name",
                    "Fort Street",
                ),
            )
            verifyAutofilledAddress("Fort Street")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836850
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836848
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836858
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
            clickPageObject(itemWithResId("streetAddress"))
            clickSelectAddressButton()
            clickPageObject(
                itemWithResIdContainingText(
                    "$packageName:id/address_name",
                    "Harrison Street",
                ),
            )
            verifyAutofilledAddress("Harrison Street")
            setTextForApartmentTextBox("Ap. 07")
            verifyManuallyFilledAddress("Ap. 07")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1836838
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
