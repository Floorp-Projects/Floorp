/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl

import mozilla.components.lib.jexl.evaluator.JexlContext
import mozilla.components.lib.jexl.ext.toJexl
import mozilla.components.lib.jexl.ext.toJexlArray
import mozilla.components.lib.jexl.value.JexlArray
import mozilla.components.lib.jexl.value.JexlObject
import org.junit.Assert.assertEquals
import org.junit.Test

class JexlTest {

    @Test
    fun `Should evaluate expressions`() {
        val jexl = Jexl()

        val result = jexl.evaluate("75 > 42")
        assertEquals(true, result.value)
    }

    @Test
    fun `Should evaluate boolean expressions`() {
        val jexl = Jexl()

        val result = jexl.evaluateBooleanExpression("42 + 23 > 50", defaultValue = false)

        assertEquals(true, result)
    }

    @Test
    fun `Should apply transform`() {
        val jexl = Jexl()

        jexl.addTransform("split") { value, arguments ->
            value.toString().split(arguments.first().toString()).toJexlArray()
        }

        jexl.addTransform("lower") { value, _ ->
            value.toString().lowercase().toJexl()
        }

        jexl.addTransform("last") { value, _ ->
            (value as JexlArray).value.last()
        }

        assertEquals(
            "poovey".toJexl(),
            jexl.evaluate(""""Pam Poovey"|lower|split(' ')|last"""),
        )

        assertEquals(
            JexlArray("password".toJexl(), "guest".toJexl()),
            jexl.evaluate(""""password==guest"|split('=' + '=')"""),
        )
    }

    @Test
    fun `Should use context`() {
        val jexl = Jexl()

        val context = JexlContext(
            "employees" to JexlArray(
                JexlObject(
                    "first" to "Sterling".toJexl(),
                    "last" to "Archer".toJexl(),
                    "age" to 36.toJexl(),
                ),
                JexlObject(
                    "first" to "Malory".toJexl(),
                    "last" to "Archer".toJexl(),
                    "age" to 75.toJexl(),
                ),
                JexlObject(
                    "first" to "Malory".toJexl(),
                    "last" to "Archer".toJexl(),
                    "age" to 33.toJexl(),
                ),
            ),
        )

        assertEquals(
            JexlArray(
                JexlObject(
                    "first" to "Sterling".toJexl(),
                    "last" to "Archer".toJexl(),
                    "age" to 36.toJexl(),
                ),
                JexlObject(
                    "first" to "Malory".toJexl(),
                    "last" to "Archer".toJexl(),
                    "age" to 33.toJexl(),
                ),
            ),
            jexl.evaluate("employees[.age >= 30 && .age < 40]", context),
        )

        assertEquals(
            JexlArray(
                JexlObject(
                    "first" to "Malory".toJexl(),
                    "last" to "Archer".toJexl(),
                    "age" to 33.toJexl(),
                ),
            ),
            jexl.evaluate("employees[.age >= 30 && .age < 90][.age < 35]", context),
        )
    }
}
