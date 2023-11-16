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

    fun verifyAllCheckBoxesAreChecked() = assertAllCheckBoxesAreChecked()
    fun verifyOpenTabsCheckBox(status: Boolean) = assertOpenTabsCheckBox(status)
    fun verifyBrowsingHistoryDetails(status: Boolean) = assertBrowsingHistoryCheckBox(status)
    fun verifyCookiesCheckBox(status: Boolean) = assertCookiesCheckBox(status)
    fun verifyCachedFilesCheckBox(status: Boolean) = assertCachedFilesCheckBox(status)
    fun verifySitePermissionsCheckBox(status: Boolean) = assertSitePermissionsCheckBox(status)
    fun verifyDownloadsCheckBox(status: Boolean) = assertDownloadsCheckBox(status)
    fun verifyOpenTabsDetails(tabNumber: String) = assertOpenTabsDescription(tabNumber)
    fun verifyBrowsingHistoryDetails(addresses: String) = assertBrowsingHistoryDescription(addresses)

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
        assertBrowsingHistoryCheckBox(false)

        clickCookiesCheckBox()
        assertCookiesCheckBox(false)

        clickCachedFilesCheckBox()
        assertCachedFilesCheckBox(false)

        clickSitePermissionsCheckBox()
        assertSitePermissionsCheckBox(false)

        clickDownloadsCheckBox()
        assertDownloadsCheckBox(false)

        assertOpenTabsCheckBox(true)
    }

    fun selectOnlyBrowsingHistoryCheckBox() {
        clickOpenTabsCheckBox()
        assertOpenTabsCheckBox(false)

        clickCookiesCheckBox()
        assertCookiesCheckBox(false)

        clickCachedFilesCheckBox()
        assertCachedFilesCheckBox(false)

        clickSitePermissionsCheckBox()
        assertSitePermissionsCheckBox(false)

        clickDownloadsCheckBox()
        assertDownloadsCheckBox(false)

        assertBrowsingHistoryCheckBox(true)
    }

    fun selectOnlyCookiesCheckBox() {
        clickOpenTabsCheckBox()
        assertOpenTabsCheckBox(false)

        assertCookiesCheckBox(true)

        clickCachedFilesCheckBox()
        assertCachedFilesCheckBox(false)

        clickSitePermissionsCheckBox()
        assertSitePermissionsCheckBox(false)

        clickDownloadsCheckBox()
        assertDownloadsCheckBox(false)

        clickBrowsingHistoryCheckBox()
        assertBrowsingHistoryCheckBox(false)
    }

    fun selectOnlyCachedFilesCheckBox() {
        clickOpenTabsCheckBox()
        assertOpenTabsCheckBox(false)

        clickBrowsingHistoryCheckBox()
        assertBrowsingHistoryCheckBox(false)

        clickCookiesCheckBox()
        assertCookiesCheckBox(false)

        assertCachedFilesCheckBox(true)

        clickSitePermissionsCheckBox()
        assertSitePermissionsCheckBox(false)

        clickDownloadsCheckBox()
        assertDownloadsCheckBox(false)
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

private fun assertAllCheckBoxesAreChecked() {
    openTabsCheckBox().assertIsChecked(true)
    browsingHistoryCheckBox().assertIsChecked(true)
    cookiesAndSiteDataCheckBox().assertIsChecked(true)
    cachedFilesCheckBox().assertIsChecked(true)
    sitePermissionsCheckBox().assertIsChecked(true)
    downloadsCheckBox().assertIsChecked(true)
}

private fun assertOpenTabsDescription(tabNumber: String) =
    openTabsDescription(tabNumber).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertBrowsingHistoryDescription(addresses: String) =
    assertUIObjectExists(browsingHistoryDescription(addresses))

private fun assertDeleteBrowsingDataSnackbar() = assertUIObjectIsGone(itemWithText("Browsing data deleted"))

private fun clickOpenTabsCheckBox() = openTabsCheckBox().click()
private fun assertOpenTabsCheckBox(status: Boolean) = openTabsCheckBox().assertIsChecked(status)
private fun clickBrowsingHistoryCheckBox() = browsingHistoryCheckBox().click()
private fun assertBrowsingHistoryCheckBox(status: Boolean) = browsingHistoryCheckBox().assertIsChecked(status)
private fun clickCookiesCheckBox() = cookiesAndSiteDataCheckBox().click()
private fun assertCookiesCheckBox(status: Boolean) = cookiesAndSiteDataCheckBox().assertIsChecked(status)
private fun clickCachedFilesCheckBox() = cachedFilesCheckBox().click()
private fun assertCachedFilesCheckBox(status: Boolean) = cachedFilesCheckBox().assertIsChecked(status)
private fun clickSitePermissionsCheckBox() = sitePermissionsCheckBox().click()
private fun assertSitePermissionsCheckBox(status: Boolean) = sitePermissionsCheckBox().assertIsChecked(status)
private fun clickDownloadsCheckBox() = downloadsCheckBox().click()
private fun assertDownloadsCheckBox(status: Boolean) = downloadsCheckBox().assertIsChecked(status)
