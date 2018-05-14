/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.json.JSONException
import org.json.JSONObject
import java.io.FileNotFoundException

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
        val sessions = mutableMapOf<Session, EngineSession>()
        return try {
            context.openFileInput(FILE_NAME).use {
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
                    val session = deserializeSession(jsonSession.getJSONObject(SESSION_KEY))
                    val engineSession = deserializeEngineSession(engine, jsonSession.getJSONObject(ENGINE_SESSION_KEY))
                    sessions.put(session, engineSession)
                }
                Pair(sessions, selectedSessionId)
            }
        } catch (_: FileNotFoundException) {
            Pair(emptyMap(), "")
        } catch (_: JSONException) {
            Pair(emptyMap(), "")
        }
    }

    @Synchronized
    override fun persist(sessions: Map<Session, EngineSession>, selectedSession: String): Boolean {
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

            context.openFileOutput(FILE_NAME, Context.MODE_PRIVATE).use {
                it.write(json.toString().toByteArray())
            }
            true
        } catch (_: FileNotFoundException) {
            false
        } catch (_: JSONException) {
            false
        }
    }

    private fun serializeSession(session: Session): JSONObject {
        val json = JSONObject()
        json.put("url", session.url)
        return json
    }

    private fun deserializeSession(json: JSONObject): Session {
        return Session(json.getString("url"))
    }

    private fun serializeEngineSession(engineSession: EngineSession): JSONObject {
        // We don't currently persist any engine session state
        return JSONObject()
    }

    private fun deserializeEngineSession(engine: Engine, json: JSONObject): EngineSession {
        // We don't currently persist any engine session state
        return engine.createSession()
    }
}
