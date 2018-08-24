/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.org.json

import org.json.JSONArray
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class JSONArrayTest {

    @Test
    fun testItCanBeIterated() {
        val array = JSONArray("[1, 2, 3]")

        val sum = array.asSequence()
                        .map { it as Int }
                        .sum()

        assertEquals(6, sum)
    }

    @Test
    fun testToListNull() {
        val jsonArray: JSONArray? = null
        val list = jsonArray.toList<Any>()
        assertEquals(0, list.size)
    }

    @Test
    fun testToListEmpty() {
        val jsonArray = JSONArray()
        val list = jsonArray.toList<Any>()
        assertEquals(0, list.size)
    }

    @Test
    fun testToListNotEmpty() {
        val jsonArray = JSONArray()
        jsonArray.put("value")
        jsonArray.put("another-value")
        val list = jsonArray.toList<String>()
        assertEquals(2, list.size)
        assertTrue(list.contains("value"))
        assertTrue(list.contains("another-value"))
    }
}
