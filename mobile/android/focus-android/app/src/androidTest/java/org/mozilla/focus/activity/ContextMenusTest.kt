/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.DeleteFilesHelper.deleteFileUsingDisplayName
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityIntentsTestRule
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.StringsHelper
import org.mozilla.focus.helpers.TestAssetHelper.getGenericTabAsset
import org.mozilla.focus.helpers.TestAssetHelper.getImageTestAsset
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.focus.helpers.TestHelper.getTargetContext
import org.mozilla.focus.helpers.TestHelper.permAllowBtn
import org.mozilla.focus.testAnnotations.SmokeTest

// These tests check the interaction with various context menu options
@RunWith(AndroidJUnit4ClassRunner::class)
class ContextMenusTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityIntentsTestRule(showFirstRun = false)

    @get: Rule
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setup() {
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)

        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun linkedImageContextMenuItemsTest() {
        val imagesTestPage = getImageTestAsset(webServer)
        val imageAssetUrl = webServer.url("download.jpg").toString()

        searchScreen {
        }.loadPage(imagesTestPage.url) {
            longPressLink("download icon")
            verifyImageContextMenu(true, imageAssetUrl)
        }
    }

    @SmokeTest
    @Test
    fun simpleImageContextMenuItemsTest() {
        val imagesTestPage = getImageTestAsset(webServer)
        val imageAssetUrl = webServer.url("rabbit.jpg").toString()

        searchScreen {
        }.loadPage(imagesTestPage.url) {
            longPressLink("rabbit.jpg")
            verifyImageContextMenu(false, imageAssetUrl)
        }
    }

    @SmokeTest
    @Test
    fun linkContextMenuItemsTest() {
        val tab1Page = getGenericTabAsset(webServer, 1)
        val tab2Page = getGenericTabAsset(webServer, 2)

        searchScreen {
        }.loadPage(tab1Page.url) {
            verifyPageContent("Tab 1")
            longPressLink("Tab 2")
            verifyLinkContextMenu(tab2Page.url)
        }
    }

    @SmokeTest
    @Test
    fun copyLinkAddressTest() {
        val tab1Page = getGenericTabAsset(webServer, 1)
        val tab2Page = getGenericTabAsset(webServer, 2)

        searchScreen {
        }.loadPage(tab1Page.url) {
            verifyPageContent("Tab 1")
            longPressLink("Tab 2")
            verifyLinkContextMenu(tab2Page.url)
            clickContextMenuCopyLink()
        }.openSearchBar {
            clearSearchBar()
            longPressSearchBar()
        }.pasteAndLoadLink {
            progressBar.waitUntilGone(TestHelper.waitingTime)
            verifyPageURL(tab2Page.url)
        }
    }

    @SmokeTest
    @Test
    fun shareLinkTest() {
        val tab1Page = getGenericTabAsset(webServer, 1)
        val tab2Page = getGenericTabAsset(webServer, 2)

        searchScreen {
        }.loadPage(tab1Page.url) {
            verifyPageContent("Tab 1")
            longPressLink("Tab 2")
            verifyLinkContextMenu(tab2Page.url)
            clickShareLink()
            verifyShareAppsListOpened()
        }
    }

    @Test
    fun copyImageLocationTest() {
        val imagesTestPage = getImageTestAsset(webServer)
        val imageAssetUrl = webServer.url("rabbit.jpg").toString()

        searchScreen {
        }.loadPage(imagesTestPage.url) {
            longPressLink("rabbit.jpg")
            verifyImageContextMenu(false, imageAssetUrl)
            clickCopyImageLocation()
        }.openSearchBar {
            clearSearchBar()
            longPressSearchBar()
        }.pasteAndLoadLink {
            progressBar.waitUntilGone(TestHelper.waitingTime)
            verifyPageURL(imageAssetUrl)
        }
    }

    @SmokeTest
    @Test
    fun saveImageTest() {
        val imagesTestPage = getImageTestAsset(webServer)
        val fileName = "rabbit.jpg"

        searchScreen {
        }.loadPage(imagesTestPage.url) {
            longPressLink(fileName)
        }.clickSaveImage {
            // If permission dialog appears on devices with API<30, grant it
            if (permAllowBtn.exists()) {
                permAllowBtn.click()
            }
            verifyDownloadConfirmationMessage(fileName)
            openDownloadedFile()
            assertNativeAppOpens(StringsHelper.GOOGLE_PHOTOS)
        }
        deleteFileUsingDisplayName(
            getTargetContext.applicationContext,
            fileName
        )
    }

    @Test
    fun shareImageTest() {
        val imagesTestPage = getImageTestAsset(webServer)

        searchScreen {
        }.loadPage(imagesTestPage.url) {
            longPressLink("rabbit.jpg")
            clickShareImage()
            verifyShareAppsListOpened()
        }
    }
}
