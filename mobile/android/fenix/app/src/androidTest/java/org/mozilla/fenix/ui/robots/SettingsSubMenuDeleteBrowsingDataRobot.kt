/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

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
        openTabsCheckBox().assertIsChecked(true)
        browsingHistoryCheckBox().assertIsChecked(true)
        cookiesAndSiteDataCheckBox().assertIsChecked(true)
        cachedFilesCheckBox().assertIsChecked(true)
        sitePermissionsCheckBox().assertIsChecked(true)
        downloadsCheckBox().assertIsChecked(true)
    }
    fun verifyOpenTabsCheckBox(status: Boolean) = openTabsCheckBox().assertIsChecked(status)
    fun verifyBrowsingHistoryDetails(status: Boolean) = browsingHistoryCheckBox().assertIsChecked(status)
    fun verifyCookiesCheckBox(status: Boolean) = cookiesAndSiteDataCheckBox().assertIsChecked(status)
    fun verifyCachedFilesCheckBox(status: Boolean) = cachedFilesCheckBox().assertIsChecked(status)
    fun verifySitePermissionsCheckBox(status: Boolean) = sitePermissionsCheckBox().assertIsChecked(status)
    fun verifyDownloadsCheckBox(status: Boolean) = downloadsCheckBox().assertIsChecked(status)
    fun verifyOpenTabsDetails(tabNumber: String) = openTabsDescription(tabNumber).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    fun verifyBrowsingHistoryDetails(addresses: String) = assertUIObjectExists(browsingHistoryDescription(addresses))

    fun verifyDeleteBrowsingDataDialog() {
        dialogMessage().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        dialogCancelButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        dialogDeleteButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    fun switchOpenTabsCheckBox() = clickOpenTabsCheckBox()
    fun switchBrowsingHistoryCheckBox() = clickBrowsingHistoryCheckBox()
    fun switchCookiesCheckBox() = clickCookiesCheckBox()
    fun switchCachedFilesCheckBox() = clickCachedFilesCheckBox()
    fun switchSitePermissionsCheckBox() = clickSitePermissionsCheckBox()
    fun switchDownloadsCheckBox() = clickDownloadsCheckBox()
    fun clickDeleteBrowsingDataButton() = deleteBrowsingDataButton().click()
    fun clickDialogCancelButton() = dialogCancelButton().click()

    fun selectOnlyOpenTabsCheckBox() {
        clickBrowsingHistoryCheckBox()
        browsingHistoryCheckBox().assertIsChecked(false)

        clickCookiesCheckBox()
        cookiesAndSiteDataCheckBox().assertIsChecked(false)

        clickCachedFilesCheckBox()
        cachedFilesCheckBox().assertIsChecked(false)

        clickSitePermissionsCheckBox()
        sitePermissionsCheckBox().assertIsChecked(false)

        clickDownloadsCheckBox()
        downloadsCheckBox().assertIsChecked(false)

        openTabsCheckBox().assertIsChecked(true)
    }

    fun selectOnlyBrowsingHistoryCheckBox() {
        clickOpenTabsCheckBox()
        openTabsCheckBox().assertIsChecked(false)

        clickCookiesCheckBox()
        cookiesAndSiteDataCheckBox().assertIsChecked(false)

        clickCachedFilesCheckBox()
        cachedFilesCheckBox().assertIsChecked(false)

        clickSitePermissionsCheckBox()
        sitePermissionsCheckBox().assertIsChecked(false)

        clickDownloadsCheckBox()
        downloadsCheckBox().assertIsChecked(false)

        browsingHistoryCheckBox().assertIsChecked(true)
    }

    fun selectOnlyCookiesCheckBox() {
        clickOpenTabsCheckBox()
        openTabsCheckBox().assertIsChecked(false)

        cookiesAndSiteDataCheckBox().assertIsChecked(true)

        clickCachedFilesCheckBox()
        cachedFilesCheckBox().assertIsChecked(false)

        clickSitePermissionsCheckBox()
        sitePermissionsCheckBox().assertIsChecked(false)

        clickDownloadsCheckBox()
        downloadsCheckBox().assertIsChecked(false)

        clickBrowsingHistoryCheckBox()
        browsingHistoryCheckBox().assertIsChecked(false)
    }

    fun selectOnlyCachedFilesCheckBox() {
        clickOpenTabsCheckBox()
        openTabsCheckBox().assertIsChecked(false)

        clickBrowsingHistoryCheckBox()
        browsingHistoryCheckBox().assertIsChecked(false)

        clickCookiesCheckBox()
        cookiesAndSiteDataCheckBox().assertIsChecked(false)

        cachedFilesCheckBox().assertIsChecked(true)

        clickSitePermissionsCheckBox()
        sitePermissionsCheckBox().assertIsChecked(false)

        clickDownloadsCheckBox()
        downloadsCheckBox().assertIsChecked(false)
    }

    fun confirmDeletionAndAssertSnackbar() {
        dialogDeleteButton().click()
        assertDeleteBrowsingDataSnackbar()
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().click()

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
private fun clickOpenTabsCheckBox() = openTabsCheckBox().click()
private fun clickBrowsingHistoryCheckBox() = browsingHistoryCheckBox().click()
private fun clickCookiesCheckBox() = cookiesAndSiteDataCheckBox().click()
private fun clickCachedFilesCheckBox() = cachedFilesCheckBox().click()
private fun clickSitePermissionsCheckBox() = sitePermissionsCheckBox().click()
private fun clickDownloadsCheckBox() = downloadsCheckBox().click()
