/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlinx.coroutines.flow

import kotlinx.coroutines.flow.flowOf
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Test

class FlowKtTest {
    @Test
    fun `ifChanged operator`() {
        val originalFlow = flowOf("A", "B", "B", "C", "A", "A", "A", "D", "A")

        val items = runBlocking { originalFlow.ifChanged().toList() }

        assertEquals(
            listOf("A", "B", "C", "A", "D", "A"),
            items)
    }

    @Test
    fun `ifChanged operator with block`() {
        val originalFlow = flowOf("banana", "bus", "apple", "big", "coconut", "circle", "home")

        val items = runBlocking {
            originalFlow.ifChanged { item -> item[0] }.toList()
        }

        assertEquals(
            listOf("banana", "apple", "big", "coconut", "home"),
            items
        )
    }
}
