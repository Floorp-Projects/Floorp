/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import android.util.AtomicFile
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SnapshotSerializer
import mozilla.components.concept.engine.Engine
import org.json.JSONException
import java.io.FileOutputStream
import java.io.IOException

/**
 * Read a [SessionManager.Snapshot] from this [AtomicFile]. Returns `null` if no snapshot could be read.
 */
fun AtomicFile.readSnapshot(
    engine: Engine,
    serializer: SnapshotSerializer = SnapshotSerializer()
): SessionManager.Snapshot? {
    return try {
        openRead().use {
            val json = it.bufferedReader().use { reader -> reader.readText() }

            val snapshot = serializer.fromJSON(engine, json)

            if (snapshot.isEmpty()) {
                null
            } else {
                snapshot
            }
        }
    } catch (_: IOException) {
        null
    } catch (_: JSONException) {
        null
    }
}

/**
 * Saves the given [SessionManager.Snapshot] to this [AtomicFile].
 */
fun AtomicFile.writeSnapshot(
    snapshot: SessionManager.Snapshot,
    serializer: SnapshotSerializer = SnapshotSerializer()
): Boolean {
    var outputStream: FileOutputStream? = null

    return try {
        val json = serializer.toJSON(snapshot)

        outputStream = startWrite()
        outputStream.write(json.toByteArray())
        finishWrite(outputStream)
        true
    } catch (_: IOException) {
        failWrite(outputStream)
        false
    } catch (_: JSONException) {
        failWrite(outputStream)
        false
    }
}
