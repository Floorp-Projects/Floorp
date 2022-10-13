/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.org.json

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class JSONObjectTest {

    @Test
    fun sortKeys() {
        val jsonObject = JSONObject()
        jsonObject.put("second-key", "second-value")
        jsonObject.put(
            "third-key",
            JSONArray().apply {
                put(1)
                put(2)
                put(3)
            },
        )
        jsonObject.put(
            "first-key",
            JSONObject().apply {
                put("one-key", "one-value")
                put("a-key", "a-value")
                put("second-key", "second")
            },
        )
        assertEquals("""{"first-key":{"a-key":"a-value","one-key":"one-value","second-key":"second"},"second-key":"second-value","third-key":[1,2,3]}""", jsonObject.sortKeys().toString())
    }

    @Test
    fun putIfNotNull() {
        val jsonObject = JSONObject()
        assertEquals(0, jsonObject.length())
        jsonObject.putIfNotNull("key", null)
        assertEquals(0, jsonObject.length())
        jsonObject.putIfNotNull("key", "value")
        assertEquals(1, jsonObject.length())
        assertEquals("value", jsonObject["key"])
    }

    @Test
    fun tryGetNull() {
        val jsonObject = JSONObject("""{"key":null}""")
        assertNull(jsonObject.tryGet("key"))
        assertNull(jsonObject.tryGet("another-key"))
    }

    @Test
    fun tryGetNotNull() {
        val jsonObject = JSONObject("""{"key":"value"}""")
        assertEquals("value", jsonObject.tryGet("key"))
    }

    @Test
    fun tryGetStringNull() {
        val jsonObject = JSONObject("""{"key":null}""")
        assertNull(jsonObject.tryGetString("key"))
        assertNull(jsonObject.tryGetString("another-key"))
    }

    @Test
    fun tryGetStringNotNull() {
        val jsonObject = JSONObject("""{"key":"value"}""")
        assertEquals("value", jsonObject.tryGetString("key"))
    }

    @Test
    fun tryGetLongNull() {
        val jsonObject = JSONObject("""{"key":null}""")
        assertNull(jsonObject.tryGetLong("key"))
        assertNull(jsonObject.tryGetLong("another-key"))
    }

    @Test
    fun tryGetLongNotNull() {
        val jsonObject = JSONObject("""{"key":218728173837192717}""")
        assertEquals(218728173837192717, jsonObject.tryGetLong("key"))
    }

    @Test
    fun tryGetIntNull() {
        val jsonObject = JSONObject("""{"key":null}""")
        assertNull(jsonObject.tryGetInt("key"))
        assertNull(jsonObject.tryGetInt("another-key"))
    }

    @Test
    fun tryGetIntNotNull() {
        val jsonObject = JSONObject("""{"key":3}""")
        assertEquals(3, jsonObject.tryGetInt("key"))
    }

    @Test
    fun mergeWith() {
        val merged = JSONObject(
            mapOf(
                "toKeep" to 3,
                "toOverride" to "OHNOZ",
            ),
        )

        merged.mergeWith(
            JSONObject(
                mapOf(
                    "newKey" to 5,
                    "toOverride" to "YAY",
                ),
            ),
        )

        val expectedObject = JSONObject(
            mapOf(
                "toKeep" to 3,
                "toOverride" to "YAY",
                "newKey" to 5,
            ),
        )
        assertEquals(expectedObject.toString(), merged.toString())
    }
}
