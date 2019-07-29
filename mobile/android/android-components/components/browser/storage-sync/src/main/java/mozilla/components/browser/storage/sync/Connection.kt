/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import androidx.annotation.GuardedBy
import mozilla.appservices.places.PlacesApi
import mozilla.appservices.places.PlacesReaderConnection
import mozilla.appservices.places.PlacesWriterConnection
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.support.sync.telemetry.SyncTelemetry
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
    /**
     * This should be removed. See: https://github.com/mozilla/application-services/issues/1877
     *
     * @return raw internal handle that could be used for referencing underlying [PlacesApi]. Use it with SyncManager.
     */
    fun getHandle(): Long

    fun reader(): PlacesReaderConnection
    fun writer(): PlacesWriterConnection

    // Until we get a real SyncManager in application-services libraries, we'll have to live with this
    // strange split that doesn't quite map all that well to our internal storage model.
    fun syncHistory(syncInfo: SyncAuthInfo)
    fun syncBookmarks(syncInfo: SyncAuthInfo)
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

    override fun getHandle(): Long {
        check(api != null) { "must call init first" }
        return api!!.getHandle()
    }

    override fun reader(): PlacesReaderConnection = synchronized(this) {
        check(cachedReader != null) { "must call init first" }
        return cachedReader!!
    }

    override fun writer(): PlacesWriterConnection {
        check(api != null) { "must call init first" }
        return api!!.getWriter()
    }

    override fun syncHistory(syncInfo: SyncAuthInfo) {
        check(api != null) { "must call init first" }
        val ping = api!!.syncHistory(syncInfo.into())
        SyncTelemetry.processHistoryPing(ping)
    }

    override fun syncBookmarks(syncInfo: SyncAuthInfo) {
        check(api != null) { "must call init first" }
        val ping = api!!.syncBookmarks(syncInfo.into())
        SyncTelemetry.processBookmarksPing(ping)
    }

    override fun close() = synchronized(this) {
        check(api != null) { "must call init first" }
        api!!.close()
        api = null
    }
}
