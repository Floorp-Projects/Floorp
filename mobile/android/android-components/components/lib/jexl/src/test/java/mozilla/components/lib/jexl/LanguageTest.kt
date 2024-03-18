/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl

import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test
import kotlin.reflect.KClass

/**
 * Additional test cases that test various JEXL expressions to get a high test coverage for the lexer, parser and
 * evaluator.
 */
class LanguageTest {
    @Test
    fun `Multiple dots in numeric expression should throw exception`() {
        "27.42.21".evaluationThrows()
    }

    @Test
    fun `Negating a literal should throw exception`() {
        "-employees".evaluationThrows()
    }

    @Test
    fun `Using non grammar character should throw exception`() {
        "§".evaluationThrows()
    }

    private fun String.evaluatesTo(expectedResult: Any) {
        val jexl = Jexl()
        val actualResult = jexl.evaluate(this)

        assertEquals(expectedResult, actualResult)
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
}
