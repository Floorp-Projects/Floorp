/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
import org.mozilla.fenix.ui.robots.clickContextMenuItem
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.downloadRobot
import org.mozilla.fenix.ui.robots.longClickPageObject
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.shareOverlay

/**
 *  Tests for verifying basic functionality of content context menus
 *
 *  - Verifies long click "Open link in new tab" UI and functionality
 *  - Verifies long click "Open link in new Private tab" UI and functionality
 *  - Verifies long click "Copy Link" UI and functionality
 *  - Verifies long click "Share Link" UI and functionality
 *  - Verifies long click "Open image in new tab" UI and functionality
 *  - Verifies long click "Save Image" UI and functionality
 *  - Verifies long click "Copy image location" UI and functionality
 *  - Verifies long click items of mixed hypertext items
 *
 */

class ContextMenusTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        activityIntentTestRule.activity.applicationContext.settings().shouldShowJumpBackInCFR = false
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
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
    fun verifyContextOpenLinkNewTab() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            mDevice.waitForIdle()
            longClickPageObject(itemWithText("Link 1"))
            verifyLinkContextMenuItems(genericURL.url)
            clickContextMenuItem("Open link in new tab")
            verifySnackBarText("New tab opened")
            clickSnackbarButton("SWITCH")
            verifyUrl(genericURL.url.toString())
        }.openTabDrawer {
            verifyNormalModeSelected()
            verifyExistingOpenTabs("Test_Page_1")
            verifyExistingOpenTabs("Test_Page_4")
        }
    }

    @SmokeTest
    @Test
    fun verifyContextOpenLinkPrivateTab() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            mDevice.waitForIdle()
            longClickPageObject(itemWithText("Link 2"))
            verifyLinkContextMenuItems(genericURL.url)
            clickContextMenuItem("Open link in private tab")
            verifySnackBarText("New private tab opened")
            clickSnackbarButton("SWITCH")
            verifyUrl(genericURL.url.toString())
        }.openTabDrawer {
            verifyPrivateModeSelected()
            verifyExistingOpenTabs("Test_Page_2")
        }
    }

    @Test
    fun verifyContextCopyLink() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            mDevice.waitForIdle()
            longClickPageObject(itemWithText("Link 3"))
            verifyLinkContextMenuItems(genericURL.url)
            clickContextMenuItem("Copy link")
            verifySnackBarText("Link copied to clipboard")
        }.openNavigationToolbar {
        }.visitLinkFromClipboard {
            verifyUrl(genericURL.url.toString())
        }
    }

    @Test
    fun verifyContextCopyLinkNotDisplayedAfterApplied() {
        val pageLinks = TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            longClickPageObject(itemWithText("Link 3"))
            verifyLinkContextMenuItems(genericURL.url)
            clickContextMenuItem("Copy link")
            verifySnackBarText("Link copied to clipboard")
        }.openNavigationToolbar {
        }.visitLinkFromClipboard {
            verifyUrl(genericURL.url.toString())
        }.openTabDrawer {
        }.openNewTab {
        }
        navigationToolbar {
            verifyClipboardSuggestionsAreDisplayed(shouldBeDisplayed = false)
        }
    }

    @Test
    fun verifyContextShareLink() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            mDevice.waitForIdle()
            longClickPageObject(itemWithText("Link 1"))
            verifyLinkContextMenuItems(genericURL.url)
            clickContextMenuItem("Share link")
            shareOverlay {
                verifyShareLinkIntent(genericURL.url)
            }
        }
    }

    @Test
    fun verifyContextOpenImageNewTab() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val imageResource =
            TestAssetHelper.getImageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            mDevice.waitForIdle()
            longClickPageObject(itemWithText("test_link_image"))
            verifyLinkImageContextMenuItems(imageResource.url)
            clickContextMenuItem("Open image in new tab")
            verifySnackBarText("New tab opened")
            clickSnackbarButton("SWITCH")
            verifyUrl(imageResource.url.toString())
        }
    }

    @Test
    fun verifyContextCopyImageLocation() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val imageResource =
            TestAssetHelper.getImageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            mDevice.waitForIdle()
            longClickPageObject(itemWithText("test_link_image"))
            verifyLinkImageContextMenuItems(imageResource.url)
            clickContextMenuItem("Copy image location")
            verifySnackBarText("Link copied to clipboard")
        }.openNavigationToolbar {
        }.visitLinkFromClipboard {
            verifyUrl(imageResource.url.toString())
        }
    }

    @Test
    fun verifyContextSaveImage() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val imageResource =
            TestAssetHelper.getImageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            mDevice.waitForIdle()
            longClickPageObject(itemWithText("test_link_image"))
            verifyLinkImageContextMenuItems(imageResource.url)
            clickContextMenuItem("Save image")
        }

        downloadRobot {
            verifyDownloadNotificationPopup()
        }.clickOpen("image/jpeg") {} // verify open intent is matched with associated data type
        downloadRobot {
            verifyPhotosAppOpens()
        }
    }

    @Test
    fun verifyContextMixedVariations() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val imageResource =
            TestAssetHelper.getImageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            mDevice.waitForIdle()
            longClickPageObject(itemWithText("Link 1"))
            verifyLinkContextMenuItems(genericURL.url)
            dismissContentContextMenu()
            longClickPageObject(itemWithText("test_link_image"))
            verifyLinkImageContextMenuItems(imageResource.url)
            dismissContentContextMenu()
            longClickPageObject(itemWithText("test_no_link_image"))
            verifyNoLinkImageContextMenuItems(imageResource.url)
        }
    }

    @Test
    fun verifyContextMixedVariationsInPDFTest() {
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF file"))
            longClickPageObject(itemWithText("Wikipedia link"))
            verifyLinkContextMenuItems("wikipedia.org".toUri(), false)
            dismissContentContextMenu()
            // Some options are missing from the linked and non liked images context menus in PDF files
            // See https://bugzilla.mozilla.org/show_bug.cgi?id=1012805 for more details
            longClickPDFImage()
            verifyLinkContextMenuItems("wikipedia.org".toUri())
            dismissContentContextMenu()
        }
    }
}
