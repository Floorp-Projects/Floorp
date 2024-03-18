/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.util

import android.util.JsonReader
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class JsonReaderKtTest {
    @Test
    fun `nextStringOrNull - string`() {
        val json = """{ "key": "value" }"""
        val reader = JsonReader(json.reader())

        reader.beginObject()
        reader.nextName()

        assertEquals("value", reader.nextStringOrNull())
    }

    @Test
    fun `nextStringOrNull - null`() {
        val json = """{ "key": null }"""
        val reader = JsonReader(json.reader())

        reader.beginObject()
        reader.nextName()

        assertNull(reader.nextStringOrNull())
    }

    @Test
    fun `nextBooleanOrNull - true`() {
        val json = """{ "key": true }"""
        val reader = JsonReader(json.reader())

        reader.beginObject()
        reader.nextName()

        assertTrue(reader.nextBooleanOrNull()!!)
    }

    @Test
    fun `nextBooleanOrNull - false`() {
        val json = """{ "key": false }"""
        val reader = JsonReader(json.reader())

        reader.beginObject()
        reader.nextName()

        assertFalse(reader.nextBooleanOrNull()!!)
    }

    @Test
    fun `nextBooleanOrNull - null`() {
        val json = """{ "key": null }"""
        val reader = JsonReader(json.reader())

        reader.beginObject()
        reader.nextName()

        assertNull(reader.nextBooleanOrNull())
    }
}
