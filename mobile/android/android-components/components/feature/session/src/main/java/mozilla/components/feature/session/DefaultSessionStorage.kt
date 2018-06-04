/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Context
import android.util.AtomicFile
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.session.storage.SessionStorage
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

const val SELECTED_SESSION_KEY = "selectedSession"
const val SESSION_KEY = "session"
const val ENGINE_SESSION_KEY = "engineSession"
const val VERSION_KEY = "version"
const val VERSION = 1
const val FILE_NAME = "mozilla_components_session_storage.json"

/**
 * Default implementation of [SessionStorage] which persists browser and engine
 * session states to a JSON file.
 *
 * The JSON format used for persisting:
 * {
 *     "version": [version],
 *     "selectedSession": "[session-uuid]",
 *     "[session-uuid]": {
 *         "session": {}
 *         "engineSession": {}
 *     },
 *     "[session-uuid]": {
 *         "session": {}
 *         "engineSession": {}
 *     }
 * }
 */
class DefaultSessionStorage(private val context: Context) : SessionStorage {

    @Synchronized
    override fun restore(engine: Engine): Pair<Map<Session, EngineSession>, String> {
        return try {
            val sessions = mutableMapOf<Session, EngineSession>()

            getFile().openRead().use {
                val json = it.bufferedReader().use {
                    it.readText()
                }

                val jsonRoot = JSONObject(json)
                val selectedSessionId = jsonRoot.getString(SELECTED_SESSION_KEY)
                jsonRoot.remove(SELECTED_SESSION_KEY)
                // We don't need to consume the version for now
                jsonRoot.remove(VERSION_KEY)
                jsonRoot.keys().forEach {
                    val jsonSession = jsonRoot.getJSONObject(it)
                    val session = deserializeSession(it, jsonSession.getJSONObject(SESSION_KEY))
                    val engineSession = deserializeEngineSession(engine, jsonSession.getJSONObject(ENGINE_SESSION_KEY))
                    sessions[session] = engineSession
                }
                Pair(sessions, selectedSessionId)
            }
        } catch (_: IOException) {
            Pair(emptyMap(), "")
        } catch (_: JSONException) {
            Pair(emptyMap(), "")
        }
    }

    @Synchronized
    override fun persist(sessions: Map<Session, EngineSession>, selectedSession: String): Boolean {
        var file: AtomicFile? = null
        var outputStream: FileOutputStream? = null

        return try {
            val json = JSONObject()
            json.put(VERSION_KEY, VERSION)
            json.put(SELECTED_SESSION_KEY, selectedSession)

            sessions.forEach({ (session, engineSession) ->
                val sessionJson = JSONObject()
                sessionJson.put(SESSION_KEY, serializeSession(session))
                sessionJson.put(ENGINE_SESSION_KEY, serializeEngineSession(engineSession))
                json.put(session.id, sessionJson)
            })

            file = getFile()
            outputStream = file.startWrite()
            outputStream.write(json.toString().toByteArray())
            file.finishWrite(outputStream)
            true
        } catch (_: IOException) {
            file?.failWrite(outputStream)
            false
        } catch (_: JSONException) {
            file?.failWrite(outputStream)
            false
        }
    }

    internal fun getFile(): AtomicFile {
        return AtomicFile(File(context.filesDir, FILE_NAME))
    }

    @Throws(JSONException::class)
    internal fun serializeSession(session: Session): JSONObject {
        return JSONObject().apply {
            put("url", session.url)
        }
    }

    @Throws(JSONException::class)
    internal fun deserializeSession(id: String, json: JSONObject): Session {
        return Session(json.getString("url"), id)
    }

    private fun serializeEngineSession(engineSession: EngineSession): JSONObject {
        return JSONObject().apply {
            engineSession.saveState().forEach({ k, v -> if (shouldSerialize(v)) put(k, v) })
        }
    }

    private fun deserializeEngineSession(engine: Engine, json: JSONObject): EngineSession {
        return engine.createSession().apply {
            val values = mutableMapOf<String, Any>()
            json.keys().forEach { k -> values[k] = json[k] }
            restoreState(values)
        }
    }

    private fun shouldSerialize(value: Any): Boolean {
        // For now we only persist primitive types
        return when (value) {
            is Number -> true
            is Boolean -> true
            is String -> true
            else -> false
        }
    }
}
