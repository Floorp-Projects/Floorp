/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.ext

import mozilla.components.lib.jexl.value.JexlArray
import mozilla.components.lib.jexl.value.JexlBoolean
import mozilla.components.lib.jexl.value.JexlDouble
import mozilla.components.lib.jexl.value.JexlInteger
import mozilla.components.lib.jexl.value.JexlString
import org.junit.Assert.assertEquals
import org.junit.Test
import java.lang.UnsupportedOperationException

class JexlExtensionsTest {
    @Test
    fun `Simple types`() {
        assertEquals("Hello", "Hello".toJexl().value)
        assertEquals(23, 23.toJexl().value)
        assertEquals(23.0, 23.0.toJexl().value, 0.00001)
        assertEquals(1.0, 1.0f.toJexl().value, 0.00001)
        assertEquals(0, 0.toJexl().value)
        assertEquals(true, true.toJexl().value)
        assertEquals(false, false.toJexl().value)
    }

    @Test
    fun `Arrays`() {
        assertEquals(
            JexlArray(JexlInteger(1), JexlInteger(2), JexlInteger(3)),
            listOf(1, 2, 3).toJexlArray(),
        )

        assertEquals(
            JexlArray(),
            emptyList<String>().toJexlArray(),
        )

        assertEquals(
            JexlArray(JexlString("Hello"), JexlString("World")),
            listOf("Hello", "World").toJexlArray(),
        )

        assertEquals(
            JexlArray(JexlDouble(1.0), JexlDouble(23.0)),
            listOf(1.0, 23.0).toJexlArray(),
        )

        assertEquals(
            JexlArray(JexlDouble(52.0), JexlDouble(-2.0)),
            listOf(52.0f, -2f).toJexlArray(),
        )

        assertEquals(
            JexlArray(JexlBoolean(false), JexlBoolean(true)),
            listOf(false, true).toJexlArray(),
        )
    }

    @Test(expected = UnsupportedOperationException::class)
    fun `Unsupported array type`() {
        listOf(Pair(1, 2), Pair(3, 4)).toJexlArray()
    }
}
