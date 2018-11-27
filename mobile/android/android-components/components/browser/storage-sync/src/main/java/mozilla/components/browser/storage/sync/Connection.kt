/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.support.annotation.GuardedBy
import org.mozilla.places.PlacesAPI
import org.mozilla.places.PlacesConnection
import java.io.Closeable
import java.io.File

const val DB_NAME = "places.sqlite"

/**
 * A singleton implementation of the [Connection] interface backed by the Rust Places library.
 */
internal object RustPlacesConnection : Connection {
    @GuardedBy("this")
    private var cachedConnection: PlacesConnection? = null

    /**
     * Creates a long-lived [PlacesConnection].
     * @param parentDir Location of the parent directory in which database is/will be stored.
     * @param encryptionString Optional encryption key; if used, database will be encrypted at rest.
     */
    fun createLongLivedConnection(parentDir: File, encryptionString: String? = null) = synchronized(this) {
        if (cachedConnection == null) {
            cachedConnection = newConnection(parentDir, encryptionString)
        }
    }

    /**
     * Creates a new [PlacesConnection] instance. Caller is responsible for closing it.
     * @param parentDir Location of the parent directory in which database is/will be stored.
     * @param encryptionString Optional encryption key; if used, database will be encrypted at rest.
     */
    fun newConnection(parentDir: File, encryptionString: String? = null): PlacesConnection = synchronized(this) {
        return PlacesConnection(File(parentDir, DB_NAME).canonicalPath, encryptionString)
    }

    override fun api(): PlacesAPI = synchronized(this) {
        requireNotNull(cachedConnection) { "createLongLivedConnection must have been called before use" }
        return cachedConnection!!
    }

    override fun close() = synchronized(this) {
        requireNotNull(cachedConnection) { "createLongLivedConnection must have been called before use" }
        cachedConnection!!.close()
    }
}

/**
 * An interface which describes a [Closeable] connection that provides access to a [PlacesAPI].
 */
interface Connection : Closeable {
    fun api(): PlacesAPI
}
