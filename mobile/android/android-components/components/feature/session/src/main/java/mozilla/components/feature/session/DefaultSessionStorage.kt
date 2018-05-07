/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.json.JSONObject
import java.io.FileNotFoundException
import java.util.UUID

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
    private val sessions = mutableMapOf<String, SessionProxy>()
    private var selectedSession: Session? = null

    @Synchronized
    override fun add(session: SessionProxy): String {
        val id = UUID.randomUUID().toString()
        sessions.put(id, session)
        return id
    }

    @Synchronized
    override fun remove(id: String): Boolean {
        return sessions.remove(id) != null
    }

    @Synchronized
    override fun get(id: String): SessionProxy? {
        return sessions[id]
    }

    @Synchronized
    override fun getSelected(): Session? {
        return selectedSession
    }

    @Synchronized
    override fun restore(engine: Engine): List<SessionProxy> {
        return try {
            sessions.clear()

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
                    sessions.put(it, SessionProxy(session, engineSession))
                }
                selectedSession = sessions[selectedSessionId]?.session
            }
            sessions.values.toList()
        } catch (_: FileNotFoundException) {
            emptyList()
        }
    }

    @Synchronized
    override fun persist(selectedSession: Session?): Boolean {
        return try {
            val json = JSONObject()
            json.put(VERSION_KEY, VERSION)

            var selectedSessionId = ""
            for ((key, value) in sessions) {
                if (value.session == selectedSession) {
                    selectedSessionId = key
                }
            }
            json.put(SELECTED_SESSION_KEY, selectedSessionId)

            sessions.forEach({ (key, value) ->
                val sessionJson = JSONObject()
                sessionJson.put(SESSION_KEY, serializeSession(value.session))
                sessionJson.put(ENGINE_SESSION_KEY, serializeEngineSession(value.engineSession))
                json.put(key, sessionJson)
            })

            context.openFileOutput(FILE_NAME, Context.MODE_PRIVATE).use {
                println(json.toString())
                it.write(json.toString().toByteArray())
            }
            true
        } catch (_: FileNotFoundException) {
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
        return JSONObject()
    }

    private fun deserializeEngineSession(engine: Engine, json: JSONObject): EngineSession {
        return engine.createSession()
    }
}
