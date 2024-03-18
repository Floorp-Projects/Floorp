/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import android.content.Context
import android.util.AtomicFile
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.EngineSessionStateStorage
import mozilla.components.support.ktx.java.io.truncateDirectory
import mozilla.components.support.ktx.util.readAndDeserialize
import mozilla.components.support.ktx.util.streamJSON
import org.json.JSONObject
import java.io.File

/**
 * Implementation of [EngineSessionStateStorage] that reads/writes [EngineSessionState] as JSON
 * files onto disk.
 *
 * This is used by components that need to persist [EngineSessionState] instances separately from
 * [RecoverableBrowserState].
 *
 * @param context A [Context] used for accessing file system.
 * @param engine An [Engine] instance used for rehydrating persisted [EngineSessionState].
 */
class FileEngineSessionStateStorage(
    private val context: Context,
    private val engine: Engine,
) : EngineSessionStateStorage {
    private val filesDir by lazy { context.filesDir }

    override suspend fun write(uuid: String, state: EngineSessionState): Boolean {
        return getStateFile(uuid).streamJSON {
            state.writeTo(this)
        }
    }

    override suspend fun read(uuid: String): EngineSessionState? {
        val jsonObject = getStateFile(uuid).readAndDeserialize { json ->
            JSONObject(json)
        }
        return jsonObject?.let { engine.createSessionState(it) }
    }

    override suspend fun delete(uuid: String) {
        getStateFile(uuid).delete()
    }

    override suspend fun deleteAll() {
        getStateDirectory(filesDir).truncateDirectory()
    }

    private fun getStateFile(uuid: String): AtomicFile {
        return AtomicFile(File(getStateDirectory(filesDir), uuid))
    }

    private fun getStateDirectory(filesDir: File): File {
        // NB: This code used to live in feature-recentlyclosedtabs, hence the hardcoded name below.
        // It wasn't renamed during a refactor to avoid having to force consumers to run data migrations.
        return File(filesDir, "mozac.feature.recentlyclosed").apply {
            mkdirs()
        }
    }
}
