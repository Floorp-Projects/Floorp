/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.org.json

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class JSONArrayTest {

    private lateinit var testData2Elements: JSONArray

    @Before
    fun setUp() {
        testData2Elements = JSONArray().apply {
            put(JSONObject("""{"a": 1}"""))
            put(JSONObject("""{"b": 2}"""))
        }
    }

    @Test
    fun itCanBeIterated() {
        val array = JSONArray("[1, 2, 3]")

        val sum = array.asSequence()
            .map { it as Int }
            .sum()

        assertEquals(6, sum)
    }

    @Test
    fun toListNull() {
        val jsonArray: JSONArray? = null
        val list = jsonArray.toList<Any>()
        assertEquals(0, list.size)
    }

    @Test
    fun toListEmpty() {
        val jsonArray = JSONArray()
        val list = jsonArray.toList<Any>()
        assertEquals(0, list.size)
    }

    @Test
    fun toListNotEmpty() {
        val jsonArray = JSONArray()
        jsonArray.put("value")
        jsonArray.put("another-value")
        val list = jsonArray.toList<String>()
        assertEquals(2, list.size)
        assertTrue(list.contains("value"))
        assertTrue(list.contains("another-value"))
    }

    @Test
    fun `WHEN mapNotNull on an empty jsonArray THEN an empty list is returned`() {
        assertEquals(emptyList<Int>(), JSONArray().mapNotNull(JSONArray::getJSONObject) { 1 })
    }

    @Test
    fun `WHEN mapNotNull getFromArray throws a JSONException THEN that item is ignored`() {
        val expected = listOf("a", "b")
        testData2Elements.put(404)
        val actual = testData2Elements.mapNotNull(JSONArray::getJSONObject) { it.keys().asSequence().first() }
        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN mapNotNull transform throws a JSONException THEN that item is ignored`() {
        val actual = testData2Elements.mapNotNull(JSONArray::getJSONObject) {
            it.get("b") // key not found for first item: throws an exception.
        }
        assertEquals(1, actual.size)
        assertEquals(2, actual[0])
    }

    @Test
    fun `WHEN mapNotNull getFromArray uses casted classes THEN data is mapped`() {
        val expected = listOf(JSONArrayTest())
        val input = JSONArray().apply { put(expected[0]) }
        val actual = input.mapNotNull(getFromArray = { i -> get(i) as JSONArrayTest }) { it }
        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN mapNotNull on an array of Int THEN nulls are removed and data is mapped`() {
        val expected = listOf(2, 4, 6, 8, 10)

        // Convert expected to input: [2, null, 4, null, ...]
        val input = JSONArray()
        expected.forEach {
            input.put(it)
            input.put(null)
        }
        val actual = input.mapNotNull(JSONArray::getInt) { it }

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN mapNotNull on an array of JSONObject THEN nulls are removed and data is mapped`() {
        val expected = listOf(
            "a" to 1,
            "b" to 2,
            "c" to 3,
        )

        // Convert expected to input: [JSONObject("a" to 1), null, JSONObject("b" to 2), null, ...]
        val input = JSONArray()
        expected.forEach {
            val obj = JSONObject().apply { put(it.first, it.second) }
            input.put(obj)
            input.put(null)
        }

        val actual = input.mapNotNull(JSONArray::getJSONObject) {
            val keys = it.keys().asSequence().toList()
            assertEquals(it.toString(), 1, keys.size)

            val key = keys.first()
            val value = it.get(key) as Int
            Pair(key, value)
        }

        assertEquals(expected, actual)
    }
}
