/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage.serialize

import android.util.AtomicFile
import android.util.JsonReader
import android.util.JsonToken
import mozilla.components.browser.session.storage.RecoverableBrowserState
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.LastMediaAccessState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.ktx.android.util.nextBooleanOrNull
import mozilla.components.support.ktx.android.util.nextIntOrNull
import mozilla.components.support.ktx.android.util.nextStringOrNull
import mozilla.components.support.ktx.util.readJSON
import java.util.UUID

/**
 * Reads a [RecoverableBrowserState] (partial, serialized [BrowserState]) or a single [RecoverableTab]
 * (partial, serialized [TabSessionState]) from disk.
 */
class BrowserStateReader {
    /**
     * Reads a serialized [RecoverableBrowserState] from the given [AtomicFile].
     *
     * @param engine The [Engine] implementation for restoring the engine state.
     * @param file The [AtomicFile] to read the the recoverable state from.
     * @param predicate an optional predicate applied to each tab to determine if it should be restored.
     */
    fun read(
        engine: Engine,
        file: AtomicFile,
        predicate: (RecoverableTab) -> Boolean = { true },
    ): RecoverableBrowserState? {
        return file.readJSON {
            browsingSession(
                engine,
                restoreSessionId = true,
                restoreParentId = true,
                predicate = predicate,
            )
        }
    }

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
        restoreParentId: Boolean = true,
    ): RecoverableTab? {
        return file.readJSON { tab(engine, restoreSessionId, restoreParentId) }
    }
}

@Suppress("ComplexMethod")
private fun JsonReader.browsingSession(
    engine: Engine,
    restoreSessionId: Boolean = true,
    restoreParentId: Boolean = true,
    predicate: (RecoverableTab) -> Boolean = { true },
): RecoverableBrowserState? {
    beginObject()

    var version = 1 // Initially we didn't save a version. If there's none then we assume it is version 1.
    var tabs: List<RecoverableTab>? = null
    var selectedIndex: Int? = null
    var selectedTabId: String? = null

    while (hasNext()) {
        when (nextName()) {
            Keys.VERSION_KEY -> version = nextInt()
            Keys.SELECTED_SESSION_INDEX_KEY -> selectedIndex = nextInt()
            Keys.SELECTED_TAB_ID_KEY -> selectedTabId = nextStringOrNull()
            Keys.SESSION_STATE_TUPLES_KEY -> tabs = tabs(engine, restoreSessionId, restoreParentId, predicate)
        }
    }

    endObject()

    if (selectedTabId == null && version == 1 && selectedIndex != null) {
        // In the first version we only saved the selected index in the list of all tabs instead
        // of the ID of the selected tab. If we come across such an older version then we try
        // to find the tab and determine the selected ID ourselves.
        selectedTabId = tabs?.getOrNull(selectedIndex)?.state?.id
    }

    return if (tabs != null && tabs.isNotEmpty()) {
        // Check if selected tab still exists after restoring/filtering and
        // use most recently accessed tab otherwise.
        if (tabs.find { it.state.id == selectedTabId } == null) {
            selectedTabId = tabs.sortedByDescending { it.state.lastAccess }.first().state.id
        }

        RecoverableBrowserState(tabs, selectedTabId)
    } else {
        null
    }
}

private fun JsonReader.tabs(
    engine: Engine,
    restoreSessionId: Boolean = true,
    restoreParentId: Boolean = true,
    predicate: (RecoverableTab) -> Boolean = { true },
): List<RecoverableTab> {
    beginArray()

    val tabs = mutableListOf<RecoverableTab>()
    while (peek() != JsonToken.END_ARRAY) {
        val tab = tab(engine, restoreSessionId, restoreParentId)
        if (tab != null && predicate(tab)) {
            tabs.add(tab)
        }
    }

    endArray()

    return tabs
}

private fun JsonReader.tab(
    engine: Engine,
    restoreSessionId: Boolean = true,
    restoreParentId: Boolean = true,
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
        engineSessionState = engineSessionState,
        state = tab.state.copy(
            id = if (restoreSessionId) tab.state.id else UUID.randomUUID().toString(),
            parentId = if (restoreParentId) tab.state.parentId else null,
        ),
    )
}

