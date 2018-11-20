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
    private var placesConnection: PlacesConnection? = null

    fun init(parentDir: File, encryptionString: String?) = synchronized(this) {
        if (placesConnection == null) {
            placesConnection = PlacesConnection(File(parentDir, DB_NAME).canonicalPath, encryptionString)
        }
    }

    override fun api(): PlacesAPI = synchronized(this) {
        requireNotNull(placesConnection) { "RustPlacesConnection must be initialized before use" }
        return placesConnection!!
    }

    override fun close() = synchronized(this) {
        requireNotNull(placesConnection) { "RustPlacesConnection must be initialized before use" }
        placesConnection!!.close()
    }
}

/**
 * An interface which describes a [Closeable] connection that provides access to a [PlacesAPI].
 */
interface Connection : Closeable {
    fun api(): PlacesAPI
}
