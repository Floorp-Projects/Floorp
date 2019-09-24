/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.util.UUID

// Current version of the format used.
private const val VERSION = 1

/**
 * Helper to transform [SessionManager.Snapshot] instances to JSON and back.
 *
 * @param restoreSessionIds If true the original [Session.id] of [Session]s will be restored. Otherwise a new ID will be
 * generated. An app may prefer to use new IDs if it expects sessions to get restored multiple times - otherwise
 * breaking the promise of a unique ID.
 * @param restoreParentIds If true the [Session.parentId] will be restored, otherwise it will remain null. Setting
 * this to false is useful for features that do not have or rely on a parent/child relationship between tabs, esp.
 * if it can't be guaranteed that the parent tab is still available when the child tabs are restored.
 */
class SnapshotSerializer(
    private val restoreSessionIds: Boolean = true,
    private val restoreParentIds: Boolean = true
) {
    fun toJSON(snapshot: SessionManager.Snapshot): String {
        val json = JSONObject()
        json.put(Keys.VERSION_KEY, VERSION)
        json.put(Keys.SELECTED_SESSION_INDEX_KEY, snapshot.selectedSessionIndex)

        val sessions = JSONArray()
        snapshot.sessions.forEachIndexed { index, sessionWithState ->
            sessions.put(index, itemToJSON(sessionWithState))
        }
        json.put(Keys.SESSION_STATE_TUPLES_KEY, sessions)

        return json.toString()
    }

    fun itemToJSON(item: SessionManager.Snapshot.Item): JSONObject {
        val sessionJson = JSONObject()
        sessionJson.put(Keys.SESSION_KEY, serializeSession(item.session))

        val engineSessionState = if (item.engineSessionState != null) {
            item.engineSessionState.toJSON()
        } else {
            item.engineSession?.saveState()?.toJSON() ?: JSONObject()
        }
        sessionJson.put(Keys.ENGINE_SESSION_KEY, engineSessionState)

        return sessionJson
    }

    fun fromJSON(engine: Engine, json: String): SessionManager.Snapshot {
        val tuples: MutableList<SessionManager.Snapshot.Item> = mutableListOf()

        val jsonRoot = JSONObject(json)
        val selectedSessionIndex = jsonRoot.getInt(Keys.SELECTED_SESSION_INDEX_KEY)

        val sessionStateTuples = jsonRoot.getJSONArray(Keys.SESSION_STATE_TUPLES_KEY)
        for (i in 0 until sessionStateTuples.length()) {
            val sessionStateTupleJson = sessionStateTuples.getJSONObject(i)
            tuples.add(itemFromJSON(engine, sessionStateTupleJson))
        }

        return SessionManager.Snapshot(
            sessions = tuples,
            selectedSessionIndex = selectedSessionIndex
        )
    }

    fun itemFromJSON(engine: Engine, json: JSONObject): SessionManager.Snapshot.Item {
        val session = deserializeSession(json.getJSONObject(Keys.SESSION_KEY), restoreSessionIds, restoreParentIds)
        val state = engine.createSessionState(json.getJSONObject(Keys.ENGINE_SESSION_KEY))

        return SessionManager.Snapshot.Item(session, engineSession = null, engineSessionState = state)
    }
}

@Throws(JSONException::class)
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun serializeSession(session: Session): JSONObject {
    return JSONObject().apply {
        put(Keys.SESSION_URL_KEY, session.url)
        put(Keys.SESSION_SOURCE_KEY, session.source.name)
        put(Keys.SESSION_UUID_KEY, session.id)
        put(Keys.SESSION_PARENT_UUID_KEY, session.parentId ?: "")
        put(Keys.SESSION_TITLE, session.title)
        put(Keys.SESSION_READER_MODE_KEY, session.readerMode)
    }
}

@Throws(JSONException::class)
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun deserializeSession(json: JSONObject, restoreId: Boolean, restoreParentId: Boolean): Session {
    val source = try {
        Session.Source.valueOf(json.getString(Keys.SESSION_SOURCE_KEY))
    } catch (e: IllegalArgumentException) {
        Session.Source.NONE
    }
    val session = Session(
        json.getString(Keys.SESSION_URL_KEY),
        // Currently, snapshot cannot contain private sessions.
        false,
        source,
        if (restoreId) {
            json.getString(Keys.SESSION_UUID_KEY)
        } else {
            UUID.randomUUID().toString()
        }
    )
    if (restoreParentId) {
        session.parentId = json.getString(Keys.SESSION_PARENT_UUID_KEY).takeIf { it != "" }
    }
    session.title = if (json.has(Keys.SESSION_TITLE)) json.getString(Keys.SESSION_TITLE) else ""
    session.readerMode = json.optBoolean(Keys.SESSION_READER_MODE_KEY, false)
    return session
}

private object Keys {
    const val SELECTED_SESSION_INDEX_KEY = "selectedSessionIndex"
    const val SESSION_STATE_TUPLES_KEY = "sessionStateTuples"

    const val SESSION_SOURCE_KEY = "source"
    const val SESSION_URL_KEY = "url"
    const val SESSION_UUID_KEY = "uuid"
    const val SESSION_PARENT_UUID_KEY = "parentUuid"
    const val SESSION_READER_MODE_KEY = "readerMode"
    const val SESSION_TITLE = "title"

    const val SESSION_KEY = "session"
    const val ENGINE_SESSION_KEY = "engineSession"

    const val VERSION_KEY = "version"
}