@Suppress("ComplexMethod", "LongMethod")
private fun JsonReader.tabSession(): RecoverableTab {
    var id: String? = null
    var parentId: String? = null
    var url: String? = null
    var title: String? = null
    var searchTerm: String = ""
    var contextId: String? = null
    var lastAccess: Long? = null
    var createdAt: Long? = null
    var lastMediaUrl: String? = null
    var lastMediaAccess: Long? = null
    var mediaSessionActive: Boolean? = null

    var readerStateActive: Boolean? = null
    var readerActiveUrl: String? = null

    var historyMetadataUrl: String? = null
    var historyMetadataSearchTerm: String? = null
    var historyMetadataReferrerUrl: String? = null

    var sourceId: Int? = null
    var externalSourcePackageId: String? = null
    var externalSourceCategory: Int? = null

    beginObject()

    while (hasNext()) {
        when (val name = nextName()) {
            Keys.SESSION_URL_KEY -> url = nextString()
            Keys.SESSION_UUID_KEY -> id = nextString()
            Keys.SESSION_CONTEXT_ID_KEY -> contextId = nextStringOrNull()
            Keys.SESSION_PARENT_UUID_KEY -> parentId = nextStringOrNull()?.takeIf { it.isNotEmpty() }
            Keys.SESSION_TITLE -> title = nextStringOrNull() ?: ""
            Keys.SESSION_SEARCH_TERM -> searchTerm = nextStringOrNull() ?: ""
            Keys.SESSION_READER_MODE_KEY -> readerStateActive = nextBooleanOrNull()
            Keys.SESSION_READER_MODE_ACTIVE_URL_KEY -> readerActiveUrl = nextStringOrNull()
            Keys.SESSION_HISTORY_METADATA_URL -> historyMetadataUrl = nextStringOrNull()
            Keys.SESSION_HISTORY_METADATA_SEARCH_TERM -> historyMetadataSearchTerm = nextStringOrNull()
            Keys.SESSION_HISTORY_METADATA_REFERRER_URL -> historyMetadataReferrerUrl = nextStringOrNull()
            Keys.SESSION_LAST_ACCESS -> lastAccess = nextLong()
            Keys.SESSION_CREATED_AT -> createdAt = nextLong()
            Keys.SESSION_LAST_MEDIA_URL -> lastMediaUrl = nextString()
            Keys.SESSION_LAST_MEDIA_SESSION_ACTIVE -> mediaSessionActive = nextBoolean()
            Keys.SESSION_LAST_MEDIA_ACCESS -> lastMediaAccess = nextLong()
            Keys.SESSION_SOURCE_ID -> sourceId = nextInt()
            Keys.SESSION_EXTERNAL_SOURCE_PACKAGE_ID -> externalSourcePackageId = nextStringOrNull()
            Keys.SESSION_EXTERNAL_SOURCE_PACKAGE_CATEGORY -> externalSourceCategory = nextIntOrNull()
            Keys.SESSION_DEPRECATED_SOURCE_KEY -> nextString()
            else -> throw IllegalArgumentException("Unknown session key: $name")
        }
    }

    endObject()

    return RecoverableTab(
        engineSessionState = null, // This will be deserialized and added separately
        state = TabState(
            id = requireNotNull(id),
            parentId = parentId,
            url = requireNotNull(url),
            title = requireNotNull(title),
            searchTerm = requireNotNull(searchTerm),
            contextId = contextId,
            readerState = ReaderState(
                active = readerStateActive ?: false,
                activeUrl = readerActiveUrl,
            ),
            historyMetadata = if (historyMetadataUrl != null) {
                HistoryMetadataKey(
                    historyMetadataUrl,
                    historyMetadataSearchTerm,
                    historyMetadataReferrerUrl,
                )
            } else {
                null
            },
            private = false, // We never serialize private sessions
            lastAccess = lastAccess ?: 0,
            createdAt = createdAt ?: 0,
            lastMediaAccessState = LastMediaAccessState(
                lastMediaUrl ?: "",
                lastMediaAccess = lastMediaAccess ?: 0,
                mediaSessionActive = mediaSessionActive ?: false,
            ),
            source = SessionState.Source.restore(sourceId, externalSourcePackageId, externalSourceCategory),
        ),
    )
}
