/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import android.util.AtomicFile
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SnapshotSerializer
import mozilla.components.concept.engine.Engine
import org.json.JSONException
import org.json.JSONObject
import java.io.FileOutputStream
import java.io.IOException

/**
 * Read a [SessionManager.Snapshot] from this [AtomicFile]. Returns `null` if no snapshot could be read.
 */
fun AtomicFile.readSnapshot(
    engine: Engine,
    serializer: SnapshotSerializer = SnapshotSerializer()
): SessionManager.Snapshot? {
    return readAndDeserialize { json ->
        val snapshot = serializer.fromJSON(engine, json)

        if (snapshot.isEmpty()) {
            null
        } else {
            snapshot
        }
    }
}

/**
 * Saves the given [SessionManager.Snapshot] to this [AtomicFile].
 */
fun AtomicFile.writeSnapshot(
    snapshot: SessionManager.Snapshot,
    serializer: SnapshotSerializer = SnapshotSerializer()
): Boolean {
    return writeString { serializer.toJSON(snapshot) }
}

/**
 * Reads a single [SessionManager.Snapshot.Item] from this [AtomicFile]. Returns `null` if no snapshot item could be
 * read.
 */
fun AtomicFile.readSnapshotItem(
    engine: Engine,
    restoreSessionId: Boolean = true,
    restoreParentId: Boolean = true,
    serializer: SnapshotSerializer = SnapshotSerializer(restoreSessionId, restoreParentId)
): SessionManager.Snapshot.Item? {
    return readAndDeserialize { json ->
        serializer.itemFromJSON(engine, JSONObject(json))
    }
}

/**
 * Saves a single [SessionManager.Snapshot.Item] to this [AtomicFile].
 */
fun AtomicFile.writeSnapshotItem(
    item: SessionManager.Snapshot.Item,
    serializer: SnapshotSerializer = SnapshotSerializer()
): Boolean {
    return writeString {
        serializer.itemToJSON(item).toString()
    }
}

private fun AtomicFile.writeString(block: () -> String): Boolean {
    var outputStream: FileOutputStream? = null

    return try {
        outputStream = startWrite()
        outputStream.write(block().toByteArray())
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

private fun <T> AtomicFile.readAndDeserialize(block: (String) -> T): T? {
    return try {
        openRead().use {
            val json = it.bufferedReader().use { reader -> reader.readText() }
            block(json)
        }
    } catch (_: IOException) {
        null
    } catch (_: JSONException) {
        null
    }
}
