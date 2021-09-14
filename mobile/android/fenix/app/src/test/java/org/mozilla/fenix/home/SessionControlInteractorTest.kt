/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.historymetadata.controller.HistoryMetadataController
import org.mozilla.fenix.home.recentbookmarks.controller.RecentBookmarksController
import org.mozilla.fenix.home.recenttabs.controller.RecentTabController
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

class SessionControlInteractorTest {

    private val controller: DefaultSessionControlController = mockk(relaxed = true)
    private val recentTabController: RecentTabController = mockk(relaxed = true)
    private val recentBookmarksController: RecentBookmarksController = mockk(relaxed = true)
    private val historyMetadataController: HistoryMetadataController = mockk(relaxed = true)

    private lateinit var interactor: SessionControlInteractor

    @Before
    fun setup() {
        interactor = SessionControlInteractor(
            controller,
            recentTabController,
            recentBookmarksController,
            historyMetadataController
        )
    }

    @Test
    fun onCollectionAddTabTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onCollectionAddTabTapped(collection)
        verify { controller.handleCollectionAddTabTapped(collection) }
    }

    @Test
    fun onCollectionOpenTabClicked() {
        val tab: Tab = mockk(relaxed = true)
        interactor.onCollectionOpenTabClicked(tab)
        verify { controller.handleCollectionOpenTabClicked(tab) }
    }

    @Test
    fun onCollectionOpenTabsTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onCollectionOpenTabsTapped(collection)
        verify { controller.handleCollectionOpenTabsTapped(collection) }
    }

    @Test
    fun onCollectionRemoveTab() {
        val collection: TabCollection = mockk(relaxed = true)
        val tab: Tab = mockk(relaxed = true)
        interactor.onCollectionRemoveTab(collection, tab, false)
        verify { controller.handleCollectionRemoveTab(collection, tab, false) }
    }

    @Test
    fun onCollectionShareTabsClicked() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onCollectionShareTabsClicked(collection)
        verify { controller.handleCollectionShareTabsClicked(collection) }
    }

    @Test
    fun onDeleteCollectionTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onDeleteCollectionTapped(collection)
        verify { controller.handleDeleteCollectionTapped(collection) }
    }

    @Test
    fun onPrivateBrowsingLearnMoreClicked() {
        interactor.onPrivateBrowsingLearnMoreClicked()
        verify { controller.handlePrivateBrowsingLearnMoreClicked() }
    }

    @Test
    fun onRenameCollectionTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onRenameCollectionTapped(collection)
        verify { controller.handleRenameCollectionTapped(collection) }
    }

    @Test
    fun onStartBrowsingClicked() {
        interactor.onStartBrowsingClicked()
        verify { controller.handleStartBrowsingClicked() }
    }

    @Test
    fun onToggleCollectionExpanded() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onToggleCollectionExpanded(collection, true)
        verify { controller.handleToggleCollectionExpanded(collection, true) }
    }

    @Test
    fun onAddTabsToCollection() {
        interactor.onAddTabsToCollectionTapped()
        verify { controller.handleCreateCollection() }
    }

    @Test
    fun onPaste() {
        interactor.onPaste("text")
        verify { controller.handlePaste("text") }
    }

    @Test
    fun onPasteAndGo() {
        interactor.onPasteAndGo("text")
        verify { controller.handlePasteAndGo("text") }
    }

    @Test
    fun onRemoveCollectionsPlaceholder() {
        interactor.onRemoveCollectionsPlaceholder()
        verify { controller.handleRemoveCollectionsPlaceholder() }
    }

    @Test
    fun onCollectionMenuOpened() {
        interactor.onCollectionMenuOpened()
        verify { controller.handleMenuOpened() }
    }

    @Test
    fun onTopSiteMenuOpened() {
        interactor.onTopSiteMenuOpened()
        verify { controller.handleMenuOpened() }
    }

    @Test
    fun onRecentTabClicked() {
        val tabId = "tabId"
        interactor.onRecentTabClicked(tabId)
        verify { recentTabController.handleRecentTabClicked(tabId) }
    }

    @Test
    fun onRecentTabShowAllClicked() {
        interactor.onRecentTabShowAllClicked()
        verify { recentTabController.handleRecentTabShowAllClicked() }
    }

    @Test
    fun onHistoryMetadataItemClicked() {
        val historyEntry = HistoryMetadata(
            key = HistoryMetadataKey("http://www.mozilla.com", null, null),
            title = "mozilla",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )

        interactor.onHistoryMetadataItemClicked(historyEntry.key.url, historyEntry.key)
        verify {
            historyMetadataController.handleHistoryMetadataItemClicked(
                historyEntry.key.url,
                historyEntry.key
            )
        }
    }

    @Test
    fun onHistoryMetadataShowAllClicked() {
        interactor.onHistoryMetadataShowAllClicked()
        verify { historyMetadataController.handleHistoryShowAllClicked() }
    }

    @Test
    fun onToggleHistoryMetadataGroupExpanded() {
        val historyEntry = HistoryMetadata(
            key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
            title = "mozilla",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null
        )
        val historyGroup = HistoryMetadataGroup(
            title = "mozilla",
            historyMetadata = listOf(historyEntry)
        )
        interactor.onToggleHistoryMetadataGroupExpanded(historyGroup)
        verify { historyMetadataController.handleToggleHistoryMetadataGroupExpanded(historyGroup) }
    }

    @Test
    fun `WHEN a recently saved bookmark is clicked THEN the selected bookmark is handled`() {
        val bookmark = BookmarkNode(
            type = BookmarkNodeType.ITEM,
            guid = "guid#${Math.random() * 1000}",
            parentGuid = null,
            position = null,
            title = null,
            url = null,
            dateAdded = 0,
            children = null
        )

        interactor.onRecentBookmarkClicked(bookmark)
        verify { recentBookmarksController.handleBookmarkClicked(bookmark) }
    }

    @Test
    fun `WHEN Show All recently saved bookmarks button is clicked THEN the click is handled`() {
        interactor.onShowAllBookmarksClicked()
        verify { recentBookmarksController.handleShowAllBookmarksClicked() }
    }
}
