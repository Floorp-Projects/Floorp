/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.support.annotation.GuardedBy
import mozilla.appservices.places.PlacesApi
import mozilla.appservices.places.PlacesReaderConnection
import mozilla.appservices.places.PlacesWriterConnection
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
interface Connection : Closeable {
    fun reader(): PlacesReaderConnection
    fun writer(): PlacesWriterConnection
    fun sync(syncInfo: SyncAuthInfo)
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
     * @param encryptionString Optional encryption key; if used, database will be encrypted at rest.
     */
    fun init(parentDir: File, encryptionString: String? = null) = synchronized(this) {
        if (api == null) {
            api = PlacesApi(File(parentDir, DB_NAME).canonicalPath, encryptionString)
        }
        cachedReader = api!!.openReader()
    }

    override fun reader(): PlacesReaderConnection = synchronized(this) {
        check(cachedReader != null) { "must call init first" }
        return cachedReader!!
    }

    override fun writer(): PlacesWriterConnection {
        check(api != null) { "must call init first" }
        return api!!.getWriter()
    }

    override fun sync(syncInfo: SyncAuthInfo) {
        check(api != null) { "must call init first" }
        api!!.sync(syncInfo)
    }

    override fun close() = synchronized(this) {
        check(api != null) { "must call init first" }
        api!!.close()
    }
}
