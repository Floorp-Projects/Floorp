/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withClassName
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.endsWith
import org.junit.Assert.assertEquals
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.hasCousin
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click

class SettingsSubMenuAutofillRobot {

    fun verifyAutofillToolbarTitle() {
        assertUIObjectExists(autofillToolbarTitle)
        Log.i(TAG, "verifyAutofillToolbarTitle: Verified \"Autofill\" toolbar title exists")
    }
    fun verifyManageAddressesToolbarTitle() {
        assertUIObjectExists(manageAddressesToolbarTitle)
        Log.i(TAG, "verifyManageAddressesToolbarTitle: Verified \"Manage addresses\" toolbar title exists")
    }

    fun verifyAddressAutofillSection(isAddressAutofillEnabled: Boolean, userHasSavedAddress: Boolean) {
        assertUIObjectExists(
            autofillToolbarTitle,
            addressesSectionTitle,
            saveAndAutofillAddressesOption,
            saveAndAutofillAddressesSummary,
        )

        if (userHasSavedAddress) {
            assertUIObjectExists(manageAddressesButton)
        } else {
            assertUIObjectExists(addAddressButton)
        }

        verifyAddressesAutofillToggle(isAddressAutofillEnabled)
    }

    fun verifyCreditCardsAutofillSection(isAddressAutofillEnabled: Boolean, userHasSavedCreditCard: Boolean) {
        assertUIObjectExists(
            autofillToolbarTitle,
            creditCardsSectionTitle,
            saveAndAutofillCreditCardsOption,
            saveAndAutofillCreditCardsSummary,
            syncCreditCardsAcrossDevicesButton,

        )

        if (userHasSavedCreditCard) {
            assertUIObjectExists(manageSavedCreditCardsButton)
        } else {
            assertUIObjectExists(addCreditCardButton)
        }

        verifySaveAndAutofillCreditCardsToggle(isAddressAutofillEnabled)
    }

    fun verifyManageAddressesSection(vararg savedAddressDetails: String) {
        assertUIObjectExists(
            navigateBackButton,
            manageAddressesToolbarTitle,
            addAddressButton,
        )
        for (savedAddressDetail in savedAddressDetails) {
            assertUIObjectExists(itemContainingText(savedAddressDetail))
            Log.i(TAG, "verifyManageAddressesSection: Verified saved address detail: $savedAddressDetail exists")
        }
    }

    fun verifySavedCreditCardsSection(creditCardLastDigits: String, creditCardExpiryDate: String) {
        assertUIObjectExists(
            navigateBackButton,
            savedCreditCardsToolbarTitle,
            addCreditCardButton,
            itemContainingText(creditCardLastDigits),
            itemContainingText(creditCardExpiryDate),
        )
    }

    fun verifyAddressesAutofillToggle(enabled: Boolean) {
        onView(withText(R.string.preferences_addresses_save_and_autofill_addresses))
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            if (enabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )
        Log.i(TAG, "verifyAddressesAutofillToggle: Verified if address autofill toggle is enabled: $enabled")
    }

    fun verifySaveAndAutofillCreditCardsToggle(enabled: Boolean) =
        onView(withText(R.string.preferences_credit_cards_save_and_autofill_cards))
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            if (enabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )

