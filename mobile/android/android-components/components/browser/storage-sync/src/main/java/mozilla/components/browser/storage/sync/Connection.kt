/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import androidx.annotation.GuardedBy
import mozilla.appservices.places.PlacesApi
import mozilla.appservices.places.PlacesReaderConnection
import mozilla.appservices.places.PlacesWriterConnection
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.support.sync.telemetry.SyncTelemetry
import org.json.JSONObject
import java.io.Closeable
import java.io.File

const val DB_NAME = "places.sqlite"

/**
 * A slight abstraction over [PlacesApi].
 *
 * A single reader is assumed here, which isn't a limitation placed on use by [PlacesApi].
 * We can switch to pooling multiple readers as the need arises. Underneath, these are connections
 * to a SQLite database, and so opening and maintaining them comes with a memory/IO burden.
 *
 * Writer is always the same, as guaranteed by [PlacesApi].
 */
internal interface Connection : Closeable {
    fun registerWithSyncManager()

    /**
     * Allows to read history, bookmarks, and other data from persistent storage.
     * All calls on the same reader are queued. Multiple readers are allowed.
     */
    fun reader(): PlacesReaderConnection

    /**
     * Create a new reader for history, bookmarks and other data from persistent storage.
     * Allows for disbanded calls from the default reader from [reader] and so being able to
     * easily start and cancel any data requests without impacting others using another reader.
     *
     * All [PlacesApi] requests are synchronized at lower levels so even with using multiple readers
     * all requests are ordered with concurrent reads not possible.
     */
    fun newReader(): PlacesReaderConnection

    /**
     * Allowed to add history, bookmarks and other data to persistent storage.
     * All calls are queued and synchronized at lower levels. Only one writer is recommended.
     */
    fun writer(): PlacesWriterConnection

    // Until we get a real SyncManager in application-services libraries, we'll have to live with this
    // strange split that doesn't quite map all that well to our internal storage model.
    fun syncHistory(syncInfo: SyncAuthInfo)
    fun syncBookmarks(syncInfo: SyncAuthInfo)

    /**
     * @return Migration metrics wrapped in a JSON object. See libplaces for schema details.
     */
    fun importVisitsFromFennec(dbPath: String): JSONObject

    /**
     * @return Migration metrics wrapped in a JSON object. See libplaces for schema details.
     */
    fun importBookmarksFromFennec(dbPath: String): JSONObject

    /**
     * @return A list of [BookmarkNode] which represent pinned sites.
     */
    fun readPinnedSitesFromFennec(dbPath: String): List<BookmarkNode>
}

/**
 * A singleton implementation of the [Connection] interface backed by the Rust Places library.
 */
internal object RustPlacesConnection : Connection {
    @GuardedBy("this")
    private var api: PlacesApi? = null

    @GuardedBy("this")
    private var cachedReader: PlacesReaderConnection? = null

    /**
     * Creates a long-lived [PlacesApi] instance, and caches a reader connection.
     * Writer connection is maintained by [PlacesApi] itself, and is created upon its initialization.
     *
     * @param parentDir Location of the parent directory in which database is/will be stored.
     */
    fun init(parentDir: File) = synchronized(this) {
        if (api == null) {
            api = PlacesApi(File(parentDir, DB_NAME).canonicalPath)
        }
        cachedReader = api!!.openReader()
    }

    override fun registerWithSyncManager() {
        val api = safeGetApi()
        check(api != null) { "must call init first" }
        api.registerWithSyncManager()
    }

    override fun reader(): PlacesReaderConnection = synchronized(this) {
        check(cachedReader != null) { "must call init first" }
        return cachedReader!!
    }

    override fun newReader(): PlacesReaderConnection = synchronized(this) {
        val api = safeGetApi()
        check(api != null) { "must call init first" }
        return api.openReader()
    }

    override fun writer(): PlacesWriterConnection {
        val api = safeGetApi()
        check(api != null) { "must call init first" }
        return api.getWriter()
    }

    override fun syncHistory(syncInfo: SyncAuthInfo) {
        val api = safeGetApi()
        check(api != null) { "must call init first" }
        val ping = api.syncHistory(syncInfo.into())
        SyncTelemetry.processHistoryPing(ping)
    }

    override fun syncBookmarks(syncInfo: SyncAuthInfo) {
        val api = safeGetApi()
        check(api != null) { "must call init first" }
        val ping = api.syncBookmarks(syncInfo.into())
        SyncTelemetry.processBookmarksPing(ping)
    }

    override fun importVisitsFromFennec(dbPath: String): JSONObject {
        val api = safeGetApi()
        check(api != null) { "must call init first" }
        return api.importVisitsFromFennec(dbPath)
    }

    override fun importBookmarksFromFennec(dbPath: String): JSONObject {
        val api = safeGetApi()
        check(api != null) { "must call init first" }
        return api.importBookmarksFromFennec(dbPath)
    }

    override fun readPinnedSitesFromFennec(dbPath: String): List<BookmarkNode> {
        val api = safeGetApi()
        check(api != null) { "must call init first" }
        return api.importPinnedSitesFromFennec(dbPath).map { it.asBookmarkNode() }
    }

    override fun close() = synchronized(this) {
        check(api != null) { "must call init first" }
        api!!.close()
        api = null
    }

    private fun safeGetApi(): PlacesApi? = synchronized(this) {
        return this.api
    }
}
