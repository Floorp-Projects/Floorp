/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.fake

import mozilla.components.concept.storage.BookmarkInfo
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import java.util.UUID

class FakeBookmarksStorage() : BookmarksStorage {
    private val bookmarkMap: HashMap<String, BookmarkNode> = hashMapOf()

    override suspend fun warmUp() {
        throw NotImplementedError()
    }

    override suspend fun getTree(guid: String, recursive: Boolean): BookmarkNode? {
        throw NotImplementedError()
    }

    override suspend fun getBookmark(guid: String): BookmarkNode? {
        throw NotImplementedError()
    }

    override suspend fun getBookmarksWithUrl(url: String): List<BookmarkNode> {
        return bookmarkMap.values.filter { it.url == url }
    }

    override suspend fun getRecentBookmarks(
        limit: Int,
        maxAge: Long?,
        currentTime: Long,
    ): List<BookmarkNode> {
        throw NotImplementedError()
    }

    override suspend fun searchBookmarks(query: String, limit: Int): List<BookmarkNode> {
        throw NotImplementedError()
    }

    override suspend fun countBookmarksInTrees(guids: List<String>): UInt {
        throw NotImplementedError()
    }

    override suspend fun addItem(
        parentGuid: String,
        url: String,
        title: String,
        position: UInt?,
    ): String {
        val id = UUID.randomUUID().toString()
        bookmarkMap[id] =
            BookmarkNode(
                type = BookmarkNodeType.ITEM,
                guid = id,
                parentGuid = parentGuid,
                position = position,
                title = title,
                url = url,
                dateAdded = 0,
                children = null,
            )
        return id
    }

    override suspend fun addFolder(parentGuid: String, title: String, position: UInt?): String {
        throw NotImplementedError()
    }

    override suspend fun addSeparator(parentGuid: String, position: UInt?): String {
        throw NotImplementedError()
    }

    override suspend fun updateNode(guid: String, info: BookmarkInfo) {
        throw NotImplementedError()
    }

    override suspend fun deleteNode(guid: String): Boolean {
        throw NotImplementedError()
    }

    override suspend fun runMaintenance(dbSizeLimit: UInt) {
        throw NotImplementedError()
    }

    override fun cleanup() {
        throw NotImplementedError()
    }

    override fun cancelWrites() {
        throw NotImplementedError()
    }

    override fun cancelReads() {
        throw NotImplementedError()
    }

    override fun cancelReads(nextQuery: String) {
        throw NotImplementedError()
    }
}
