/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import org.json.JSONArray
import org.json.JSONObject

private const val VERSION = 2

/**
 * Helper to transform [BrowserState] instances to JSON and.
 */
class BrowserStateSerializer {

    /**
     * Serializes the provided [BrowserState] to JSON.
     *
     * @param state the state to serialize.
     * @return the serialized state as JSON.
     */
    fun toJSON(state: BrowserState): String {
        val json = JSONObject()
        json.put(Keys.VERSION_KEY, VERSION)
        json.put(Keys.SELECTED_TAB_ID_KEY, state.selectedTabId)

        val sessions = JSONArray()
        state.tabs.filter { !it.content.private }.forEachIndexed { index, tab ->
            sessions.put(index, tabToJSON(tab))
        }

        json.put(Keys.SESSION_STATE_TUPLES_KEY, sessions)

        return json.toString()
    }

    private fun tabToJSON(tab: TabSessionState): JSONObject {
        val itemJson = JSONObject()

        val sessionJson = JSONObject().apply {
            put(Keys.SESSION_URL_KEY, tab.content.url)
            put(Keys.SESSION_UUID_KEY, tab.id)
            put(Keys.SESSION_PARENT_UUID_KEY, tab.parentId ?: "")
            put(Keys.SESSION_TITLE, tab.content.title)
            put(Keys.SESSION_CONTEXT_ID_KEY, tab.contextId)
        }

        sessionJson.put(Keys.SESSION_READER_MODE_KEY, tab.readerState.active)
        if (tab.readerState.active && tab.readerState.activeUrl != null) {
            sessionJson.put(Keys.SESSION_READER_MODE_ACTIVE_URL_KEY, tab.readerState.activeUrl)
        }
        sessionJson.put(Keys.SESSION_LAST_ACCESS, tab.lastAccess)

        itemJson.put(Keys.SESSION_KEY, sessionJson)

        val engineSessionState = tab.engineState.engineSessionState
        val engineSessionStateJson = engineSessionState?.toJSON() ?: JSONObject()
        itemJson.put(Keys.ENGINE_SESSION_KEY, engineSessionStateJson)

        return itemJson
    }
}
