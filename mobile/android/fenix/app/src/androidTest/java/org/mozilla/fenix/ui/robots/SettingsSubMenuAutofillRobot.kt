/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

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
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithDescriptionExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.hasCousin
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click

class SettingsSubMenuAutofillRobot {

    fun verifyAutofillToolbarTitle() = assertItemContainingTextExists(autofillToolbarTitle)
    fun verifyManageAddressesToolbarTitle() = assertItemContainingTextExists(manageAddressesToolbarTitle)

    fun verifyAddressAutofillSection(isAddressAutofillEnabled: Boolean, userHasSavedAddress: Boolean) {
        assertItemContainingTextExists(
            autofillToolbarTitle,
            addressesSectionTitle,
            saveAndAutofillAddressesOption,
            saveAndAutofillAddressesSummary,
        )

        if (userHasSavedAddress) {
            assertItemContainingTextExists(manageAddressesButton)
        } else {
            assertItemContainingTextExists(addAddressButton)
        }

        verifyAddressesAutofillToggle(isAddressAutofillEnabled)
    }

    fun verifyCreditCardsAutofillSection(isAddressAutofillEnabled: Boolean, userHasSavedCreditCard: Boolean) {
        assertItemContainingTextExists(
            autofillToolbarTitle,
            creditCardsSectionTitle,
            saveAndAutofillCreditCardsOption,
            saveAndAutofillCreditCardsSummary,
            syncCreditCardsAcrossDevicesButton,

        )

        if (userHasSavedCreditCard) {
            assertItemContainingTextExists(manageSavedCreditCardsButton)
        } else {
            assertItemContainingTextExists(addCreditCardButton)
        }

        verifySaveAndAutofillCreditCardsToggle(isAddressAutofillEnabled)
    }

    fun verifyManageAddressesSection(vararg savedAddressDetails: String) {
        assertItemWithDescriptionExists(navigateBackButton)
        assertItemContainingTextExists(
            manageAddressesToolbarTitle,
            addAddressButton,
        )
        for (savedAddressDetail in savedAddressDetails) {
            assertTrue(
                mDevice.findObject(
                    UiSelector().textContains(savedAddressDetail),
                ).waitForExists(waitingTime),
            )
        }
    }

    fun verifySavedCreditCardsSection(creditCardLastDigits: String, creditCardExpiryDate: String) {
        assertItemWithDescriptionExists(navigateBackButton)
        assertItemContainingTextExists(
            savedCreditCardsToolbarTitle,
            addCreditCardButton,
            itemContainingText(creditCardLastDigits),
            itemContainingText(creditCardExpiryDate),
        )
    }

