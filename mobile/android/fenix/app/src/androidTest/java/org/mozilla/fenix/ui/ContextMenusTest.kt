/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.downloadRobot
import org.mozilla.fenix.ui.robots.navigationToolbar

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
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            setDispatcher(AndroidAssetDispatcher())
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    @Ignore("Disabling because of intermittent failures https://github.com/mozilla-mobile/fenix/issues/8663")
    fun verifyContextOpenLinkNewTab() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            verifyPageContent(pageLinks.content)
            longClickMatchingText("Link 1")
            verifyLinkContextMenuItems(genericURL.url)
            clickContextOpenLinkInNewTab()
            verifySnackBarText("New tab opened")
            snackBarButtonClick("Switch")
            verifyUrl(genericURL.url.toString())
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            verifyExistingOpenTabs("Test_Page_4")
        }
    }

    @Ignore("Intermittent failure - https://github.com/mozilla-mobile/fenix/issues/10586")
    @Test
    fun verifyContextOpenLinkPrivateTab() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            verifyPageContent(pageLinks.content)
            longClickMatchingText("Link 2")
            verifyLinkContextMenuItems(genericURL.url)
            clickContextOpenLinkInPrivateTab()
            verifySnackBarText("New private tab opened")
            snackBarButtonClick("Switch")
            verifyUrl(genericURL.url.toString())
        }.openTabDrawer {
            verifyPrivateModeSelected()
            verifyExistingOpenTabs("Test_Page_2")
        }
    }

    @Ignore("Intermittent failure - https://github.com/mozilla-mobile/fenix/issues/8832")
    @Test
    fun verifyContextCopyLink() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            verifyPageContent(pageLinks.content)
            longClickMatchingText("Link 3")
            verifyLinkContextMenuItems(genericURL.url)
            clickContextCopyLink()
            verifySnackBarText("Link copied to clipboard")
        }.openNavigationToolbar {
        }.visitLinkFromClipboard {
            verifyUrl(genericURL.url.toString())
        }
    }

    @Ignore("Intermittent failure - https://github.com/mozilla-mobile/fenix/issues/10586")
    @Test
    fun verifyContextShareLink() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            verifyPageContent(pageLinks.content)
            longClickMatchingText("Link 1")
            verifyLinkContextMenuItems(genericURL.url)
            clickContextShareLink(genericURL.url) // verify share intent is matched with associated URL
        }
    }

    @Ignore("Temp disable intermittent failure - https://github.com/mozilla-mobile/fenix/issues/7687")
    @Test
    fun verifyContextOpenImageNewTab() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val imageResource =
            TestAssetHelper.getImageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            verifyPageContent(pageLinks.content)
            longClickMatchingText("test_link_image")
            verifyLinkImageContextMenuItems(imageResource.url)
            clickContextOpenImageNewTab()
            verifySnackBarText("New tab opened")
            snackBarButtonClick("Switch")
            verifyUrl(imageResource.url.toString())
        }
    }

    @Ignore("Temp disable intermittent failure - https://github.com/mozilla-mobile/fenix/issues/7687")
    @Test
    fun verifyContextCopyImageLocation() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val imageResource =
            TestAssetHelper.getImageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            verifyPageContent(pageLinks.content)
            longClickMatchingText("test_link_image")
            verifyLinkImageContextMenuItems(imageResource.url)
            clickContextCopyImageLocation()
            verifySnackBarText("Link copied to clipboard")
        }.openNavigationToolbar {
        }.visitLinkFromClipboard {
            verifyUrl(imageResource.url.toString())
        }
    }

    @Ignore("Temp disable intermittent failure - https://github.com/mozilla-mobile/fenix/issues/7666")
    @Test
    fun verifyContextSaveImage() {
        val pageLinks =
            TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val imageResource =
            TestAssetHelper.getImageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pageLinks.url) {
            verifyPageContent(pageLinks.content)
            longClickMatchingText("test_link_image")
            verifyLinkImageContextMenuItems(imageResource.url)
            clickContextSaveImage()
        }

        downloadRobot {
        }.clickAllowPermission {
            verifyDownloadNotificationPopup()
        }.clickOpen("image/jpeg") {} // verify open intent is matched with associated data type
        downloadRobot {
            verifyPhotosAppOpens()
        }
    }

    @Ignore("Temp disable intermittent failure - https://github.com/mozilla-mobile/fenix/issues/7693")
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
            verifyPageContent(pageLinks.content)
            longClickMatchingText("Link 1")
            verifyLinkContextMenuItems(genericURL.url)
            mDevice.pressBack()
            longClickMatchingText("test_link_image")
            verifyLinkImageContextMenuItems(imageResource.url)
            mDevice.pressBack()
            longClickMatchingText("test_no_link_image")
            verifyNoLinkImageContextMenuItems("test_no_link_image")
        }
    }
}
