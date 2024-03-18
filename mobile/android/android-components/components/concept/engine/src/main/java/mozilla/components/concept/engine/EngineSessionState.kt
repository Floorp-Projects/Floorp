/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.util.JsonWriter

/**
 * The state of an [EngineSession]. An instance can be obtained from [EngineSession.saveState]. Creating a new
 * [EngineSession] and calling [EngineSession.restoreState] with the same state instance should restore the previous
 * session.
 */
interface EngineSessionState {
    /**
     * Writes this state as JSON to the given [JsonWriter].
     *
     * When reading JSON from disk [Engine.createSessionState] can be used to turn it back into an [EngineSessionState]
     * instance.
     */
    fun writeTo(writer: JsonWriter)
}

/**
 * An interface describing a storage layer for an [EngineSessionState].
 */
interface EngineSessionStateStorage {
    /**
     * Writes a [state] with a provided [uuid] as its identifier.
     *
     * @return A boolean flag indicating if the write was a success.
     */
    suspend fun write(uuid: String, state: EngineSessionState): Boolean

    /**
     * Reads an [EngineSessionState] given a provided [uuid] as its identifier.
     *
     * @return A [EngineSessionState] if one is present for the given [uuid], `null` otherwise.
     */
    suspend fun read(uuid: String): EngineSessionState?

    /**
     * Deletes persisted [EngineSessionState] for a given [uuid].
     */
    suspend fun delete(uuid: String)

    /**
     * Deletes all persisted [EngineSessionState] instances.
     */
    suspend fun deleteAll()
}
