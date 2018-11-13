/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import kotlinx.coroutines.Dispatchers.Unconfined
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.consumeEach
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertArrayEquals
import org.junit.Test

class CoroutineScopeTest {

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    @Test
    fun `debounce 3 and 4 of 4 inputs`() {
        val list = listOf("t", "te", "tes", "test")
        val channel = Channel<String>()
        runBlocking(Unconfined) {
            launch(Unconfined) {
                val delay = listOf(10L, 20L, 40L, 90L)
                (0 until list.size).forEach {
                    delay(delay[it])
                    channel.send(list[it])
                }
                channel.close()
            }

            val strings = mutableListOf<String>()
            debounce(80, channel).consumeEach {
                strings += it
            }
            assertArrayEquals(strings.toTypedArray(), arrayOf("tes", "test"))
        }
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    @Test
    fun `debounce 1 and 4 of 4 inputs`() {
        val list = listOf("t", "te", "tes", "test")
        val channel = Channel<String>()
        runBlocking(Unconfined) {
            launch(Unconfined) {
                val delay = listOf(10L, 90L, 40L, 20L)
                (0 until list.size).forEach {
                    delay(delay[it])
                    channel.send(list[it])
                }
                channel.close()
            }

            val strings = mutableListOf<String>()
            debounce(80, channel).consumeEach {
                strings += it
            }
            assertArrayEquals(strings.toTypedArray(), arrayOf("t", "test"))
        }
    }
}
