package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic functionality of Library accessed from browser screen
 */
class LibraryMenuTest {

    private lateinit var mockWebServer: MockWebServer
    private lateinit var defaultWebPage: TestAssetHelper.TestAsset

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            setDispatcher(AndroidAssetDispatcher())
            start()
        }
        defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun libraryMenuItemsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
            verifyLibraryView()
            verifyHistoryButton()
            verifyBookmarksButton()
        }
    }

    @Test
    fun backButtonTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
        }.goBack {
            verifyBrowserScreen()
        }
    }

    @Test
    fun bookmarksButtonTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
            verifyBookmarksMenuView()
        }
    }

    @Test
    fun historyButtonTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openHistory {
            verifyHistoryMenuView()
        }
    }
}
