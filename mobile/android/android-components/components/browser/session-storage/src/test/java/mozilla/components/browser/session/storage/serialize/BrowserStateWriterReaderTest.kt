/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage.serialize

import android.util.AtomicFile
import android.util.JsonReader
import android.util.JsonWriter
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.ktx.util.streamJSON
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class BrowserStateWriterReaderTest {
    @Test
    fun `Read and write single tab`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work"
        ).copy(
            engineState = EngineState(engineSessionState = engineState)
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString())
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertEquals("https://www.mozilla.org", restoredTab.url)
        assertEquals("Mozilla", restoredTab.title)
        assertEquals("work", restoredTab.contextId)
        assertEquals(engineState, restoredTab.state)
        assertFalse(restoredTab.readerState.active)
        assertNull(restoredTab.readerState.activeUrl)
    }

    @Test
    fun `Read and write tab with reader state`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
            readerState = ReaderState(active = true, activeUrl = "https://www.example.org")
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString())
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertTrue(restoredTab.readerState.active)
        assertEquals("https://www.example.org", restoredTab.readerState.activeUrl)
    }

    @Test
    fun `Read tab with session source`() {
        // We don't write tabs with session source anymore but need to be tolerant to
        // session source being in the JSON to remain backward compatible.
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)
        val tab = createTab(url = "https://www.mozilla.org", title = "Mozilla")
        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString())
        )
        writeTabWithSource(tab, file)

        // When reading we don't care about the source either as we will just set
        // it to RESTORED. So we just need to make sure we de-serialized successfully.
        val reader = BrowserStateReader()
        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertEquals("https://www.mozilla.org", restoredTab.url)
        assertEquals("Mozilla", restoredTab.title)
    }

    @Test
    fun `Read and write tab with history metadata`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
            historyMetadata = HistoryMetadataKey(
                "https://www.mozilla.org",
                searchTerm = "test",
                referrerUrl = "https://firefox.com"
            )
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString())
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertNotNull(restoredTab.historyMetadata)
        assertEquals(tab.content.url, restoredTab.historyMetadata!!.url)
    }
}

private fun createFakeEngineState(): EngineSessionState {
    val state: EngineSessionState = mock()
    whenever(state.writeTo(any())).then {
        val writer = it.arguments[0] as JsonWriter
        writer.beginObject()
        writer.endObject()
    }
    return state
}

private fun createFakeEngine(engineState: EngineSessionState): Engine {
    val engine: Engine = mock()
    whenever(engine.createSessionStateFrom(any())).then {
        val reader = it.arguments[0] as JsonReader
        reader.beginObject()
        reader.endObject()
        engineState
    }
    return engine
}

private fun writeTabWithSource(tab: TabSessionState, file: AtomicFile) {
    file.streamJSON { tabWithSource(tab) }
}

private fun JsonWriter.tabWithSource(
    tab: TabSessionState
) {
    beginObject()

    name(Keys.SESSION_KEY)
    beginObject().apply {
        name(Keys.SESSION_URL_KEY)
        value(tab.content.url)

        name(Keys.SESSION_UUID_KEY)
        value(tab.id)

        name(Keys.SESSION_TITLE)
        value(tab.content.title)

        name(Keys.SESSION_SOURCE_KEY)
        value(tab.source.name)

        endObject()
    }

    endObject()
}
