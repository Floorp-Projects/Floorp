/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.ext

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class JSONKtTest {

    @Test
    fun `WHEN flatMapObj on an empty jsonArray THEN an empty list is returned`() {
        assertEquals(emptyList<Int>(), JSONArray().mapObjNotNull { 1 })
    }

    @Test
    fun `WHEN flatMapObj transform throws a JSONException THEN that item is ignored`() {
        val input = JSONArray().apply {
            put(JSONObject("""{"a": 1}"""))
            put(JSONObject("""{"b": 2}"""))
        }

        val actual = input.mapObjNotNull {
            it.get("b") // key not found for first item: throws an exception.
        }
        assertEquals(1, actual.size)
        assertEquals(2, actual[0])
    }

    @Test
    fun `WHEN flatMapObj on a list THEN nulls are removed and data is mapped`() {
        val expected = listOf(
            "a" to 1,
            "b" to 2,
            "c" to 3
        )

        // Convert expected input: [JSONObject("a" to 1), null, JSONObject("b" to 2), null, ...]
        val input = JSONArray()
        expected.forEach {
            val obj = JSONObject().apply { put(it.first, it.second) }
            input.put(obj)
            input.put(null)
        }

        val actual = input.mapObjNotNull {
            val keys = it.keys().asSequence().toList()
            assertEquals(it.toString(), 1, keys.size)

            val key = keys.first()
            val value = it.get(key) as Int
            Pair(key, value)
        }

        assertEquals(expected, actual)
    }
}
