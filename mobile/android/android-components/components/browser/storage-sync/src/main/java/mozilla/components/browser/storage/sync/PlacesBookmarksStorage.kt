/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.content.Context
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkFolder
import mozilla.appservices.places.BookmarkItem
import mozilla.appservices.places.BookmarkSeparator
import mozilla.appservices.places.BookmarkTreeNode
import mozilla.appservices.places.BookmarkUpdateInfo
import mozilla.appservices.places.PlacesApi
import mozilla.components.concept.storage.BookmarkInfo
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.log.logger.Logger

/**
 * Implementation of the [BookmarksStorage] which is backed by a Rust Places lib via [PlacesApi].
 */
@Suppress("TooManyFunctions")
open class PlacesBookmarksStorage(context: Context) : PlacesStorage(context), BookmarksStorage, SyncableStore {

    override val logger = Logger("PlacesBookmarksStorage")

    /**
     * Produces a bookmarks tree for the given guid string.
     *
     * @param guid The bookmark guid to obtain.
     * @param recursive Whether to recurse and obtain all levels of children.
     * @return The populated root starting from the guid.
     */
    override suspend fun getTree(guid: String, recursive: Boolean): BookmarkNode? {
        return withContext(scope.coroutineContext) {
            reader.getBookmarksTree(guid, recursive)?.let {
                asBookmarkNode(it)
            }
        }
    }

    /**
     * Obtains the details of a bookmark without children, if one exists with that guid. Otherwise, null.
     *
     * @param guid The bookmark guid to obtain.
     * @return The bookmark node or null if it does not exist.
     */
    override suspend fun getBookmark(guid: String): BookmarkNode? {
        return withContext(scope.coroutineContext) {
            asBookmarkNode(reader.getBookmark(guid))
        }
    }

    /**
     * Produces a list of all bookmarks with the given URL.
     *
     * @param url The URL string.
     * @return The list of bookmarks that match the URL
     */
    override suspend fun getBookmarksWithUrl(url: String): List<BookmarkNode> {
        return withContext(scope.coroutineContext) {
            reader.getBookmarksWithURL(url).mapNotNull(::asBookmarkNode)
        }
    }

    /**
     * Searches bookmarks with a query string.
     *
     * @param query The query string to search.
     * @param limit The maximum number of entries to return.
     * @return The list of matching bookmark nodes up to the limit number of items.
     */
    override suspend fun searchBookmarks(query: String, limit: Int): List<BookmarkNode> {
        return withContext(scope.coroutineContext) {
            reader.searchBookmarks(query, limit).mapNotNull(::asBookmarkNode)
        }
    }

    /**
     * Adds a new bookmark item to a given node.
     *
     * Sync behavior: will add new bookmark item to remote devices.
     *
     * @param parentGuid The parent guid of the new node.
     * @param url The URL of the bookmark item to add.
     * @param title The title of the bookmark item to add.
     * @param position The optional position to add the new node or null to append.
     * @return The guid of the newly inserted bookmark item.
     */
    override suspend fun addItem(parentGuid: String, url: String, title: String, position: Int?): String {
        return withContext(scope.coroutineContext) {
            writer.createBookmarkItem(parentGuid, url, title, position)
        }
    }

    /**
     * Adds a new bookmark folder to a given node.
     *
     * Sync behavior: will add new separator to remote devices.
     *
     * @param parentGuid The parent guid of the new node.
     * @param title The title of the bookmark folder to add.
     * @param position The optional position to add the new node or null to append.
     * @return The guid of the newly inserted bookmark item.
     */
    override suspend fun addFolder(parentGuid: String, title: String, position: Int?): String {
        return withContext(scope.coroutineContext) {
            writer.createFolder(parentGuid, title, position)
        }
    }

    /**
     * Adds a new bookmark separator to a given node.
     *
     * Sync behavior: will add new separator to remote devices.
     *
     * @param parentGuid The parent guid of the new node.
     * @param position The optional position to add the new node or null to append.
     * @return The guid of the newly inserted bookmark item.
     */
    override suspend fun addSeparator(parentGuid: String, position: Int?): String {
        return withContext(scope.coroutineContext) {
            writer.createSeparator(parentGuid, position)
        }
    }

    /**
     * Edits the properties of an existing bookmark item and/or moves an existing one underneath a new parent guid.
     *
     * Sync behavior: will alter bookmark item on remote devices.
     *
     * @param guid The guid of the item to update.
     * @param info The info to change in the bookmark.
     */
    override suspend fun updateNode(guid: String, info: BookmarkInfo) {
        return withContext(scope.coroutineContext) {
            writer.updateBookmark(guid, BookmarkUpdateInfo(info.parentGuid, info.position, info.title, info.url))
        }
    }

    /**
     * Deletes a bookmark node and all of its children, if any.
     *
     * Sync behavior: will remove bookmark from remote devices.
     *
     * @return Whether the bookmark existed or not.
     */
    override suspend fun deleteNode(guid: String): Boolean = withContext(scope.coroutineContext) {
        writer.deleteBookmarkNode(guid)
    }

    private fun asBookmarkNode(node: BookmarkTreeNode?): BookmarkNode? {
        return node?.let {
            when (it) {
                is BookmarkItem -> {
                    BookmarkNode(BookmarkNodeType.ITEM, it.guid, it.parentGUID, it.position, it.title, it.url, null)
                }
                is BookmarkFolder -> {
                    BookmarkNode(BookmarkNodeType.FOLDER, it.guid, it.parentGUID, it.position, it.title, null,
                            it.children?.mapNotNull(::asBookmarkNode))
                }
                is BookmarkSeparator -> {
                    BookmarkNode(BookmarkNodeType.SEPARATOR, it.guid, it.parentGUID, it.position, null, null, null)
                }
            }
        }
    }

    /**
     * Runs syncBookmarks() method on the places Connection
     *
     * @param authInfo The authentication information to sync with.
     * @return Sync status of OK or Error
     */
    override suspend fun sync(authInfo: SyncAuthInfo): SyncStatus {
        return withContext(scope.coroutineContext) {
            syncAndHandleExceptions {
                places.syncBookmarks(authInfo)
            }
        }
    }

    /**
     * This should be removed. See: https://github.com/mozilla/application-services/issues/1877
     *
     * @return raw internal handle that could be used for referencing underlying [PlacesApi]. Use it with SyncManager.
     */
    override fun getHandle(): Long {
        return places.getHandle()
    }
}
