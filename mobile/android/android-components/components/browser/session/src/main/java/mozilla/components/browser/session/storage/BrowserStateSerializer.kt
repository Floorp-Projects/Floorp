/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import android.util.AtomicFile
import android.util.JsonWriter
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.ktx.util.streamJSON

private const val VERSION = 2

/**
 * Helper to transform [BrowserState] instances to JSON and.
 */
class BrowserStateSerializer {
    /**
     * Writes the [BrowserState] to [file] as JSON.
     */
    fun write(
        state: BrowserState,
        file: AtomicFile
    ): Boolean = file.streamJSON { state(state) }
}

/**
 * Writes [BrowserState] to [JsonWriter].
 */
private fun JsonWriter.state(
    state: BrowserState
) {
    beginObject()

    name(Keys.VERSION_KEY)
    value(VERSION)

    name(Keys.SELECTED_TAB_ID_KEY)
    value(state.selectedTabId)

    name(Keys.SESSION_STATE_TUPLES_KEY)

    beginArray()

    state.tabs.filter { !it.content.private }.forEachIndexed { _, tab ->
        tab(tab)
    }

    endArray()

    endObject()
}

/**
 * Writes a [TabSessionState] to [JsonWriter].
 */
private fun JsonWriter.tab(
    tab: TabSessionState
) {
    beginObject()

    name(Keys.SESSION_KEY)
    beginObject().apply {
        name(Keys.SESSION_URL_KEY)
        value(tab.content.url)

        name(Keys.SESSION_UUID_KEY)
        value(tab.id)

        name(Keys.SESSION_PARENT_UUID_KEY)
        value(tab.parentId ?: "")

        name(Keys.SESSION_TITLE)
        value(tab.content.title)

        name(Keys.SESSION_CONTEXT_ID_KEY)
        value(tab.contextId)

        name(Keys.SESSION_READER_MODE_KEY)
        value(tab.readerState.active)

        name(Keys.SESSION_LAST_ACCESS)
        value(tab.lastAccess)

        if (tab.readerState.active && tab.readerState.activeUrl != null) {
            name(Keys.SESSION_READER_MODE_ACTIVE_URL_KEY)
            value(tab.readerState.activeUrl)
        }

        endObject()
    }

    name(Keys.ENGINE_SESSION_KEY)
    engineSession(tab.engineState.engineSessionState)

    endObject()
}

/**
 * Writes a (nullable) [EngineSessionState] to [JsonWriter].
 */
private fun JsonWriter.engineSession(engineSessionState: EngineSessionState?) {
    if (engineSessionState == null) {
        beginObject()
        endObject()
    } else {
        engineSessionState.writeTo(this)
    }
}
