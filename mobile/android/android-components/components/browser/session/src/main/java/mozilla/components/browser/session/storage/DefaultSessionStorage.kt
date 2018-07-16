/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import android.content.Context
import android.util.AtomicFile
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

const val SELECTED_SESSION_KEY = "selectedSession"
const val SESSION_KEY = "session"
const val ENGINE_SESSION_KEY = "engineSession"
const val VERSION_KEY = "version"
const val VERSION = 1
const val FILE_NAME_FORMAT = "mozilla_components_session_storage_%s.json"

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
class DefaultSessionStorage(
    private val context: Context,
    private val savePeriodically: Boolean = true,
    private val saveIntervalInSeconds: Long = 300,
    private val scheduler: ScheduledExecutorService = Executors.newSingleThreadScheduledExecutor()
) : SessionStorage {
    private var scheduledFuture: ScheduledFuture<*>? = null

    override fun start(sessionManager: SessionManager) {
        if (savePeriodically) {
            scheduledFuture = scheduler.scheduleAtFixedRate(
                { persist(sessionManager) },
                saveIntervalInSeconds,
                saveIntervalInSeconds,
                TimeUnit.SECONDS)
        }
    }

    override fun stop() {
        scheduledFuture?.cancel(false)
    }

    @Synchronized
    override fun restore(sessionManager: SessionManager): Boolean {
        return try {
            getFile(sessionManager.engine.name()).openRead().use {
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
                    val engineSession = deserializeEngineSession(
                        sessionManager.engine,
                        jsonSession.getJSONObject(ENGINE_SESSION_KEY))
                    sessionManager.add(session, engineSession = engineSession)
                }

                sessionManager.findSessionById(selectedSessionId)?.let { session ->
                    sessionManager.select(session)
                }
            }
            true
        } catch (_: IOException) {
            false
        } catch (_: JSONException) {
            false
        }
    }

    @Synchronized
    override fun persist(sessionManager: SessionManager): Boolean {
        var file: AtomicFile? = null
        var outputStream: FileOutputStream? = null

        return try {
            val json = JSONObject()
            json.put(VERSION_KEY, VERSION)
            json.put(SELECTED_SESSION_KEY, sessionManager.selectedSession?.id ?: "")

            sessionManager.sessions.forEach { session ->
                val sessionJson = JSONObject()
                sessionJson.put(SESSION_KEY, serializeSession(session))
                sessionJson.put(ENGINE_SESSION_KEY, serializeEngineSession(sessionManager.getEngineSession(session)))
                json.put(session.id, sessionJson)
            }

            file = getFile(sessionManager.engine.name())
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

    internal fun getFile(engine: String): AtomicFile {
        return AtomicFile(File(context.filesDir, String.format(FILE_NAME_FORMAT, engine).toLowerCase()))
    }

    @Throws(JSONException::class)
    internal fun serializeSession(session: Session): JSONObject {
        return JSONObject().apply {
            put("url", session.url)
            put("source", session.source.name)
        }
    }

    @Throws(JSONException::class)
    internal fun deserializeSession(id: String, json: JSONObject): Session {
        val source = try {
            Source.valueOf(json.getString("source"))
        } catch (e: IllegalArgumentException) {
            Source.NONE
        }
        return Session(json.getString("url"), source, id)
    }

    private fun serializeEngineSession(engineSession: EngineSession?): JSONObject {
        if (engineSession == null) {
            return JSONObject()
        }
        return JSONObject().apply {
            engineSession.saveState().forEach { k, v -> if (shouldSerialize(v)) put(k, v) }
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
