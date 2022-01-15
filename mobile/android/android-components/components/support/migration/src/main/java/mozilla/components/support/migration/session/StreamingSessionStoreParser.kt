/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration.session

import android.util.JsonReader
import android.util.JsonToken
import mozilla.components.browser.session.storage.RecoverableBrowserState
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.support.migration.Result
import java.io.File
import java.io.IOException
import java.util.UUID

/**
 * Session store parser reading the session store as a stream, reducing memory impact.
 *
 * This is a lenient parser ignoring all unknown tokens, only cherry-picking the values we care
 * about.
 */
internal object StreamingSessionStoreParser {
    @Throws(IOException::class, IllegalStateException::class)
    internal fun parse(file: File): Result.Success<RecoverableBrowserState> {
        var state: RecoverableBrowserState? = null

        file.inputStream().bufferedReader().use {
            val reader = JsonReader(it)
            reader.isLenient = true

            reader.beginObject()

            while (reader.hasNext()) {
                val name = reader.nextName()
                if (name == "windows") {
                    state = parseWindows(reader)
                } else {
                    reader.skipValue()
                }
            }

            reader.endObject()
        }

        return Result.Success(
            state ?: RecoverableBrowserState(emptyList(), null)
        )
    }

    /**
     * Parses the windows array. We will try to parse the first window (via [parseWindow]) and
     * ignore all other windows. The assumption here is that Fennec only ever has one window.
     *
     * {
     *   "windows" [..]
     * }
     */
    private fun parseWindows(reader: JsonReader): RecoverableBrowserState {
        var sessions = listOf<RecoverableTab>()
        var selection = -1

        reader.beginArray()
        if (reader.hasNext()) {
            reader.beginObject()
            parseWindow(reader).apply {
                sessions = first
                selection = second
            }
            reader.endObject()
        }
        while (reader.hasNext()) {
            reader.skipValue()
        }
        reader.endArray()

        return RecoverableBrowserState(
            sessions,
            sessions.getOrNull(selection)?.state?.id
        )
    }

    /**
     * Parses a window object.
     *
     * {
     *   "tabs": [..]     // Delegating to [parseTabs]
     *   "selected": ..   // Determines selected tab.
     *   [..]             // Ignoring all other properties
     * }
     */
    private fun parseWindow(reader: JsonReader): Pair<List<RecoverableTab>, Int> {
        var tabs: List<RecoverableTab> = emptyList()
        var selection = -1

        while (reader.hasNext()) {
            when (reader.nextName()) {
                "tabs" -> {
                    tabs = parseTabs(reader)
                }
                "selected" -> {
                    selection = reader.nextInt() - 1
                }
                else -> {
                    reader.skipValue()
                }
            }
        }

        return Pair(tabs, selection)
    }

    /**
     * Parses a tab array.
     *
     * [
     *   ... // Delegates to [parseTab]
     * ]
     */
    private fun parseTabs(reader: JsonReader): List<RecoverableTab> {
        val tabs = mutableListOf<RecoverableTab>()

        reader.beginArray()

        while (reader.hasNext()) {
            reader.beginObject()
            parseTab(reader)?.let { session ->
                tabs.add(session)
            }
            reader.endObject()
        }

        reader.endArray()

        return tabs
    }

    /**
     * Parses a tab object.
     *
     * {
     *   "entries": [..] // Delegates to [parseEntries]
     *   "index": ..     // Used to determine current entry in tab history.
     *   ..              // Ignoring all other properties
     * }
     */
    private fun parseTab(reader: JsonReader): RecoverableTab? {
        var entries: List<RecoverableTab> = emptyList()
        var index = -1

        while (reader.hasNext()) {
            when (reader.nextName()) {
                "entries" -> {
                    entries = parseEntries(reader)
                }
                "index" -> {
                    index = reader.nextInt()
                }
                else -> {
                    reader.skipValue()
                }
            }
        }

        return if (index < 1 || entries.size < index) {
            // This tab has no entries at all. Let's just skip it.
            null
        } else {
            entries[index - 1]
        }
    }

    /**
     * Parses an entries array.
     *
     * [
     *   ... // Delegates to [parseEntry]
     * ]
     */
    private fun parseEntries(reader: JsonReader): List<RecoverableTab> {
        val entries = mutableListOf<RecoverableTab>()

        reader.beginArray()

        while (reader.hasNext()) {
            reader.beginObject()
            parseEntry(reader)?.let { entries.add(it) }
            reader.endObject()
        }

        reader.endArray()

        return entries
    }

    /**
     * Parses an entry object.
     *
     * {
     *   "url": ".."     // URL of the entry
     *   "title": ".."   // Title of the entry
     *   [..]            // Ignoring all other properties
     * }
     */
    private fun parseEntry(reader: JsonReader): RecoverableTab? {
        var parsedUrl: String? = null
        var parsedTitle: String? = null

        while (reader.hasNext()) {
            when (reader.nextName()) {
                "url" -> parsedUrl = reader.nextString()

                "title" -> if (reader.peek() == JsonToken.NULL) {
                    reader.nextNull()
                } else {
                    parsedTitle = reader.nextString()
                }

                else -> reader.skipValue()
            }
        }

        return parsedUrl?.let { url ->
            RecoverableTab(
                engineSessionState = null,
                state = TabState(
                    id = UUID.randomUUID().toString(),
                    url = url,
                    title = parsedTitle?.ifEmpty { url } ?: url
                )
            )
        }
    }
}
