/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage.serialize

import android.util.AtomicFile
import android.util.JsonReader
import android.util.JsonWriter
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.ExternalPackage
import mozilla.components.browser.state.state.LastMediaAccessState
import mozilla.components.browser.state.state.PackageCategory
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.SessionState
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
            contextId = "work",
        ).copy(
            engineState = EngineState(engineSessionState = engineState),
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertEquals("https://www.mozilla.org", restoredTab.state.url)
        assertEquals("Mozilla", restoredTab.state.title)
        assertEquals("work", restoredTab.state.contextId)
        assertEquals(engineState, restoredTab.engineSessionState)
        assertFalse(restoredTab.state.readerState.active)
        assertNull(restoredTab.state.readerState.activeUrl)
    }

    @Test
    fun `Read and write tab with reader state`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
            readerState = ReaderState(active = true, activeUrl = "https://www.example.org"),
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertTrue(restoredTab.state.readerState.active)
        assertEquals("https://www.example.org", restoredTab.state.readerState.activeUrl)
    }

    @Test
    fun `Read tab with deprecated session source`() {
        // We don't persist session source of tabs unless it's an external source.
        // However, in older versions we did persist other types of sources so need to be tolerant
        // if these older cases are encountered in JSON to remain backward compatible.
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)
        val tab = createTab(url = "https://www.mozilla.org", title = "Mozilla")
        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )
        writeTabWithDeprecatedSource(tab, file)

        // When reading a tab that didn't have a source persisted, we just need to make sure
        // it is deserialized correctly. In this case, source defaults to `Internal.Restored`.
        val reader = BrowserStateReader()
        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)
        assertEquals(SessionState.Source.Internal.None, restoredTab.state.source)

        assertEquals("https://www.mozilla.org", restoredTab.state.url)
        assertEquals("Mozilla", restoredTab.state.title)
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
                referrerUrl = "https://firefox.com",
            ),
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertNotNull(restoredTab.state.historyMetadata)
        assertEquals(tab.content.url, restoredTab.state.historyMetadata!!.url)
    }

    @Test
    fun `Read and write tab with external custom tab source and full caller`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
            source = SessionState.Source.External.CustomTab(
                caller = ExternalPackage("com.mozilla.test", PackageCategory.PRODUCTIVITY),
            ),
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertNotNull(restoredTab.state.source)
        assertTrue(restoredTab.state.source is SessionState.Source.External.CustomTab)
        with(restoredTab.state.source as SessionState.Source.External.CustomTab) {
            assertEquals("com.mozilla.test", this.caller!!.packageId)
            assertEquals(PackageCategory.PRODUCTIVITY, this.caller!!.category)
        }
    }

    @Test
    fun `Read and write tab with external action view source and partial caller`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
            source = SessionState.Source.External.ActionView(
                caller = ExternalPackage("com.mozilla.test", category = PackageCategory.UNKNOWN),
            ),
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertNotNull(restoredTab.state.source)
        assertTrue(restoredTab.state.source is SessionState.Source.External.ActionView)
        with(restoredTab.state.source as SessionState.Source.External.ActionView) {
            assertEquals("com.mozilla.test", this.caller!!.packageId)
            assertEquals(PackageCategory.UNKNOWN, this.caller!!.category)
        }
    }

    @Test
    fun `Read and write tab with external action send source and missing caller`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
            source = SessionState.Source.External.ActionSend(caller = null),
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertNotNull(restoredTab.state.source)
        assertTrue(restoredTab.state.source is SessionState.Source.External.ActionSend)
        with(restoredTab.state.source as SessionState.Source.External.ActionSend) {
            assertNull(this.caller)
        }
    }

    @Test
    fun `Read and write tab with LastMediaAccessState`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
            lastMediaAccessState = LastMediaAccessState("https://www.mozilla.org", lastMediaAccess = 333L, true),
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertEquals("https://www.mozilla.org", restoredTab.state.lastMediaAccessState.lastMediaUrl)
        assertEquals(333L, restoredTab.state.lastMediaAccessState.lastMediaAccess)
        assertTrue(restoredTab.state.lastMediaAccessState.mediaSessionActive)
    }

    @Test
    fun `Read and write tab with createdAt`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)
        val currentTime = System.currentTimeMillis()

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
            createdAt = currentTime,
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertEquals(currentTime, restoredTab.state.createdAt)
    }

    @Test
    fun `Read and write tab without createdAt`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertNotNull(restoredTab.state.createdAt)
    }

    @Test
    fun `Read and write tab with search term`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
            searchTerms = "test search",
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertEquals("test search", restoredTab.state.searchTerm)
    }

    @Test
    fun `Read and write tab without search term`() {
        val engineState = createFakeEngineState()
        val engine = createFakeEngine(engineState)

        val tab = createTab(
            url = "https://www.mozilla.org",
            title = "Mozilla",
            contextId = "work",
        )

        val writer = BrowserStateWriter()
        val reader = BrowserStateReader()

        val file = AtomicFile(
            File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString()),
        )

        assertTrue(writer.writeTab(tab, file))

        val restoredTab = reader.readTab(engine, file)
        assertNotNull(restoredTab!!)

        assertEquals("", restoredTab.state.searchTerm)
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

private fun writeTabWithDeprecatedSource(tab: TabSessionState, file: AtomicFile) {
    file.streamJSON { tabWithDeprecatedSource(tab) }
}

private fun JsonWriter.tabWithDeprecatedSource(
    tab: TabSessionState,
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

        name(Keys.SESSION_DEPRECATED_SOURCE_KEY)
        value(tab.source.toString())

        endObject()
    }

    endObject()
}
