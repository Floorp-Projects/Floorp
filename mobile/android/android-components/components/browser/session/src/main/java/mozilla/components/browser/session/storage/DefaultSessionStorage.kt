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
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

const val SELECTED_SESSION_INDEX_KEY = "selectedSessionIndex"
const val SESSION_STATE_TUPLES_KEY = "sessionStateTuples"

const val SESSION_SOURCE_KEY = "source"
const val SESSION_URL_KEY = "url"
const val SESSION_UUID_KEY = "uuid"

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
 *     "version": Int,
 *     "selectedSessionIndex": Int,
 *     "sessionStateTuples": [
 *         {
 *             "session": {},
 *             "engineSession": {}
 *         },
 *         ...
 *     ]
 * }
 */
@Suppress("TooManyFunctions")
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
                { sessionManager.createSnapshot()?.let { persist(sessionManager.engine, it) } },
                saveIntervalInSeconds,
                saveIntervalInSeconds,
                TimeUnit.SECONDS)
        }
    }

    override fun stop() {
        scheduledFuture?.cancel(false)
    }

    @Synchronized
    override fun clear(engine: Engine) {
        getFile(engine.name()).delete()
    }

    @Synchronized
    @SuppressWarnings("ReturnCount")
    override fun read(engine: Engine): SessionsSnapshot? {
        val tuples: MutableList<SessionWithState> = mutableListOf()
        var selectedSessionIndex = 0

        try {
            getFile(engine.name()).openRead().use {
                val json = it.bufferedReader().use {
                    it.readText()
                }

                val jsonRoot = JSONObject(json)
                selectedSessionIndex = jsonRoot.getInt(SELECTED_SESSION_INDEX_KEY)
                val sessionStateTuples = jsonRoot.getJSONArray(SESSION_STATE_TUPLES_KEY)
                for (i in 0..(sessionStateTuples.length() - 1)) {
                    val sessionStateTupleJson = sessionStateTuples.getJSONObject(i)
                    val session = deserializeSession(sessionStateTupleJson.getJSONObject(SESSION_KEY))
                    val engineSession = deserializeEngineSession(
                        engine,
                        sessionStateTupleJson.getJSONObject(ENGINE_SESSION_KEY)
                    )
                    tuples.add(SessionWithState(session, engineSession))
                }
            }
        } catch (_: IOException) {
            return null
        } catch (_: JSONException) {
            return null
        }

        if (tuples.isEmpty()) {
            return null
        }

        // If we see an illegal selected index on disk, reset it to 0.
        if (tuples.getOrNull(selectedSessionIndex) == null) {
            selectedSessionIndex = 0
        }

        return SessionsSnapshot(
            sessions = tuples,
            selectedSessionIndex = selectedSessionIndex
        )
    }

    @Synchronized
    override fun persist(engine: Engine, snapshot: SessionsSnapshot): Boolean {
        require(snapshot.sessions.isNotEmpty()) {
            "SessionsSnapshot must not be empty"
        }
        requireNotNull(snapshot.sessions.getOrNull(snapshot.selectedSessionIndex)) {
            "SessionSnapshot's selected index must be in bounds"
        }

        var file: AtomicFile? = null
        var outputStream: FileOutputStream? = null

        return try {
            val json = JSONObject()
            json.put(VERSION_KEY, VERSION)
            json.put(SELECTED_SESSION_INDEX_KEY, snapshot.selectedSessionIndex)

            val sessions = JSONArray()
            snapshot.sessions.forEachIndexed { index, sessionWithState ->
                val sessionJson = JSONObject()
                sessionJson.put(SESSION_KEY, serializeSession(sessionWithState.session))
                sessionJson.put(ENGINE_SESSION_KEY, serializeEngineSession(sessionWithState.engineSession))
                sessions.put(index, sessionJson)
            }
            json.put(SESSION_STATE_TUPLES_KEY, sessions)

            file = getFile(engine.name())
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
            put(SESSION_URL_KEY, session.url)
            put(SESSION_SOURCE_KEY, session.source.name)
            put(SESSION_UUID_KEY, session.id)
        }
    }

    @Throws(JSONException::class)
    internal fun deserializeSession(json: JSONObject): Session {
        val source = try {
            Source.valueOf(json.getString(SESSION_SOURCE_KEY))
        } catch (e: IllegalArgumentException) {
            Source.NONE
        }
        return Session(
                json.getString(SESSION_URL_KEY),
                // Currently, snapshot cannot contain private sessions.
                false,
                source,
                json.getString(SESSION_UUID_KEY)
        )
    }

    private fun serializeEngineSession(engineSession: EngineSession?): JSONObject {
        if (engineSession == null) {
            return JSONObject()
        }
        return JSONObject().apply {
            engineSession.saveState().forEach { (k, v) -> if (shouldSerialize(v)) put(k, v) }
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
