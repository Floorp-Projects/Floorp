/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText

class SettingsSubMenuAutofillRobot {

    fun clickAddAddressButton() = addAddressButton.click()
    fun clickManageAddressesButton() = manageAddressesButton.click()
    fun clickSavedAddress(firstName: String) = savedAddress(firstName).clickAndWaitForNewWindow(waitingTime)
    fun clickDeleteAddressButton() {
        deleteAddressButton.waitForExists(waitingTime)
        deleteAddressButton.click()
    }
    fun clickCancelDeleteAddressButton() {
        cancelDeleteAddressButton.waitForExists(waitingTime)
        cancelDeleteAddressButton.click()
    }
    fun clickConfirmDeleteAddressButton() {
        confirmDeleteAddressButton.waitForExists(waitingTime)
        confirmDeleteAddressButton.click()
    }
    fun clickSubRegionOption(subRegion: String) {
        subRegionOption(subRegion).waitForExists(waitingTime)
        subRegionOption(subRegion).click()
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
        emailAddress: String
    ) {
        firstNameTextInput.waitForExists(waitingTime)
        firstNameTextInput.setText(firstName)
        middleNameTextInput.setText(middleName)
        lastNameTextInput.setText(lastName)
        streetAddressTextInput.setText(streetAddress)
        scrollToElementByText(getStringResource(R.string.addresses_city))
        cityTextInput.setText(city)
        subRegionDropDown.click()
        clickSubRegionOption(state)
        zipCodeTextInput.setText(zipCode)
        countryDropDown.click()
        clickCountryOption(country)
        scrollToElementByText(getStringResource(R.string.addresses_phone))
        phoneTextInput.setText(phoneNumber)
        emailTextInput.setText(emailAddress)
        scrollToElementByText(getStringResource(R.string.addresses_save_button))
        saveButton.click()
        manageAddressesButton.waitForExists(waitingTime)
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.pressBack()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private val addAddressButton = mDevice.findObject(UiSelector().textContains(getStringResource(R.string.preferences_addresses_add_address)))
private val manageAddressesButton = mDevice.findObject(UiSelector().textContains(getStringResource(R.string.preferences_addresses_manage_addresses)))

private val firstNameTextInput = mDevice.findObject(UiSelector().resourceId("$packageName:id/first_name_input"))
private val middleNameTextInput = mDevice.findObject(UiSelector().resourceId("$packageName:id/middle_name_input"))
private val lastNameTextInput = mDevice.findObject(UiSelector().resourceId("$packageName:id/last_name_input"))
private val streetAddressTextInput = mDevice.findObject(UiSelector().resourceId("$packageName:id/street_address_input"))
private val cityTextInput = mDevice.findObject(UiSelector().resourceId("$packageName:id/city_input"))
private val subRegionDropDown = mDevice.findObject(UiSelector().resourceId("$packageName:id/subregion_drop_down"))
private val zipCodeTextInput = mDevice.findObject(UiSelector().resourceId("$packageName:id/zip_input"))
private val countryDropDown = mDevice.findObject(UiSelector().resourceId("$packageName:id/country_drop_down"))
private val phoneTextInput = mDevice.findObject(UiSelector().resourceId("$packageName:id/phone_input"))
private val emailTextInput = mDevice.findObject(UiSelector().resourceId("$packageName:id/email_input"))
private val saveButton = mDevice.findObject(UiSelector().resourceId("$packageName:id/save_button"))
private val deleteAddressButton = mDevice.findObject(UiSelector().resourceId("$packageName:id/delete_address_button"))
private val cancelDeleteAddressButton = mDevice.findObject(UiSelector().resourceId("android:id/button2"))
private val confirmDeleteAddressButton = mDevice.findObject(UiSelector().resourceId("android:id/button1"))

private fun savedAddress(firstName: String) = mDevice.findObject(UiSelector().textContains(firstName))
private fun subRegionOption(subRegion: String) = mDevice.findObject(UiSelector().textContains(subRegion))
private fun countryOption(country: String) = mDevice.findObject(UiSelector().textContains(country))
