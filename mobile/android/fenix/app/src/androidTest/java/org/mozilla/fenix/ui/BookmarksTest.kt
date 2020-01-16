/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu
import androidx.test.platform.app.InstrumentationRegistry.getInstrumentation
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.BookmarkRoot
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.longTapSelectItem
import org.mozilla.fenix.ui.robots.bookmarksMenu
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.multipleSelectionToolbar
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic functionality of bookmarks
 */
class BookmarksTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

    private lateinit var mockWebServer: MockWebServer
    private val bookmarksFolderName = "New Folder"

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

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
        // Clearing all bookmarks data after each test to avoid overlapping data
        val bookmarksStorage = activityTestRule.activity?.bookmarkStorage
        runBlocking {
            val bookmarks = bookmarksStorage?.getTree(BookmarkRoot.Mobile.id)?.children
            bookmarks?.forEach { bookmarksStorage.deleteNode(it.guid) }
        }
    }

    @Test
    fun noBookmarkItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyEmptyBookmarksList()
        }
    }

    @Test
    fun verifyBookmarkButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            verifyAddBookmarkButton()
            clickAddBookmarkButton()
        }
        browserScreen {
        }.openThreeDotMenu {
            verifyEditBookmarkButton()
        }
    }

    @Test
    fun addBookmarkTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
            verifyBookmarkedURL(defaultWebPage.url)
            verifyBookmarkFavicon()
        }
    }

    @Test
    fun createBookmarkFolderTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            clickAddFolderButton()
            verifyKeyboardVisible()
            addNewFolderName(bookmarksFolderName)
            saveNewFolder()
            verifyFolderTitle(bookmarksFolderName)
            verifyKeyboardHidden()
        }
    }

    @Test
    fun cancelCreateBookmarkFolderTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            clickAddFolderButton()
            addNewFolderName(bookmarksFolderName)
            navigateUp()
            verifyKeyboardHidden()
        }
    }

    @Test
    fun editBookmarkViewTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
        }.openThreeDotMenu {
        }.clickEdit {
            verifyEditBookmarksView()
            verifyBookmarkNameEditBox()
            verifyBookmarkURLEditBox()
            verifyParentFolderSelector()
            navigateUp()
            verifyBookmarksMenuView()
        }
    }

    @Test
    fun copyBookmarkURLTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
        }.openThreeDotMenu {
        }.clickCopy {
            verifyCopySnackBarText()
        }
    }

    @Test
    fun openBookmarkInNewTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
        }.openThreeDotMenu {
        }.clickOpenInNewTab {
            verifyPageContent(defaultWebPage.content)
        }.openHomeScreen {
            verifyOpenTabsHeader()
        }
    }

    @Test
    fun openBookmarkInPrivateTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
        }.openThreeDotMenu {
        }.clickOpenInPrivateTab {
            verifyPageContent(defaultWebPage.content)
        }.openHomeScreen {
            verifyPrivateSessionHeader()
        }
    }

    @Test
    fun deleteBookmarkTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
        }.openThreeDotMenu {
        }.clickDelete {
            verifyDeleteSnackBarText()
        }
    }

    @Test
    fun multiSelectionToolbarItemsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
            longTapSelectItem(defaultWebPage.url)
        }

        multipleSelectionToolbar {
            verifyMultiSelectionCheckmark()
            verifyMultiSelectionCounter()
            verifyShareBookmarksButton()
            verifyCloseToolbarButton()
        }.closeToolbarReturnToBookmarks {
            verifyBookmarksMenuView()
        }
    }

    @Test
    fun openSelectionInNewTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openHomeScreen {
            closeTab()
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
            longTapSelectItem(defaultWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }

        multipleSelectionToolbar {
        }.clickOpenNewTab {
            verifyExistingTabList()
            verifyOpenTabsHeader()
        }
    }

    @Test
    fun openSelectionInPrivateTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
            longTapSelectItem(defaultWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }

        multipleSelectionToolbar {
        }.clickOpenPrivateTab {
            verifyExistingTabList()
            verifyPrivateSessionHeader()
        }
    }

    @Test
    fun deleteMultipleSelectionTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        browserScreen {
            createBookmark(firstWebPage.url)
            createBookmark(secondWebPage.url)
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
            longTapSelectItem(firstWebPage.url)
            longTapSelectItem(secondWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }

        multipleSelectionToolbar {
            clickMultiSelectionDelete()
        }

        bookmarksMenu {
            verifyEmptyBookmarksList()
        }
    }

    @Test
    fun shareButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openLibrary {
        }.openBookmarks {
            longTapSelectItem(defaultWebPage.url)
        }

        multipleSelectionToolbar {
            clickShareBookmarksButton()
            verifyShareOverlay()
            verifyShareTabFavicon()
            verifyShareTabTitle()
            verifyShareTabUrl()
        }
    }

    @Test
    fun multipleBookmarkDeletions() {
        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            createFolder("1")
            getInstrumentation().waitForIdleSync()
            createFolder("2")
            getInstrumentation().waitForIdleSync()
            createFolder("3")
            getInstrumentation().waitForIdleSync()
        }.openThreeDotMenu("1") {
        }.clickDelete {
        }.openThreeDotMenu("2") {
        }.clickDelete {
            verifyFolderTitle("3")
        }.goBack {
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyFolderTitle("3")
        }
    }
}
