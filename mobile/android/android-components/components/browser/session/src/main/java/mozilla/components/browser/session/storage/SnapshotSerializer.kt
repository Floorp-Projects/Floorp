/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.Engine
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.lang.IllegalStateException
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
        val itemJson = JSONObject()

        val sessionJson = serializeSession(item.session)
        sessionJson.put(Keys.SESSION_READER_MODE_KEY, item.readerState?.active ?: false)
        if (item.readerState?.active == true && item.readerState.activeUrl != null) {
            sessionJson.put(Keys.SESSION_READER_MODE_ACTIVE_URL_KEY, item.readerState.activeUrl)
        }
        sessionJson.put(Keys.SESSION_LAST_ACCESS, item.lastAccess)
        itemJson.put(Keys.SESSION_KEY, sessionJson)

        val engineSessionState = if (item.engineSessionState != null) {
            item.engineSessionState.toJSON()
        } else {
            item.engineSession?.saveState()?.toJSON() ?: JSONObject()
        }
        itemJson.put(Keys.ENGINE_SESSION_KEY, engineSessionState)
        return itemJson
    }

    fun fromJSON(engine: Engine, json: String): SessionManager.Snapshot {
        val tuples: MutableList<SessionManager.Snapshot.Item> = mutableListOf()

        val jsonRoot = JSONObject(json)

        val version = jsonRoot.getInt(Keys.VERSION_KEY)

        val sessionStateTuples = jsonRoot.getJSONArray(Keys.SESSION_STATE_TUPLES_KEY)
        for (i in 0 until sessionStateTuples.length()) {
            val sessionStateTupleJson = sessionStateTuples.getJSONObject(i)
            tuples.add(itemFromJSON(engine, sessionStateTupleJson))
        }

        val selectedSessionIndex = when (version) {
            1 -> jsonRoot.getInt(Keys.SELECTED_SESSION_INDEX_KEY)
            2 -> {
                val selectedTabId = jsonRoot.getString(Keys.SELECTED_TAB_ID_KEY)
                tuples.indexOfFirst { it.session.id == selectedTabId }
            }
            else -> {
                throw IllegalStateException("Unknown session store format version ($version")
            }
        }

        return SessionManager.Snapshot(
            sessions = tuples,
            selectedSessionIndex = selectedSessionIndex
        )
    }

    fun itemFromJSON(engine: Engine, json: JSONObject): SessionManager.Snapshot.Item {
        val sessionJson = json.getJSONObject(Keys.SESSION_KEY)
        val engineSessionJson = json.getJSONObject(Keys.ENGINE_SESSION_KEY)
        val session = deserializeSession(sessionJson, restoreSessionIds, restoreParentIds)
        val readerState = ReaderState(
            active = sessionJson.optBoolean(Keys.SESSION_READER_MODE_KEY, false),
            activeUrl = sessionJson.tryGetString(Keys.SESSION_READER_MODE_ACTIVE_URL_KEY)
        )
        val lastAccess = sessionJson.optLong(Keys.SESSION_LAST_ACCESS, 0)
        val engineState = engine.createSessionState(engineSessionJson)

        return SessionManager.Snapshot.Item(
            session,
            engineSession = null,
            engineSessionState = engineState,
            readerState = readerState,
            lastAccess = lastAccess
        )
    }
}

@Throws(JSONException::class)
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun serializeSession(session: Session): JSONObject {
    return JSONObject().apply {
        put(Keys.SESSION_URL_KEY, session.url)
        put(Keys.SESSION_UUID_KEY, session.id)
        put(Keys.SESSION_PARENT_UUID_KEY, session.parentId ?: "")
        put(Keys.SESSION_TITLE, session.title)
        put(Keys.SESSION_CONTEXT_ID_KEY, session.contextId)
    }
}

@Throws(JSONException::class)
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun deserializeSession(
    json: JSONObject,
    restoreId: Boolean,
    restoreParentId: Boolean
): Session {
    val session = Session(
        json.getString(Keys.SESSION_URL_KEY),
        // Currently, snapshot cannot contain private sessions.
        false,
        SessionState.Source.RESTORED,
        if (restoreId) {
            json.getString(Keys.SESSION_UUID_KEY)
        } else {
            UUID.randomUUID().toString()
        },
        json.tryGetString(Keys.SESSION_CONTEXT_ID_KEY)
    )
    if (restoreParentId) {
        session.parentId = json.getString(Keys.SESSION_PARENT_UUID_KEY).takeIf { it != "" }
    }
    session.title = if (json.has(Keys.SESSION_TITLE)) json.getString(Keys.SESSION_TITLE) else ""
    return session
}

internal object Keys {
    const val SELECTED_SESSION_INDEX_KEY = "selectedSessionIndex"
    const val SELECTED_TAB_ID_KEY = "selectedTabId"
    const val SESSION_STATE_TUPLES_KEY = "sessionStateTuples"

    const val SESSION_URL_KEY = "url"
    const val SESSION_UUID_KEY = "uuid"
    const val SESSION_CONTEXT_ID_KEY = "contextId"
    const val SESSION_PARENT_UUID_KEY = "parentUuid"
    const val SESSION_READER_MODE_KEY = "readerMode"
    const val SESSION_READER_MODE_ACTIVE_URL_KEY = "readerModeArticleUrl"
    const val SESSION_TITLE = "title"
    const val SESSION_LAST_ACCESS = "lastAccess"

    const val SESSION_KEY = "session"
    const val ENGINE_SESSION_KEY = "engineSession"

    const val VERSION_KEY = "version"
}
