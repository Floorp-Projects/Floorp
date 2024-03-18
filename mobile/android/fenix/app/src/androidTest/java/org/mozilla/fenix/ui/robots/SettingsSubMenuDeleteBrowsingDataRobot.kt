/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers.isDialog
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectIsGone
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.assertIsChecked
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Delete Browsing Data sub menu.
 */
class SettingsSubMenuDeleteBrowsingDataRobot {

    fun verifyAllCheckBoxesAreChecked() {
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Trying to verify that the \"Open tabs\" check box is checked")
        openTabsCheckBox().assertIsChecked(true)
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Verified that the \"Open tabs\" check box is checked")
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Trying to verify that the \"Browsing history\" check box is checked")
        browsingHistoryCheckBox().assertIsChecked(true)
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Verified that the \"Browsing history\" check box is checked")
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Trying to verify that the \"Cookies and site data\" check box is checked")
        cookiesAndSiteDataCheckBox().assertIsChecked(true)
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Verified that the \"Cookies and site data\" check box is checked")
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Trying to verify that the \"Cached images and files\" check box is checked")
        cachedFilesCheckBox().assertIsChecked(true)
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Verified that the \"Cached images and files\" check box is checked")
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Trying to verify that the \"Site permissions\" check box is checked")
        sitePermissionsCheckBox().assertIsChecked(true)
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Verified that the \"Site permissions\" check box is checked")
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Trying to verify that the \"Downloads\" check box is checked")
        downloadsCheckBox().assertIsChecked(true)
        Log.i(TAG, "verifyAllCheckBoxesAreChecked: Verified that the \"Downloads\" check box is checked")
    }
    fun verifyOpenTabsCheckBox(status: Boolean) {
        Log.i(TAG, "verifyOpenTabsCheckBox: Trying to verify that the \"Open tabs\" check box is checked: $status")
        openTabsCheckBox().assertIsChecked(status)
        Log.i(TAG, "verifyOpenTabsCheckBox: Verified that the \"Open tabs\" check box is checked: $status")
    }
    fun verifyBrowsingHistoryDetails(status: Boolean) {
        Log.i(TAG, "verifyBrowsingHistoryDetails: Trying to verify that the \"Browsing history\" check box is checked: $status")
        browsingHistoryCheckBox().assertIsChecked(status)
        Log.i(TAG, "verifyBrowsingHistoryDetails: Verified that the \"Browsing history\" check box is checked: $status")
    }
    fun verifyCookiesCheckBox(status: Boolean) {
        Log.i(TAG, "verifyCookiesCheckBox: Trying to verify that the \"Cookies and site data\" check box is checked: $status")
        cookiesAndSiteDataCheckBox().assertIsChecked(status)
        Log.i(TAG, "verifyCookiesCheckBox: Verified that the \"Cookies and site data\" check box is checked: $status")
    }
    fun verifyCachedFilesCheckBox(status: Boolean) {
        Log.i(TAG, "verifyCachedFilesCheckBox: Trying to verify that the \"Cached images and files\" check box is checked: $status")
        cachedFilesCheckBox().assertIsChecked(status)
        Log.i(TAG, "verifyCachedFilesCheckBox: Verified that the \"Cached images and files\" check box is checked: $status")
    }
    fun verifySitePermissionsCheckBox(status: Boolean) {
        Log.i(TAG, "verifySitePermissionsCheckBox: Trying to verify that the \"Site permissions\" check box is checked: $status")
        sitePermissionsCheckBox().assertIsChecked(status)
        Log.i(TAG, "verifySitePermissionsCheckBox: Verified that the \"Site permissions\" check box is checked: $status")
    }
    fun verifyDownloadsCheckBox(status: Boolean) {
        Log.i(TAG, "verifyDownloadsCheckBox: Trying to verify that the \"Downloads\" check box is checked: $status")
        downloadsCheckBox().assertIsChecked(status)
        Log.i(TAG, "verifyDownloadsCheckBox: Verified that the \"Downloads\" check box is checked: $status")
    }
    fun verifyOpenTabsDetails(tabNumber: String) {
        Log.i(TAG, "verifyOpenTabsDetails: Trying to verify that the \"Open tabs\" option summary containing $tabNumber open tabs is visible")
        openTabsDescription(tabNumber).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyOpenTabsDetails: Verified that the \"Open tabs\" option summary containing $tabNumber open tabs is visible")
    }
    fun verifyBrowsingHistoryDetails(addresses: String) = assertUIObjectExists(browsingHistoryDescription(addresses))

    fun verifyDeleteBrowsingDataDialog() {
        Log.i(TAG, "verifyDeleteBrowsingDataDialog: Trying to verify that the delete browsing data dialog message is visible")
        dialogMessage().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyDeleteBrowsingDataDialog: Verified that the delete browsing data dialog message is visible")
        Log.i(TAG, "verifyDeleteBrowsingDataDialog: Trying to verify that the delete browsing data dialog \"Cancel\" button is visible")
        dialogCancelButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyDeleteBrowsingDataDialog: Verified that the delete browsing data dialog \"Cancel\" button is visible")
        Log.i(TAG, "verifyDeleteBrowsingDataDialog: Trying to verify that the delete browsing data dialog \"Delete\" button is visible")
        dialogDeleteButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyDeleteBrowsingDataDialog: Verified that the delete browsing data dialog \"Delete\" button is visible")
    }

    fun switchOpenTabsCheckBox() = clickOpenTabsCheckBox()
    fun switchBrowsingHistoryCheckBox() = clickBrowsingHistoryCheckBox()
    fun switchCookiesCheckBox() = clickCookiesCheckBox()
    fun switchCachedFilesCheckBox() = clickCachedFilesCheckBox()
    fun switchSitePermissionsCheckBox() = clickSitePermissionsCheckBox()
    fun switchDownloadsCheckBox() = clickDownloadsCheckBox()
    fun clickDeleteBrowsingDataButton() {
        Log.i(TAG, "clickDeleteBrowsingDataButton: Trying to click the \"Delete browsing data\" button")
        deleteBrowsingDataButton().click()
        Log.i(TAG, "clickDeleteBrowsingDataButton: Clicked the \"Delete browsing data\" button")
    }
    fun clickDialogCancelButton() {
        Log.i(TAG, "clickDialogCancelButton: Trying to click the delete browsing data dialog \"Cancel\" button")
        dialogCancelButton().click()
        Log.i(TAG, "clickDialogCancelButton: Clicked the delete browsing data dialog \"Cancel\" button")
    }

    fun selectOnlyOpenTabsCheckBox() {
        clickBrowsingHistoryCheckBox()
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Trying to verify that the \"Browsing history\" check box is not checked")
        browsingHistoryCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Verified that the \"Browsing history\" check box is not checked")

        clickCookiesCheckBox()
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Trying to verify that the \"Cookies and site data\" check box is not checked")
        cookiesAndSiteDataCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Verified that the \"Cookies and site data\" check box is not checked")

        clickCachedFilesCheckBox()
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Trying to verify that the \"Cached images and files\" check box is not checked")
        cachedFilesCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Verified that the \"Cached images and files\" check box is not checked")

        clickSitePermissionsCheckBox()
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Trying to verify that the \"Site permissions\" check box is not checked")
        sitePermissionsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Verified that the \"Site permissions\" check box is not checked")

        clickDownloadsCheckBox()
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Trying to verify that the \"Downloads\" check box is not checked")
        downloadsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Verified that the \"Downloads\" check box is not checked")
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Trying to verify that the \"Open tabs\" check box is checked")
        openTabsCheckBox().assertIsChecked(true)
        Log.i(TAG, "selectOnlyOpenTabsCheckBox: Trying to verify that the \"Open tabs\" check box is checked")
    }

    fun selectOnlyBrowsingHistoryCheckBox() {
        clickOpenTabsCheckBox()
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Trying to verify that the \"Open tabs\" check box is not checked")
        openTabsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Verified that the \"Open tabs\" check box is not checked")

        clickCookiesCheckBox()
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Trying to verify that the \"Cookies and site data\" check box is not checked")
        cookiesAndSiteDataCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Verified that the \"Cookies and site data\" check box is not checked")

        clickCachedFilesCheckBox()
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Trying to verify that the \"Cached images and files\" check box is not checked")
        cachedFilesCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Verified that the \"Cached images and files\" check box is not checked")

        clickSitePermissionsCheckBox()
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Trying to verify that the \"Site permissions\" check box is not checked")
        sitePermissionsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Verified that the \"Site permissions\" check box is not checked")

        clickDownloadsCheckBox()
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Trying to verify that the \"Downloads\" check box is not checked")
        downloadsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Verified that the \"Downloads\" check box is not checked")

        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Trying to verify that the \"Browsing history\" check box is checked")
        browsingHistoryCheckBox().assertIsChecked(true)
        Log.i(TAG, "selectOnlyBrowsingHistoryCheckBox: Verified that the \"Browsing history\" check box is checked")
    }

    fun selectOnlyCookiesCheckBox() {
        clickOpenTabsCheckBox()
        Log.i(TAG, "selectOnlyCookiesCheckBox: Trying to verify that the \"Open tabs\" check box is not checked")
        openTabsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyCookiesCheckBox: Verified that the \"Open tabs\" check box is not checked")
        Log.i(TAG, "selectOnlyCookiesCheckBox: Trying to verify that the \"Cookies and site data\" check box is checked")
        cookiesAndSiteDataCheckBox().assertIsChecked(true)
        Log.i(TAG, "selectOnlyCookiesCheckBox: Verified that the \"Cookies and site data\" check box is checked")

        clickCachedFilesCheckBox()
        Log.i(TAG, "selectOnlyCookiesCheckBox: Trying to verify that the \"Cached images and files\" check box is not checked")
        cachedFilesCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyCookiesCheckBox: Verified that the \"Cached images and files\" check box is not checked")

        clickSitePermissionsCheckBox()
        Log.i(TAG, "selectOnlyCookiesCheckBox: Trying to verify that the \"Site permissions\" check box is not checked")
        sitePermissionsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyCookiesCheckBox: Verified that the \"Site permissions\" check box is not checked")

        clickDownloadsCheckBox()
        Log.i(TAG, "selectOnlyCookiesCheckBox: Trying to verify that the \"Downloads\" check box is not checked")
        downloadsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyCookiesCheckBox: Verified that the \"Downloads\" check box is not checked")

        clickBrowsingHistoryCheckBox()
        Log.i(TAG, "selectOnlyCookiesCheckBox: Trying to verify that the \"Browsing history\" check box is not checked")
        browsingHistoryCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyCookiesCheckBox: Verified that the \"Browsing history\" check box is not checked")
    }

    fun selectOnlyCachedFilesCheckBox() {
        clickOpenTabsCheckBox()
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Trying to verify that the \"Open tabs\" check box is not checked")
        openTabsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Verified that the \"Open tabs\" check box is not checked")

        clickBrowsingHistoryCheckBox()
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Trying to verify that the \"Browsing history\" check box is not checked")
        browsingHistoryCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Verified that the \"Browsing history\" check box is not checked")

        clickCookiesCheckBox()
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Trying to verify that the \"Cookies and site data\" check box is not checked")
        cookiesAndSiteDataCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Verified that the \"Cookies and site data\" check box is not checked")
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Trying to verify that the \"Cached images and files\" check box is checked")
        cachedFilesCheckBox().assertIsChecked(true)
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Verified that the \"Cached images and files\" check box is checked")

        clickSitePermissionsCheckBox()
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Trying to verify that the \"Site permissions\" check box is not checked")
        sitePermissionsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Verified that the \"Site permissions\" check box is not checked")

        clickDownloadsCheckBox()
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Trying to verify that the \"Downloads\" check box is not checked")
        downloadsCheckBox().assertIsChecked(false)
        Log.i(TAG, "selectOnlyCachedFilesCheckBox: Verified that the \"Downloads\" check box is not checked")
    }

    fun confirmDeletionAndAssertSnackbar() {
        Log.i(TAG, "confirmDeletionAndAssertSnackbar: Trying to click the delete browsing data dialog \"Delete\" button")
        dialogDeleteButton().click()
        Log.i(TAG, "confirmDeletionAndAssertSnackbar: Clicked the delete browsing data dialog \"Delete\" button")
        assertDeleteBrowsingDataSnackbar()
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Trying to click navigate up toolbar button")
            goBackButton().click()
            Log.i(TAG, "goBack: Clicked the navigate up toolbar button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

private fun deleteBrowsingDataButton() = onView(withId(R.id.delete_data))

private fun dialogDeleteButton() = onView(withText("Delete")).inRoot(isDialog())

private fun dialogCancelButton() = onView(withText("Cancel")).inRoot(isDialog())

private fun openTabsDescription(tabNumber: String) = onView(withText("$tabNumber tabs"))

private fun openTabsCheckBox() = onView(allOf(withId(R.id.checkbox), hasSibling(withText("Open tabs"))))

private fun browsingHistoryDescription(addresses: String) = mDevice.findObject(UiSelector().textContains("$addresses addresses"))

private fun browsingHistoryCheckBox() =
    onView(allOf(withId(R.id.checkbox), hasSibling(withText("Browsing history"))))

private fun cookiesAndSiteDataCheckBox() =
    onView(allOf(withId(R.id.checkbox), hasSibling(withText("Cookies and site data"))))

private fun cachedFilesCheckBox() =
    onView(allOf(withId(R.id.checkbox), hasSibling(withText("Cached images and files"))))

private fun sitePermissionsCheckBox() =
    onView(allOf(withId(R.id.checkbox), hasSibling(withText("Site permissions"))))

private fun downloadsCheckBox() =
    onView(allOf(withId(R.id.checkbox), hasSibling(withText("Downloads"))))

private fun dialogMessage() =
    onView(withText("$appName will delete the selected browsing data."))
        .inRoot(isDialog())

private fun assertDeleteBrowsingDataSnackbar() = assertUIObjectIsGone(itemWithText("Browsing data deleted"))
private fun clickOpenTabsCheckBox() {
    Log.i(TAG, "clickOpenTabsCheckBox: Trying to click the \"Open tabs\" check box")
    openTabsCheckBox().click()
    Log.i(TAG, "clickOpenTabsCheckBox: Clicked the \"Open tabs\" check box")
}
private fun clickBrowsingHistoryCheckBox() {
    Log.i(TAG, "clickBrowsingHistoryCheckBox: Trying to click the \"Browsing history\" check box")
    browsingHistoryCheckBox().click()
    Log.i(TAG, "clickBrowsingHistoryCheckBox: Clicked the \"Browsing history\" check box")
}
private fun clickCookiesCheckBox() {
    Log.i(TAG, "clickCookiesCheckBox: Trying to click the \"Cookies and site data\" check box")
    cookiesAndSiteDataCheckBox().click()
    Log.i(TAG, "clickCookiesCheckBox: Clicked the \"Cookies and site data\" check box")
}
private fun clickCachedFilesCheckBox() {
    Log.i(TAG, "clickCachedFilesCheckBox: Trying to click the \"Cached images and files\" check box")
    cachedFilesCheckBox().click()
    Log.i(TAG, "clickCachedFilesCheckBox: Clicked the \"Cached images and files\" check box")
}
private fun clickSitePermissionsCheckBox() {
    Log.i(TAG, "clickSitePermissionsCheckBox: Trying to click the \"Site permissions\" check box")
    sitePermissionsCheckBox().click()
    Log.i(TAG, "clickSitePermissionsCheckBox: Clicked the \"Site permissions\" check box")
}
private fun clickDownloadsCheckBox() {
    Log.i(TAG, "clickDownloadsCheckBox: Trying to click the \"Downloads\" check box")
    downloadsCheckBox().click()
    Log.i(TAG, "clickDownloadsCheckBox: Clicked the \"Downloads\" check box")
}