    fun verifyAddressesAutofillToggle(enabled: Boolean) =
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
        assertItemContainingTextExists(addAddressToolbarTitle)
        assertItemWithDescriptionExists(navigateBackButton)
        assertItemWithResIdExists(
            toolbarCheckmarkButton,
            firstNameTextInput,
            middleNameTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_street_address))
        assertItemWithResIdExists(
            lastNameTextInput,
            streetAddressTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_country))
        assertItemWithResIdExists(
            cityTextInput,
            subRegionDropDown,
            zipCodeTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_save_button))
        assertItemWithResIdExists(
            countryDropDown,
            phoneTextInput,
            emailTextInput,
        )
        assertItemWithResIdExists(
            saveButton,
            cancelButton,
        )
    }

    fun verifyCountryOption(country: String) {
        scrollToElementByText(getStringResource(R.string.addresses_country))
        mDevice.pressBack()
        assertItemContainingTextExists(itemContainingText(country))
    }

    fun verifyStateOption(state: String) =
        assertItemContainingTextExists(itemContainingText(state))

    fun verifyCountryOptions(vararg countries: String) {
        countryDropDown.click()
        for (country in countries) {
            assertItemContainingTextExists(itemContainingText(country))
        }
    }

    fun selectCountry(country: String) {
        countryDropDown.click()
        countryOption(country).click()
    }

    fun verifyEditAddressView() {
        assertItemContainingTextExists(editAddressToolbarTitle)
        assertItemWithDescriptionExists(navigateBackButton)
        assertItemWithResIdExists(
            toolbarDeleteAddressButton,
            toolbarCheckmarkButton,
            firstNameTextInput,
            middleNameTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_street_address))
        assertItemWithResIdExists(
            lastNameTextInput,
            streetAddressTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_country))
        assertItemWithResIdExists(
            cityTextInput,
            subRegionDropDown,
            zipCodeTextInput,
        )
        scrollToElementByText(getStringResource(R.string.addresses_save_button))
        assertItemWithResIdExists(
            countryDropDown,
            phoneTextInput,
            emailTextInput,
        )
        assertItemWithResIdExists(
            saveButton,
            cancelButton,
        )
        assertItemContainingTextExists(deleteAddressButton)
    }

    fun clickSaveAndAutofillAddressesOption() = saveAndAutofillAddressesOption.click()
    fun clickAddAddressButton() = addAddressButton.click()
    fun clickManageAddressesButton() = manageAddressesButton.click()
    fun clickSavedAddress(firstName: String) = savedAddress(firstName).clickAndWaitForNewWindow(waitingTime)
    fun clickDeleteAddressButton() {
        toolbarDeleteAddressButton.waitForExists(waitingTime)
        toolbarDeleteAddressButton.click()
    }
    fun clickCancelDeleteAddressButton() = cancelDeleteAddressButton.click()

    fun clickConfirmDeleteAddressButton() = confirmDeleteAddressButton.click()

    fun clickSubRegionOption(subRegion: String) {
        scrollToElementByText(subRegion)
        subRegionOption(subRegion).also {
            it.waitForExists(waitingTime)
            it.click()
        }
    }
    fun clickCountryOption(country: String) {
        countryOption(country).waitForExists(waitingTime)
        countryOption(country).click()
    }
    fun verifyAddAddressButton() = assertTrue(addAddressButton.waitForExists(waitingTime))

    fun fillAndSaveAddress(
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
        firstNameTextInput.waitForExists(waitingTime)
        mDevice.pressBack()
        firstNameTextInput.setText(firstName)
        middleNameTextInput.setText(middleName)
        lastNameTextInput.setText(lastName)
        streetAddressTextInput.setText(streetAddress)
        cityTextInput.setText(city)
        subRegionDropDown.click()
        clickSubRegionOption(state)
        zipCodeTextInput.setText(zipCode)
        countryDropDown.click()
        clickCountryOption(country)
        scrollToElementByText(getStringResource(R.string.addresses_save_button))
        phoneTextInput.setText(phoneNumber)
        emailTextInput.setText(emailAddress)
        saveButton.click()
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

    fun verifyAddCreditCardsButton() = assertTrue(addCreditCardButton.waitForExists(waitingTime))

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
        assertItemContainingTextExists(editCreditCardToolbarTitle)
        assertItemWithDescriptionExists(navigateBackButton)

        assertItemWithResIdExists(
            deleteCreditCardToolbarButton,
            saveCreditCardToolbarButton,
        )

        assertEquals(cardNumber, creditCardNumberTextInput.text)
        assertEquals(cardName, nameOnCreditCardTextInput.text)

        // Can't get the text from the drop-down items, need to verify them individually
        assertItemWithResIdExists(
            expiryYearDropDown,
            expiryMonthDropDown,
        )

        assertItemContainingTextExists(
            itemContainingText(expiryMonth),
            itemContainingText(expiryYear),
        )

        assertItemWithResIdExists(
            saveButton,
            cancelButton,
        )

        assertItemContainingTextExists(deleteCreditCardMenuButton)
    }

    fun verifyEditCreditCardToolbarTitle() = assertItemContainingTextExists(editCreditCardToolbarTitle)

    fun verifyCreditCardNumberErrorMessage() =
        assertItemContainingTextExists(itemContainingText(getStringResource(R.string.credit_cards_number_validation_error_message)))

    fun verifyNameOnCreditCardErrorMessage() =
        assertItemContainingTextExists(itemContainingText(getStringResource(R.string.credit_cards_name_on_card_validation_error_message)))

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.pressBack()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun goBackToAutofillSettings(interact: SettingsSubMenuAutofillRobot.() -> Unit): SettingsSubMenuAutofillRobot.Transition {
            navigateBackButton.click()

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

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
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
