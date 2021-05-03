/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu
import androidx.test.espresso.IdlingRegistry
import androidx.test.platform.app.InstrumentationRegistry.getInstrumentation
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.BookmarkRoot
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.longTapSelectItem
import org.mozilla.fenix.ui.robots.bookmarksMenu
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.mDevice
import org.mozilla.fenix.ui.robots.multipleSelectionToolbar
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic functionality of bookmarks
 */
class BookmarksTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

    private lateinit var mockWebServer: MockWebServer
    private val bookmarksFolderName = "New Folder"
    private val testBookmark = object {
        var title: String = "Bookmark title"
        var url: String = "https://www.test.com"
    }
    private var bookmarksListIdlingResource: RecyclerViewIdlingResource? = null

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
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

        if (bookmarksListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(bookmarksListIdlingResource!!)
        }
    }

    @Test
    fun defaultDesktopBookmarksFoldersTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 1)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

            selectFolder("Desktop Bookmarks")
            verifyFolderTitle("Bookmarks Menu")
            verifyFolderTitle("Bookmarks Toolbar")
            verifyFolderTitle("Other Bookmarks")
            verifySignInToSyncButton()
        }.clickSingInToSyncButton {
            verifyTurnOnSyncToolbarTitle()
        }
    }

    @Test
    fun verifyBookmarkButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.bookmarkPage {
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
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

            verifyBookmarkedURL(defaultWebPage.url.toString())
            verifyBookmarkFavicon(defaultWebPage.url)
        }
    }

    @Test
    fun createBookmarkFolderTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 1)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

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
    fun editBookmarkTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)
        }.openThreeDotMenu(defaultWebPage.url) {
        }.clickEdit {
            verifyEditBookmarksView()
            verifyBookmarkNameEditBox()
            verifyBookmarkURLEditBox()
            verifyParentFolderSelector()
            changeBookmarkTitle(testBookmark.title)
            changeBookmarkUrl(testBookmark.url)
            saveEditBookmark()
            verifyBookmarkTitle(testBookmark.title)
            verifyBookmarkedURL(testBookmark.url)
            verifyKeyboardHidden()
        }
    }

    @Test
    fun copyBookmarkURLTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)
        }.openThreeDotMenu(defaultWebPage.url) {
        }.clickCopy {
            verifyCopySnackBarText()
        }
    }

    @Test
    fun threeDotMenuShareBookmarkTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)
        }.openThreeDotMenu(defaultWebPage.url) {
        }.clickShare {
            verifyShareOverlay()
            verifyShareBookmarkFavicon()
            verifyShareBookmarkTitle()
            verifyShareBookmarkUrl()
        }
    }

    @Test
    fun openBookmarkInNewTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)
        }.openThreeDotMenu(defaultWebPage.url) {
        }.clickOpenInNewTab {
            verifyTabTrayIsOpened()
            verifyNormalModeSelected()
        }
    }

    @Test
    fun openBookmarkInPrivateTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)
        }.openThreeDotMenu(defaultWebPage.url) {
        }.clickOpenInPrivateTab {
            verifyTabTrayIsOpened()
            verifyPrivateModeSelected()
        }
    }

    @Test
    fun deleteBookmarkTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)
        }.openThreeDotMenu(defaultWebPage.url) {
            IdlingRegistry.getInstance().unregister(bookmarksListIdlingResource!!)
        }.clickDelete {
            verifyDeleteSnackBarText()
            verifyUndoDeleteSnackBarButton()
        }
    }

    @Test
    fun undoDeleteBookmarkTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)
        }.openThreeDotMenu(defaultWebPage.url) {
            IdlingRegistry.getInstance().unregister(bookmarksListIdlingResource!!)
        }.clickDelete {
            verifyUndoDeleteSnackBarButton()
            clickUndoDeleteButton()
            verifySnackBarHidden()
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)
            verifyBookmarkedURL(defaultWebPage.url.toString())
        }
    }

    @Test
    fun multiSelectionToolbarItemsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

            longTapSelectItem(defaultWebPage.url)
        }

        multipleSelectionToolbar {
            verifyMultiSelectionCheckmark(defaultWebPage.url)
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
        }.openTabDrawer {
            closeTab()
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

            longTapSelectItem(defaultWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }

        multipleSelectionToolbar {
        }.clickOpenNewTab {
            verifyNormalModeSelected()
            verifyExistingTabList()
        }
    }

    @Test
    fun openSelectionInPrivateTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

            longTapSelectItem(defaultWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }

        multipleSelectionToolbar {
        }.clickOpenPrivateTab {
            verifyPrivateModeSelected()
            verifyExistingTabList()
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
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 3)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

            longTapSelectItem(firstWebPage.url)
            longTapSelectItem(secondWebPage.url)
            IdlingRegistry.getInstance().unregister(bookmarksListIdlingResource!!)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }

        multipleSelectionToolbar {
            clickMultiSelectionDelete()
        }

        bookmarksMenu {
            verifyDeleteMultipleBookmarksSnackBar()
        }
    }

    @Test
    fun multipleSelectionShareButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

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
    fun multipleBookmarkDeletionsTest() {
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
            verifyDeleteFolderConfirmationMessage()
            confirmFolderDeletion()
            verifyDeleteSnackBarText()
        }.openThreeDotMenu("2") {
        }.clickDelete {
            verifyDeleteFolderConfirmationMessage()
            confirmFolderDeletion()
            verifyDeleteSnackBarText()
            verifyFolderTitle("3")
        }.closeMenu {
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyFolderTitle("3")
        }
    }

    @Test
    fun changeBookmarkParentFolderTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 2)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

            createFolder(bookmarksFolderName)

            IdlingRegistry.getInstance().unregister(bookmarksListIdlingResource!!)

        }.openThreeDotMenu(defaultWebPage.title) {
        }.clickEdit {
            clickParentFolderSelector()
            selectFolder(bookmarksFolderName)
            navigateUp()
            saveEditBookmark()

            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

            selectFolder(bookmarksFolderName)
            verifyBookmarkedURL(defaultWebPage.url.toString())

            IdlingRegistry.getInstance().unregister(bookmarksListIdlingResource!!)
        }
    }

    @Test
    fun navigateBookmarksFoldersTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            createFolder("1")
            getInstrumentation().waitForIdleSync()
            waitForBookmarksFolderContentToExist("Bookmarks", "1")
            selectFolder("1")
            verifyCurrentFolderTitle("1")
            createFolder("2")
            getInstrumentation().waitForIdleSync()
            waitForBookmarksFolderContentToExist("1", "2")
            selectFolder("2")
            verifyCurrentFolderTitle("2")
            navigateUp()
            waitForBookmarksFolderContentToExist("1", "2")
            verifyCurrentFolderTitle("1")
            mDevice.pressBack()
            verifyBookmarksMenuView()
        }
    }

    @Test
    fun cantSelectDesktopFoldersTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list))
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)

            longTapDesktopFolder("Desktop Bookmarks")
            verifySelectDefaultFolderSnackBarText()
        }
    }

    @Test
    fun verifyCloseMenuTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
        }.closeMenu {
            verifyHomeScreen()
        }
    }
}
