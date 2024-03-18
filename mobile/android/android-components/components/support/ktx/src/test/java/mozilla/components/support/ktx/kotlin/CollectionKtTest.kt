/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import org.junit.Assert
import org.junit.Test

class CollectionKtTest {

    @Test
    fun `cross product of each element is called exactly`() {
        val numbers = listOf(1, 2, 3)
        val letters = listOf('a', 'b', 'c')
        var counter = 0
        numbers.crossProduct(letters) { _, _ ->
            counter++
        }

        Assert.assertEquals(numbers.size * letters.size, counter)
    }

    @Test
    fun `cross product of each element is of the same type`() {
        val numbers = listOf(1, 2, 3)
        val letters = listOf('a', 'b', 'c')
        numbers.crossProduct(letters) { number, letter ->
            Assert.assertEquals(Int::class, number::class)
            Assert.assertEquals(Char::class, letter::class)
        }
    }

    @Test
    fun `cross product of each pair is in order`() {
        val numbers = listOf(1, 2, 3)
        val letters = listOf('a', 'b', 'c')
        val assertions = arrayOf(
            1 to 'a',
            1 to 'b',
            1 to 'c',
            2 to 'a',
            2 to 'b',
            2 to 'c',
            3 to 'a',
            3 to 'b',
            3 to 'c',
        )
        var position = 0
        numbers.crossProduct(letters) { number, letter ->
            Assert.assertEquals(assertions[position].first, number)
            Assert.assertEquals(assertions[position].second, letter)
            position++
        }
    }

    @Suppress("USELESS_IS_CHECK")
    @Test
    fun `cross product result is list of return type`() {
        val numbers = listOf(1, 2, 3)
        val letters = listOf('a', 'b', 'c')
        val result = numbers.crossProduct(letters) { number, letter ->
            number to letter
        }
        Assert.assertTrue(result is List)
        Assert.assertEquals(Pair::class, result[0]::class)
        Assert.assertEquals(Int::class, result[0].first::class)
        Assert.assertEquals(Char::class, result[0].second::class)
    }
}
