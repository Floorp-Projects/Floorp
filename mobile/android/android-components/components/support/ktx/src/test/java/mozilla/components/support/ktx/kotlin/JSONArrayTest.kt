/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import junit.framework.Assert.assertEquals
import org.json.JSONArray
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
}