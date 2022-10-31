/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlinx.coroutines.flow

import kotlinx.coroutines.flow.flowOf
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Test

class FlowKtTest {

    data class StringState(val value: String)
    data class CharState(val value: Char)
    data class IntState(val value: Int)

    @Test
    fun `ifChanged operator`() = runTest {
        val originalFlow = flowOf("A", "B", "B", "C", "A", "A", "A", "D", "A")

        val items = originalFlow.ifChanged().toList()

        assertEquals(
            listOf("A", "B", "C", "A", "D", "A"),
            items,
        )
    }

    @Test
    fun `ifChanged operator with block`() = runTest {
        val originalFlow = flowOf("banana", "bus", "apple", "big", "coconut", "circle", "home")

        val items = originalFlow.ifChanged { item -> item[0] }.toList()

        assertEquals(
            listOf("banana", "apple", "big", "coconut", "home"),
            items,
        )
    }

    @Test
    fun `ifChanged operator uses structural equality`() = runTest {
        val originalFlow = flowOf(
            StringState("A"),
            StringState("B"),
            StringState("B"),
            StringState("C"),
            StringState("A"),
            StringState("A"),
            StringState("A"),
            StringState("D"),
            StringState("A"),
        )

        val items = originalFlow.ifChanged().toList()

        assertEquals(
            listOf(
                StringState("A"),
                StringState("B"),
                StringState("C"),
                StringState("A"),
                StringState("D"),
                StringState("A"),
            ),
            items,
        )
    }

    @Test
    fun `ifAnyChanged operator with block`() = runTest {
        val originalFlow = flowOf("banana", "bandanna", "bus", "apple", "big", "coconut", "circle", "home")

        val items = originalFlow.ifAnyChanged { item -> arrayOf(item[0], item[1]) }.toList()

        assertEquals(
            listOf("banana", "bus", "apple", "big", "coconut", "circle", "home"),
            items,
        )
    }

    @Test
    fun `ifAnyChanged operator uses structural equality`() = runTest {
        val originalFlow = flowOf("banana", "bandanna", "bus", "apple", "big", "coconut", "circle", "home")

        val items =
            originalFlow.ifAnyChanged {
                    item ->
                arrayOf(CharState(item[0]), CharState(item[1]))
            }.toList()

        assertEquals(
            listOf("banana", "bus", "apple", "big", "coconut", "circle", "home"),
            items,
        )
    }

    @Test
    fun `filterChanged operator`() = runTest {
        val intFlow = flowOf(listOf(0), listOf(0, 1), listOf(0, 1, 2, 3), listOf(4), listOf(5, 6, 7, 8))
        val identityItems = intFlow.filterChanged { item -> item }.toList()
        assertEquals(listOf(0, 1, 2, 3, 4, 5, 6, 7, 8), identityItems)

        val moduloFlow = flowOf(listOf(1), listOf(1, 2), listOf(3, 4, 5), listOf(3, 4))
        val moduloItems = moduloFlow.filterChanged { item -> item % 2 }.toList()
        assertEquals(listOf(1, 2, 3, 4, 5), moduloItems)

        // Here we simulate a non-pure transform function (a function with a side-effect), causing
        // the transformed values to be different for the same input.
        var counter = 0
        val sideEffectFlow = flowOf(listOf(0), listOf(0, 1), listOf(0, 1, 2, 3), listOf(4), listOf(5, 6, 7, 8))
        val sideEffectItems = sideEffectFlow.filterChanged { item -> item + counter++ }.toList()
        assertEquals(listOf(0, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8), sideEffectItems)
    }

    @Test
    fun `filterChanged operator check for structural equality`() = runTest {
        val intFlow = flowOf(
            listOf(IntState(0)),
            listOf(IntState(0), IntState(1)),
            listOf(IntState(0), IntState(1), IntState(2), IntState(3)),
            listOf(IntState(4)),
            listOf(IntState(5), IntState(6), IntState(7), IntState(8)),
        )

        val identityItems = intFlow.filterChanged { item -> item }.toList()
        assertEquals(
            listOf(
                IntState(0),
                IntState(1),
                IntState(2),
                IntState(3),
                IntState(4),
                IntState(5),
                IntState(6),
                IntState(7),
                IntState(8),
            ),
            identityItems,
        )
    }
}