    fun verifyAddAddressView() {
        assertUIObjectExists(
            addAddressToolbarTitle,
            navigateBackButton,
            toolbarCheckmarkButton,
            firstNameTextInput,
            middleNameTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_street_address))
        Log.i(TAG, "verifyAddAddressView: Scrolled to \"Street Address\" text input")
        assertUIObjectExists(
            lastNameTextInput,
            streetAddressTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_country))
        Log.i(TAG, "verifyAddAddressView: Scrolled to \"Country or region\" dropdown")
        assertUIObjectExists(
            cityTextInput,
            subRegionDropDown,
            zipCodeTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_save_button))
        Log.i(TAG, "verifyAddAddressView: Scrolled to \"Save\" button")
        assertUIObjectExists(
            countryDropDown,
            phoneTextInput,
            emailTextInput,
        )
        assertUIObjectExists(
            saveButton,
            cancelButton,
        )
    }

    fun verifyCountryOption(country: String) {
        scrollToElementByText(getStringResource(R.string.addresses_country))
        Log.i(TAG, "verifyCountryOption: Scrolled to \"Country or region\" dropdown")
        mDevice.pressBack()
        Log.i(TAG, "fillAndSaveAddress: Dismissed \"Country or region\" dropdown using device back button")
        assertUIObjectExists(itemContainingText(country))
    }

    fun verifyStateOption(state: String) {
        assertUIObjectExists(itemContainingText(state))
    }

    fun verifyCountryOptions(vararg countries: String) {
        countryDropDown.click()
        Log.i(TAG, "verifyCountryOptions: Clicked \"Country or region\" dropdown")
        for (country in countries) {
            assertUIObjectExists(itemContainingText(country))
        }
    }

    fun selectCountry(country: String) {
        countryDropDown.click()
        Log.i(TAG, "selectCountry: Clicked \"Country or region\" dropdown")
        countryOption(country).click()
        Log.i(TAG, "selectCountry: Selected $country dropdown option")
    }

    fun verifyEditAddressView() {
        assertUIObjectExists(
            editAddressToolbarTitle,
            navigateBackButton,
            toolbarDeleteAddressButton,
            toolbarCheckmarkButton,
            firstNameTextInput,
            middleNameTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_street_address))
        Log.i(TAG, "verifyEditAddressView: Scrolled to \"Street Address\" text input")
        assertUIObjectExists(
            lastNameTextInput,
            streetAddressTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_country))
        Log.i(TAG, "verifyEditAddressView: Scrolled to \"Country or region\" dropdown")
        assertUIObjectExists(
            cityTextInput,
            subRegionDropDown,
            zipCodeTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_save_button))
        Log.i(TAG, "verifyEditAddressView: Scrolled to \"Save\" button")
        assertUIObjectExists(
            countryDropDown,
            phoneTextInput,
            emailTextInput,
        )
        assertUIObjectExists(
            saveButton,
            cancelButton,
        )
        assertUIObjectExists(deleteAddressButton)
    }

    fun clickSaveAndAutofillAddressesOption() {
        saveAndAutofillAddressesOption.click()
        Log.i(TAG, "clickSaveAndAutofillAddressesOption: Clicked \"Save and autofill addresses\" button")
    }
    fun clickAddAddressButton() {
        addAddressButton.click()
        Log.i(TAG, "clickAddAddressButton: Clicked \"Add address\" button")
    }
    fun clickManageAddressesButton() {
        manageAddressesButton.click()
        Log.i(TAG, "clickManageAddressesButton: Clicked \"Manage addresses\" button")
    }
    fun clickSavedAddress(firstName: String) {
        savedAddress(firstName).clickAndWaitForNewWindow(waitingTime)
        Log.i(TAG, "clickSavedAddress: Clicked $firstName saved address and waiting for a new window for $waitingTime")
    }
    fun clickDeleteAddressButton() {
        Log.i(TAG, "clickDeleteAddressButton: Looking for delete address toolbar button")
        toolbarDeleteAddressButton.waitForExists(waitingTime)
        toolbarDeleteAddressButton.click()
        Log.i(TAG, "clickDeleteAddressButton: Clicked delete address toolbar button")
    }
    fun clickCancelDeleteAddressButton() {
        cancelDeleteAddressButton.click()
        Log.i(TAG, "clickCancelDeleteAddressButton: Clicked \"CANCEL\" button from delete address dialog")
    }

    fun clickConfirmDeleteAddressButton() {
        confirmDeleteAddressButton.click()
        Log.i(TAG, "clickConfirmDeleteAddressButton: Clicked \"DELETE\" button from delete address dialog")
    }

    fun clickSubRegionOption(subRegion: String) {
        scrollToElementByText(subRegion)
        Log.i(TAG, "clickSubRegionOption: Scrolled to \"State\" drop down")
        subRegionOption(subRegion).also {
            Log.i(TAG, "clickSubRegionOption: Looking for \"State\" $subRegion dropdown option")
            it.waitForExists(waitingTime)
            it.click()
            Log.i(TAG, "clickSubRegionOption: Clicked \"State\" $subRegion dropdown option")
        }
    }
    fun clickCountryOption(country: String) {
        Log.i(TAG, "clickCountryOption: Looking for \"Country or region\" $country dropdown option")
        countryOption(country).waitForExists(waitingTime)
        countryOption(country).click()
        Log.i(TAG, "clickCountryOption: Clicked \"Country or region\" $country dropdown option")
    }
    fun verifyAddAddressButton() {
        assertUIObjectExists(addAddressButton)
        Log.i(TAG, "verifyAddAddressButton: Verified \"Add address\" button exists")
    }

    fun fillAndSaveAddress(
        navigateToAutofillSettings: Boolean,
        isAddressAutofillEnabled: Boolean = true,
        userHasSavedAddress: Boolean = false,
        firstName: String,
        middleName: String,
        lastName: String,
        streetAddress: String,
        city: String,
        state: String,
        zipCode: String,
        country: String,
        phoneNumber: String,
        emailAddress: String,
    ) {
        if (navigateToAutofillSettings) {
            homeScreen {
            }.openThreeDotMenu {
            }.openSettings {
            }.openAutofillSubMenu {
                verifyAddressAutofillSection(isAddressAutofillEnabled, userHasSavedAddress)
                clickAddAddressButton()
            }
        }
        Log.i(TAG, "fillAndSaveAddress: Looking for \"First Name\" text input")
        firstNameTextInput.waitForExists(waitingTime)
        mDevice.pressBack()
        Log.i(TAG, "fillAndSaveAddress: Dismissed keyboard using device back button")
        firstNameTextInput.setText(firstName)
        Log.i(TAG, "fillAndSaveAddress: \"First Name\" set to $firstName")
        middleNameTextInput.setText(middleName)
        Log.i(TAG, "fillAndSaveAddress: \"Middle Name\" set to $middleName")
        lastNameTextInput.setText(lastName)
        Log.i(TAG, "fillAndSaveAddress: \"Last Name\" set to $lastName")
        streetAddressTextInput.setText(streetAddress)
        Log.i(TAG, "fillAndSaveAddress: \"Street Address\" set to $streetAddress")
        cityTextInput.setText(city)
        Log.i(TAG, "fillAndSaveAddress: \"City\" set to $city")
        subRegionDropDown.click()
        Log.i(TAG, "fillAndSaveAddress: Clicked \"State\" dropdown button")
        clickSubRegionOption(state)
        Log.i(TAG, "fillAndSaveAddress: Selected $state as \"State\"")
        zipCodeTextInput.setText(zipCode)
        Log.i(TAG, "fillAndSaveAddress: \"Zip\" set to $zipCode")
        countryDropDown.click()
        Log.i(TAG, "fillAndSaveAddress: Clicked \"Country or region\" dropdown button")
        clickCountryOption(country)
        Log.i(TAG, "fillAndSaveAddress: Selected $country as \"Country or region\"")
        scrollToElementByText(getStringResource(R.string.addresses_save_button))
        Log.i(TAG, "fillAndSaveAddress: Scrolled to \"Save\" button")
        phoneTextInput.setText(phoneNumber)
        Log.i(TAG, "fillAndSaveAddress: \"Phone\" set to $phoneNumber")
        emailTextInput.setText(emailAddress)
        Log.i(TAG, "fillAndSaveAddress: \"Email\" set to $emailAddress")
        saveButton.click()
        Log.i(TAG, "fillAndSaveAddress: Clicked \"Save\" button")
        Log.i(TAG, "fillAndSaveAddress: Looking for \"Manage addressese\" button")
        manageAddressesButton.waitForExists(waitingTime)
    }

    fun clickAddCreditCardButton() = addCreditCardButton.click()
    fun clickManageSavedCreditCardsButton() = manageSavedCreditCardsButton.click()
    fun clickSecuredCreditCardsLaterButton() = securedCreditCardsLaterButton.click()
    fun clickSavedCreditCard() = savedCreditCardNumber.clickAndWaitForNewWindow(waitingTime)
    fun clickDeleteCreditCardToolbarButton() {
        deleteCreditCardToolbarButton.waitForExists(waitingTime)
        deleteCreditCardToolbarButton.click()
    }
    fun clickDeleteCreditCardMenuButton() {
        deleteCreditCardMenuButton.waitForExists(waitingTime)
        deleteCreditCardMenuButton.click()
    }
    fun clickSaveAndAutofillCreditCardsOption() = saveAndAutofillCreditCardsOption.click()

    fun clickConfirmDeleteCreditCardButton() = confirmDeleteCreditCardButton.click()

    fun clickCancelDeleteCreditCardButton() = cancelDeleteCreditCardButton.click()

    fun clickExpiryMonthOption(expiryMonth: String) {
        expiryMonthOption(expiryMonth).waitForExists(waitingTime)
        expiryMonthOption(expiryMonth).click()
    }

    fun clickExpiryYearOption(expiryYear: String) {
        expiryYearOption(expiryYear).waitForExists(waitingTime)
        expiryYearOption(expiryYear).click()
    }

    fun verifyAddCreditCardsButton() = assertUIObjectExists(addCreditCardButton)

    fun fillAndSaveCreditCard(cardNumber: String, cardName: String, expiryMonth: String, expiryYear: String) {
        creditCardNumberTextInput.waitForExists(waitingTime)
        creditCardNumberTextInput.setText(cardNumber)
        nameOnCreditCardTextInput.setText(cardName)
        expiryMonthDropDown.click()
        clickExpiryMonthOption(expiryMonth)
        expiryYearDropDown.click()
        clickExpiryYearOption(expiryYear)

        saveButton.click()
        manageSavedCreditCardsButton.waitForExists(waitingTime)
    }

    fun clearCreditCardNumber() =
        creditCardNumberTextInput.also {
            it.waitForExists(waitingTime)
            it.clearTextField()
        }

    fun clearNameOnCreditCard() =
        nameOnCreditCardTextInput.also {
            it.waitForExists(waitingTime)
            it.clearTextField()
        }

    fun clickSaveCreditCardToolbarButton() = saveCreditCardToolbarButton.click()

    fun verifyEditCreditCardView(
        cardNumber: String,
        cardName: String,
        expiryMonth: String,
        expiryYear: String,
    ) {
        assertUIObjectExists(
            editCreditCardToolbarTitle,
            navigateBackButton,
            deleteCreditCardToolbarButton,
            saveCreditCardToolbarButton,
        )

        assertEquals(cardNumber, creditCardNumberTextInput.text)
        assertEquals(cardName, nameOnCreditCardTextInput.text)

        // Can't get the text from the drop-down items, need to verify them individually
        assertUIObjectExists(
            expiryYearDropDown,
            expiryMonthDropDown,
        )

        assertUIObjectExists(
            itemContainingText(expiryMonth),
            itemContainingText(expiryYear),
        )

        assertUIObjectExists(
            saveButton,
            cancelButton,
        )

        assertUIObjectExists(deleteCreditCardMenuButton)
    }

    fun verifyEditCreditCardToolbarTitle() = assertUIObjectExists(editCreditCardToolbarTitle)

    fun verifyCreditCardNumberErrorMessage() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.credit_cards_number_validation_error_message)))

    fun verifyNameOnCreditCardErrorMessage() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.credit_cards_name_on_card_validation_error_message)))

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.pressBack()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun goBackToAutofillSettings(interact: SettingsSubMenuAutofillRobot.() -> Unit): SettingsSubMenuAutofillRobot.Transition {
            navigateBackButton.click()
            Log.i(TAG, "goBackToAutofillSettings: Clicked \"Navigate back\" toolbar button")

            SettingsSubMenuAutofillRobot().interact()
            return SettingsSubMenuAutofillRobot.Transition()
        }

        fun goBackToSavedCreditCards(interact: SettingsSubMenuAutofillRobot.() -> Unit): SettingsSubMenuAutofillRobot.Transition {
            navigateBackButton.click()

            SettingsSubMenuAutofillRobot().interact()
            return SettingsSubMenuAutofillRobot.Transition()
        }

        fun goBackToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.pressBack()
            Log.i(TAG, "goBackToBrowser: Go back to browser view using device back button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun autofillScreen(interact: SettingsSubMenuAutofillRobot.() -> Unit): SettingsSubMenuAutofillRobot.Transition {
    SettingsSubMenuAutofillRobot().interact()
    return SettingsSubMenuAutofillRobot.Transition()
}

