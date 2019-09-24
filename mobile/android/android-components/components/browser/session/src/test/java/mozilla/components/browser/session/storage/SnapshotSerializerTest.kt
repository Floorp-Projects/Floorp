/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SnapshotSerializerTest {

    @Test
    fun `Serialize and deserialize session`() {
        val originalSession = Session(
            "https://www.mozilla.org",
            source = Session.Source.ACTION_VIEW,
            id = "test-id").apply {
            title = "Hello World"
            readerMode = true
        }

        val json = serializeSession(originalSession)
        val restoredSession = deserializeSession(json, restoreId = true, restoreParentId = false)

        assertEquals("https://www.mozilla.org", restoredSession.url)
        assertEquals(Session.Source.ACTION_VIEW, restoredSession.source)
        assertEquals("test-id", restoredSession.id)
        assertEquals("Hello World", restoredSession.title)
        assertTrue(restoredSession.readerMode)
    }

    @Test
    fun `Deserialize minimal session (without title)`() {
        val json = JSONObject().apply {
            put("url", "https://www.mozilla.org")
            put("source", "ACTION_VIEW")
            put("uuid", "test-id")
            put("parentUuid", "")
        }

        val restoredSession = deserializeSession(json, restoreId = true, restoreParentId = false)

        assertEquals("https://www.mozilla.org", restoredSession.url)
        assertEquals(Session.Source.ACTION_VIEW, restoredSession.source)
        assertEquals("test-id", restoredSession.id)
        assertFalse(restoredSession.readerMode)
    }

    @Test
    fun `Deserialize session without restoring id`() {
        val originalSession = Session(
            "https://www.mozilla.org",
            source = Session.Source.ACTION_VIEW,
            id = "test-id").apply {
            title = "Hello World"
            readerMode = true
        }

        val json = serializeSession(originalSession)
        val restoredSession = deserializeSession(json, restoreId = false, restoreParentId = false)

        assertEquals("https://www.mozilla.org", restoredSession.url)
        assertEquals(Session.Source.ACTION_VIEW, restoredSession.source)
        assertNotEquals("test-id", restoredSession.id)
        assertTrue(restoredSession.id.isNotBlank())
        assertEquals("Hello World", restoredSession.title)
        assertTrue(restoredSession.readerMode)

        val restoredSessionAgain = deserializeSession(json, restoreId = false, restoreParentId = false)
        assertNotEquals("test-id", restoredSessionAgain.id)
        assertNotEquals(restoredSession.id, restoredSessionAgain.id)
        assertTrue(restoredSessionAgain.id.isNotBlank())
    }

    @Test
    fun `Deserialize session without restoring parent id`() {
        val originalSession = Session(
                "https://www.mozilla.org",
                source = Session.Source.ACTION_VIEW,
                id = "test-id").apply {
            parentId = "test-parent-id"
            title = "Hello World"
            readerMode = true
        }

        val json = serializeSession(originalSession)
        val restoredSession = deserializeSession(json, restoreId = true, restoreParentId = true)
        assertEquals("https://www.mozilla.org", restoredSession.url)
        assertEquals(Session.Source.ACTION_VIEW, restoredSession.source)
        assertEquals("test-id", restoredSession.id)
        assertEquals("test-parent-id", restoredSession.parentId)
        assertTrue(restoredSession.id.isNotBlank())
        assertEquals("Hello World", restoredSession.title)
        assertTrue(restoredSession.readerMode)

        val restoredSessionAgain = deserializeSession(json, restoreId = true, restoreParentId = false)
        assertEquals("test-id", restoredSessionAgain.id)
        assertNull(restoredSessionAgain.parentId)
    }
}
