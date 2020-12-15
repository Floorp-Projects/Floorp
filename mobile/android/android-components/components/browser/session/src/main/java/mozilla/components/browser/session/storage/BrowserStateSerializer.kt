/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import android.util.AtomicFile
import android.util.JsonReader
import android.util.JsonWriter
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.ktx.android.util.nextBooleanOrNull
import mozilla.components.support.ktx.android.util.nextStringOrNull
import mozilla.components.support.ktx.util.readJSON
import mozilla.components.support.ktx.util.streamJSON
import java.util.UUID

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

    /**
     * Writes a single [TabSessionState] to [file] in JSON format.
     */
    fun writeTab(
        tab: TabSessionState,
        file: AtomicFile
    ): Boolean = file.streamJSON { tab(tab) }

    /**
     * Reads a single [RecoverableTab] from the given [file].
     *
     * @param engine The [Engine] implementation for restoring the engine state.
     * @param restoreSessionId Whether the original tab ID should be restored or whether a new ID
     * should be generated for the tab.
     * @param restoreParentId Whether the original parent tab ID should be restored or whether it
     * should be set to `null`.
     */
    fun readTab(
        engine: Engine,
        file: AtomicFile,
        restoreSessionId: Boolean = true,
        restoreParentId: Boolean = true
    ): RecoverableTab? {
        return file.readJSON { tab(engine, restoreSessionId, restoreParentId) }
    }
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

private fun JsonReader.tab(
    engine: Engine,
    restoreSessionId: Boolean = true,
    restoreParentId: Boolean = true
): RecoverableTab? {
    beginObject()

    var engineSessionState: EngineSessionState? = null
    var tab: RecoverableTab? = null

    while (hasNext()) {
        when (nextName()) {
            Keys.SESSION_KEY -> tab = tabSession()
            Keys.ENGINE_SESSION_KEY -> engineSessionState = engine.createSessionStateFrom(this)
        }
    }

    endObject()

    return tab?.copy(
        state = engineSessionState,
        id = if (restoreSessionId) tab.id else UUID.randomUUID().toString(),
        parentId = if (restoreParentId) tab.parentId else null
    )
}

@Suppress("ComplexMethod")
private fun JsonReader.tabSession(): RecoverableTab? {
    var id: String? = null
    var parentId: String? = null
    var url: String? = null
    var title: String? = null
    var contextId: String? = null
    var lastAccess: Long? = null

    var readerStateActive: Boolean? = null
    var readerActiveUrl: String? = null

    beginObject()

    while (hasNext()) {
        when (val name = nextName()) {
            Keys.SESSION_URL_KEY -> url = nextString()
            Keys.SESSION_UUID_KEY -> id = nextString()
            Keys.SESSION_CONTEXT_ID_KEY -> contextId = nextStringOrNull()
            Keys.SESSION_PARENT_UUID_KEY -> parentId = nextStringOrNull()
            Keys.SESSION_TITLE -> title = nextString()
            Keys.SESSION_READER_MODE_KEY -> readerStateActive = nextBooleanOrNull()
            Keys.SESSION_READER_MODE_ACTIVE_URL_KEY -> readerActiveUrl = nextStringOrNull()
            Keys.SESSION_LAST_ACCESS -> lastAccess = nextLong()
            else -> throw IllegalArgumentException("Unknown session key: $name")
        }
    }

    endObject()

    return RecoverableTab(
        id = requireNotNull(id),
        parentId = parentId,
        url = requireNotNull(url),
        title = requireNotNull(title),
        contextId = contextId,
        state = null, // This will be deserialized and added separately
        readerState = ReaderState(
            active = readerStateActive ?: false,
            activeUrl = readerActiveUrl
        ),
        private = false, // We never serialize private sessions
        lastAccess = lastAccess ?: 0
    )
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