private val autofillToolbarTitle = itemContainingText(getStringResource(R.string.preferences_autofill))
private val addressesSectionTitle = itemContainingText(getStringResource(R.string.preferences_addresses))
private val manageAddressesToolbarTitle =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/navigationToolbar")
            .childSelector(UiSelector().text(getStringResource(R.string.addresses_manage_addresses))),
    )

private val saveAndAutofillAddressesOption = itemContainingText(getStringResource(R.string.preferences_addresses_save_and_autofill_addresses))
private val saveAndAutofillAddressesSummary = itemContainingText(getStringResource(R.string.preferences_addresses_save_and_autofill_addresses_summary))
private val addAddressButton = itemContainingText(getStringResource(R.string.preferences_addresses_add_address))
private val manageAddressesButton =
    mDevice.findObject(
        UiSelector()
            .resourceId("android:id/title")
            .text(getStringResource(R.string.preferences_addresses_manage_addresses)),
    )
private val addAddressToolbarTitle = itemContainingText(getStringResource(R.string.addresses_add_address))
private val editAddressToolbarTitle = itemContainingText(getStringResource(R.string.addresses_edit_address))
private val toolbarCheckmarkButton = itemWithResId("$packageName:id/save_address_button")
private val navigateBackButton = itemWithDescription(getStringResource(R.string.action_bar_up_description))
private val firstNameTextInput = itemWithResId("$packageName:id/first_name_input")
private val middleNameTextInput = itemWithResId("$packageName:id/middle_name_input")
private val lastNameTextInput = itemWithResId("$packageName:id/last_name_input")
private val streetAddressTextInput = itemWithResId("$packageName:id/street_address_input")
private val cityTextInput = itemWithResId("$packageName:id/city_input")
private val subRegionDropDown = itemWithResId("$packageName:id/subregion_drop_down")
private val zipCodeTextInput = itemWithResId("$packageName:id/zip_input")
private val countryDropDown = itemWithResId("$packageName:id/country_drop_down")
private val phoneTextInput = itemWithResId("$packageName:id/phone_input")
private val emailTextInput = itemWithResId("$packageName:id/email_input")
private val saveButton = itemWithResId("$packageName:id/save_button")
private val cancelButton = itemWithResId("$packageName:id/cancel_button")
private val deleteAddressButton = itemContainingText(getStringResource(R.string.addressess_delete_address_button))
private val toolbarDeleteAddressButton = itemWithResId("$packageName:id/delete_address_button")
private val cancelDeleteAddressButton = onView(withId(android.R.id.button2)).inRoot(RootMatchers.isDialog())
private val confirmDeleteAddressButton = onView(withId(android.R.id.button1)).inRoot(RootMatchers.isDialog())

