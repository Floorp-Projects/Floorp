/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ExperimentPayloadTest {
    @Test
    fun testGet() {
        val payload = ExperimentPayload()
        payload.put("key", "value")
        assertEquals("value", payload.get("key"))
        assertNull("other", payload.get("other"))
    }

    @Test
    fun testGetKeys() {
        val payload = ExperimentPayload()
        payload.put("first-key", "first-value")
        payload.put("second-key", "second-value")
        val keys = payload.getKeys()
        assertEquals(2, keys.size)
        assertTrue(keys.contains("first-key"))
        assertTrue(keys.contains("second-key"))
    }

    @Test
    fun testGetBooleanList() {
        val payload = ExperimentPayload()
        payload.put("boolean-key", listOf(true, false))
        assertEquals(listOf(true, false), payload.getBooleanList("boolean-key"))
    }

    @Test(expected = ClassCastException::class)
    fun testGetBooleanListInvalidType() {
        val payload = ExperimentPayload()
        payload.put("boolean-key", "other-value")
        payload.getBooleanList("boolean-key")
    }

    @Test
    fun testGetIntList() {
        val payload = ExperimentPayload()
        payload.put("int-key", listOf(1, 2))
        assertEquals(listOf(1, 2), payload.getIntList("int-key"))
    }

    @Test(expected = ClassCastException::class)
    fun testGetIntListInvalidType() {
        val payload = ExperimentPayload()
        payload.put("int-key", "other-value")
        payload.getIntList("int-key")
    }

    @Test
    fun testGetLongList() {
        val payload = ExperimentPayload()
        payload.put("long-key", listOf(1L, 2L))
        assertEquals(listOf(1L, 2L), payload.getLongList("long-key"))
    }

    @Test(expected = ClassCastException::class)
    fun testGetLongListInvalidType() {
        val payload = ExperimentPayload()
        payload.put("long-key", "other-value")
        payload.getLongList("long-key")
    }

    @Test
    fun testGetDoubleList() {
        val payload = ExperimentPayload()
        payload.put("double-key", listOf(1, 2.5))
        assertEquals(listOf(1, 2.5), payload.getDoubleList("double-key"))
    }

    @Test(expected = ClassCastException::class)
    fun testGetDoubleListInvalidType() {
        val payload = ExperimentPayload()
        payload.put("double-key", "other-value")
        payload.getDoubleList("double-key")
    }

    @Test
    fun testGetStringList() {
        val payload = ExperimentPayload()
        payload.put("string-key", listOf("first", "second"))
        assertEquals(listOf("first", "second"), payload.getStringList("string-key"))
    }

    @Test(expected = ClassCastException::class)
    fun testGetStringListInvalidType() {
        val payload = ExperimentPayload()
        payload.put("string-key", "other-value")
        payload.getStringList("string-key")
    }
}