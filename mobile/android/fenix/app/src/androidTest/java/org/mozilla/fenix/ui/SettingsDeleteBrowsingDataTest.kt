/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AppAndSystemHelper.setNetworkEnabled
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.getStorageTestAsset
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.settingsScreen

/**
 *  Tests for verifying the Settings for:
 *  Delete Browsing Data
 */

class SettingsDeleteBrowsingDataTest : TestSetup() {
    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/937561
    @Test
    fun deleteBrowsingDataOptionStatesTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyAllCheckBoxesAreChecked()
            switchBrowsingHistoryCheckBox()
            switchCachedFilesCheckBox()
            verifyOpenTabsCheckBox(true)
            verifyBrowsingHistoryDetails(false)
            verifyCookiesCheckBox(true)
            verifyCachedFilesCheckBox(false)
            verifySitePermissionsCheckBox(true)
            verifyDownloadsCheckBox(true)
        }

        restartApp(activityTestRule)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyOpenTabsCheckBox(true)
            verifyBrowsingHistoryDetails(false)
            verifyCookiesCheckBox(true)
            verifyCachedFilesCheckBox(false)
            verifySitePermissionsCheckBox(true)
            verifyDownloadsCheckBox(true)
            switchOpenTabsCheckBox()
            switchBrowsingHistoryCheckBox()
            switchCookiesCheckBox()
            switchCachedFilesCheckBox()
            switchSitePermissionsCheckBox()
            switchDownloadsCheckBox()
            verifyOpenTabsCheckBox(false)
            verifyBrowsingHistoryDetails(true)
            verifyCookiesCheckBox(false)
            verifyCachedFilesCheckBox(true)
            verifySitePermissionsCheckBox(false)
            verifyDownloadsCheckBox(false)
        }

        restartApp(activityTestRule)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyOpenTabsCheckBox(false)
            verifyBrowsingHistoryDetails(true)
            verifyCookiesCheckBox(false)
            verifyCachedFilesCheckBox(true)
            verifySitePermissionsCheckBox(false)
            verifyDownloadsCheckBox(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/517811
    @Test
    fun deleteOpenTabsBrowsingDataWithNoOpenTabsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyAllCheckBoxesAreChecked()
            selectOnlyOpenTabsCheckBox()
            clickDeleteBrowsingDataButton()
            verifyDeleteBrowsingDataDialog()
            confirmDeletionAndAssertSnackbar()
        }
        settingsScreen {
            verifyGeneralHeading()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/353531
    @SmokeTest
    @Test
    fun deleteOpenTabsBrowsingDataTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyAllCheckBoxesAreChecked()
            selectOnlyOpenTabsCheckBox()
            clickDeleteBrowsingDataButton()
            verifyDeleteBrowsingDataDialog()
            clickDialogCancelButton()
            verifyOpenTabsCheckBox(true)
            clickDeleteBrowsingDataButton()
            verifyDeleteBrowsingDataDialog()
            confirmDeletionAndAssertSnackbar()
        }
        settingsScreen {
            verifyGeneralHeading()
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyOpenTabsDetails("0")
        }.goBack {
        }.goBack {
        }.openTabDrawer {
            verifyNoOpenTabsInNormalBrowsing()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/378864
    @SmokeTest
    @Test
    fun deleteBrowsingHistoryTest() {
        val genericPage = getStorageTestAsset(mockWebServer, "generic1.html").url

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage) {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyBrowsingHistoryDetails("1")
            selectOnlyBrowsingHistoryCheckBox()
            clickDeleteBrowsingDataButton()
            verifyDeleteBrowsingDataDialog()
            clickDialogCancelButton()
            verifyBrowsingHistoryDetails(true)
            clickDeleteBrowsingDataButton()
            verifyDeleteBrowsingDataDialog()
            confirmDeletionAndAssertSnackbar()
            verifyBrowsingHistoryDetails("0")
            exitMenu()
        }
        navigationToolbar {
        }.openThreeDotMenu {
        }.openHistory {
            verifyEmptyHistoryView()
            mDevice.pressBack()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/416041
    @SmokeTest
    @Test
    fun deleteCookiesAndSiteDataTest() {
        val genericPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val storageWritePage = getStorageTestAsset(mockWebServer, "storage_write.html").url
        val storageCheckPage = getStorageTestAsset(mockWebServer, "storage_check.html").url

        // Browsing a generic page to allow GV to load on a fresh run
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(storageWritePage) {
            verifyPageContent("No cookies set")
            clickPageObject(itemWithResId("setCookies"))
            verifyPageContent("user=android")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(storageCheckPage) {
            verifyPageContent("Session storage has value")
            verifyPageContent("Local storage has value")
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            selectOnlyCookiesCheckBox()
            clickDeleteBrowsingDataButton()
            verifyDeleteBrowsingDataDialog()
            clickDialogCancelButton()
            verifyCookiesCheckBox(status = true)
            clickDeleteBrowsingDataButton()
            verifyDeleteBrowsingDataDialog()
            confirmDeletionAndAssertSnackbar()
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(storageCheckPage) {
            verifyPageContent("Session storage empty")
            verifyPageContent("Local storage empty")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(storageWritePage) {
            verifyPageContent("No cookies set")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/416042
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1807268")
    @SmokeTest
    @Test
    fun deleteCachedFilesTest() {
        val pocketTopArticles = getStringResource(R.string.pocket_pinned_top_articles)

        homeScreen {
            verifyExistingTopSitesTabs(pocketTopArticles)
        }.openTopSiteTabWithTitle(pocketTopArticles) {
            waitForPageToLoad()
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery("about:cache") {
            // disabling wifi to prevent downloads in the background
            setNetworkEnabled(enabled = false)
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            selectOnlyCachedFilesCheckBox()
            clickDeleteBrowsingDataButton()
            verifyDeleteBrowsingDataDialog()
            confirmDeletionAndAssertSnackbar()
            exitMenu()
        }
        browserScreen {
        }.openThreeDotMenu {
        }.refreshPage {
            verifyNetworkCacheIsEmpty("memory")
            verifyNetworkCacheIsEmpty("disk")
        }
        setNetworkEnabled(enabled = true)
    }
}