private val creditCardsSectionTitle = itemContainingText(getStringResource(R.string.preferences_credit_cards))
private val saveAndAutofillCreditCardsOption = itemContainingText(getStringResource(R.string.preferences_credit_cards_save_and_autofill_cards))
private val saveAndAutofillCreditCardsSummary = itemContainingText(getStringResource(R.string.preferences_credit_cards_save_and_autofill_cards_summary))
private val syncCreditCardsAcrossDevicesButton = itemContainingText(getStringResource(R.string.preferences_credit_cards_sync_cards_across_devices))
private val addCreditCardButton = mDevice.findObject(UiSelector().textContains(getStringResource(R.string.preferences_credit_cards_add_credit_card)))
private val savedCreditCardsToolbarTitle = itemContainingText(getStringResource(R.string.credit_cards_saved_cards))
private val editCreditCardToolbarTitle = itemContainingText(getStringResource(R.string.credit_cards_edit_card))
private val manageSavedCreditCardsButton = mDevice.findObject(UiSelector().textContains(getStringResource(R.string.preferences_credit_cards_manage_saved_cards)))
private val creditCardNumberTextInput = mDevice.findObject(UiSelector().resourceId("$packageName:id/card_number_input"))
private val nameOnCreditCardTextInput = mDevice.findObject(UiSelector().resourceId("$packageName:id/name_on_card_input"))
private val expiryMonthDropDown = mDevice.findObject(UiSelector().resourceId("$packageName:id/expiry_month_drop_down"))
private val expiryYearDropDown = mDevice.findObject(UiSelector().resourceId("$packageName:id/expiry_year_drop_down"))
private val savedCreditCardNumber = mDevice.findObject(UiSelector().resourceId("$packageName:id/credit_card_logo"))
private val deleteCreditCardToolbarButton = mDevice.findObject(UiSelector().resourceId("$packageName:id/delete_credit_card_button"))
private val saveCreditCardToolbarButton = itemWithResId("$packageName:id/save_credit_card_button")
private val deleteCreditCardMenuButton = itemContainingText(getStringResource(R.string.credit_cards_delete_card_button))
private val confirmDeleteCreditCardButton = onView(withId(android.R.id.button1)).inRoot(RootMatchers.isDialog())
private val cancelDeleteCreditCardButton = onView(withId(android.R.id.button2)).inRoot(RootMatchers.isDialog())
private val securedCreditCardsLaterButton = onView(withId(android.R.id.button2)).inRoot(RootMatchers.isDialog())

private fun savedAddress(firstName: String) = mDevice.findObject(UiSelector().textContains(firstName))
private fun subRegionOption(subRegion: String) = mDevice.findObject(UiSelector().textContains(subRegion))
private fun countryOption(country: String) = mDevice.findObject(UiSelector().textContains(country))

private fun expiryMonthOption(expiryMonth: String) = mDevice.findObject(UiSelector().textContains(expiryMonth))
private fun expiryYearOption(expiryYear: String) = mDevice.findObject(UiSelector().textContains(expiryYear))
