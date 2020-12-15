/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import android.util.AtomicFile
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.BrowserStateSerializer
import mozilla.components.browser.session.storage.SnapshotSerializer
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
import mozilla.components.support.ktx.util.readAndDeserialize
import mozilla.components.support.ktx.util.writeString
import org.json.JSONObject

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
 * Saves the given [BrowserState] to this [AtomicFile].
 */
fun AtomicFile.writeState(
    state: BrowserState,
    serializer: BrowserStateSerializer = BrowserStateSerializer()
): Boolean {
    return serializer.write(state, file = this)
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
 * Reads a single [RecoverableTab] from this [AtomicFile]. Returns `null` if no tab could be read.
 */
fun AtomicFile.readTab(
    engine: Engine,
    restoreSessionId: Boolean = true,
    restoreParentId: Boolean = true,
    serializer: BrowserStateSerializer = BrowserStateSerializer()
): RecoverableTab? {
    return serializer.readTab(engine, this, restoreSessionId, restoreParentId)
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
