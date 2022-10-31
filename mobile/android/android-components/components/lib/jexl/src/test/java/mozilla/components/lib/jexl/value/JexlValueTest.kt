/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.value

import mozilla.components.lib.jexl.Jexl
import mozilla.components.lib.jexl.JexlException
import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test
import kotlin.reflect.KClass

class JexlValueTest {
    @Test
    fun `double arithmetic`() {
        "2.0 + 1".evaluatesTo(3.0)
        "2.0 + 4.0".evaluatesTo(6.0)
        "2.0 + 'a'".evaluatesTo("2.0a")
        "2.0 + true".evaluatesTo(3.0)
        "2.0 + false".evaluatesTo(2.0)
        "2.0 + {}".evaluationThrows()

        "2.0 * 2".evaluatesTo(4.0)
        "3.0 * 3.0".evaluatesTo(9.0)
        "2.0 * true".evaluatesTo(2.0)
        "2.0 * false".evaluatesTo(0.0)
        "2.0 * a".evaluationThrows()

        "4.0 / 2".evaluatesTo(2.0)
        "6.0 / 3.0".evaluatesTo(2.0)
        "2.0 / 'a'".evaluationThrows()

        "2.0 == 2.0".evaluatesTo(true)
        "2.0 == 2".evaluatesTo(true)
        "2.1 == 2.0".evaluatesTo(false)

        "2.0 > 1.0".evaluatesTo(true)
        "2.0 < 4.0".evaluatesTo(true)
        "1.0 > 2.0".evaluatesTo(false)
        "5.0 < 2.0".evaluatesTo(false)
        "1.0 < 1.0".evaluatesTo(false)
        "1.0 > 1.0".evaluatesTo(false)
    }

    @Test
    fun `integer arithmetic`() {
        "2 + 1".evaluatesTo(3)
        "2 + 4.0".evaluatesTo(6.0)
        "2 + 'a'".evaluatesTo("2a")
        "2 + true".evaluatesTo(3)
        "2 + false".evaluatesTo(2)
        "2 + {}".evaluationThrows()

        "2 * 2".evaluatesTo(4)
        "3 * 3.0".evaluatesTo(9.0)
        "2 * true".evaluatesTo(2)
        "2 * false".evaluatesTo(0)
        "2 * a".evaluationThrows()

        "4 / 2".evaluatesTo(2)
        "6 / 3.0".evaluatesTo(2.0)
        "2 / 'a'".evaluationThrows()

        "2 == 2.0".evaluatesTo(true)
        "2 == 2".evaluatesTo(true)

        "2 > 1".evaluatesTo(true)
        "2 < 4".evaluatesTo(true)
        "1 > 2".evaluatesTo(false)
        "5 < 2".evaluatesTo(false)
        "1 < 1".evaluatesTo(false)
        "1 > 1".evaluatesTo(false)
    }

    @Test
    fun `boolean arithmetic`() {
        "true / false".evaluationThrows()

        "true * 2".evaluatesTo(2)
        "false * 5".evaluatesTo(0)
        "true * 2.0".evaluatesTo(2.0)
        "false * 5.0".evaluatesTo(0.0)
        "true * {}".evaluationThrows()
        "true * []".evaluationThrows()
        "true * true".evaluatesTo(1)
        "false * false".evaluatesTo(0)
        "true * false".evaluatesTo(0)

        "true + 1".evaluatesTo(2)
        "false + 1".evaluatesTo(1)
        "true + 1.0".evaluatesTo(2.0)
        "false + 1.0".evaluatesTo(1.0)
        "true + 'hello'".evaluatesTo("truehello")
        "true + true".evaluatesTo(2)
        "true + false".evaluatesTo(1)
        "false + true".evaluatesTo(1)
        "false + false".evaluatesTo(0)
        "false + {}".evaluationThrows()
        "true + []".evaluationThrows()

        "true > false".evaluationThrows()
        "false < false".evaluationThrows()

        "true == true".evaluatesTo(true)
        "false == false".evaluatesTo(true)
        "true == false".evaluatesTo(false)
        "false == true".evaluatesTo(false)
        "true == 'hello'".evaluatesTo(false)
    }

    @Test
    fun `string arithmetic`() {
        "'a' / 2".evaluationThrows()
        "'a' * 2".evaluationThrows()

        "'hello' + 1".evaluatesTo("hello1")
        "'hello' + 2.0".evaluatesTo("hello2.0")
        "'hello' + ' ' + 'world'".evaluatesTo("hello world")

        "'hello' > 'world'".evaluationThrows()
        "'world' < 'hello'".evaluationThrows()

        "'hello' == true".evaluatesTo(false)
        "'' == true".evaluatesTo(false)
        "'hello' == false".evaluatesTo(false)
        "'' == false".evaluatesTo(false)
    }

    @Test
    fun `array arithmetic`() {
        "[1,2,3] == [1,2,3]".evaluatesTo(true)
        "[2,3,4] == [2,3,5]".evaluatesTo(false)
    }

    @Test
    fun `object arithmetic`() {
        "{} / 2".evaluationThrows()
        "{} * 5".evaluationThrows()
        "{} + 'hello'".evaluationThrows()
        "{} > 9".evaluationThrows()

        "{} == {}".evaluatesTo(true)
        "{agent:'Archer'} == { agent: 'Archer' }".evaluatesTo(true)
        "{a: 1, b: 2} == {b: 2, a: 1}".evaluatesTo(true)
        "{} == 2".evaluatesTo(false)
    }
}

private fun String.evaluatesTo(expectedResult: Any, unpacked: Boolean = true) {
    val jexl = Jexl()
    val actualResult = jexl.evaluate(this)

    assertEquals(expectedResult, if (unpacked) actualResult.value else actualResult)
}

private fun String.evaluationThrows() {
    evaluationThrows(JexlException::class)
}

private inline fun <reified T : Throwable> String.evaluationThrows(clazz: KClass<T>?) {
    try {
        evaluatesTo(Any())
        fail("Expected exception to be thrown: $clazz")
    } catch (e: Throwable) {
        if (e !is T) {
            throw e
        }
    }
}
