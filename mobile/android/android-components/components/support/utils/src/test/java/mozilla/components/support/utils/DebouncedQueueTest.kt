/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceTimeBy
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
class DebouncedQueueTest {
    private val coroutineScope = TestScope()

    @Test
    fun `WHEN multiple functions are queued at the same time THEN only the last one gets called`() = coroutineScope.runTest {
        val firstCall = mock<() -> Unit>()
        val secondCall = mock<() -> Unit>()
        val thirdCall = mock<() -> Unit>()

        val delayMillis = 300L
        val delayOffset = 1L

        val queue = DebouncedQueue(
            scope = coroutineScope,
            delayMillis = delayMillis,
        )

        queue.enqueue(firstCall)
        queue.enqueue(secondCall)
        queue.enqueue(thirdCall)

        advanceTimeBy(delayMillis + delayOffset)
        verify(firstCall, never()).invoke()
        verify(secondCall, never()).invoke()
        verify(thirdCall).invoke()
    }

    @Test
    fun `WHEN a delay is set THEN no functions are called before the delay expires`() = coroutineScope.runTest {
        val firstCall = mock<() -> Unit>()
        val secondCall = mock<() -> Unit>()
        val thirdCall = mock<() -> Unit>()

        val delayMillis = 300L
        val delayOffset = 1L

        val queue = DebouncedQueue(
            scope = coroutineScope,
            delayMillis = delayMillis,
        )

        queue.enqueue(firstCall)
        queue.enqueue(secondCall)
        queue.enqueue(thirdCall)

        advanceTimeBy(delayMillis - delayOffset)
        verify(firstCall, never()).invoke()
        verify(secondCall, never()).invoke()
        verify(thirdCall, never()).invoke()

        advanceTimeBy(delayOffset * 2)
        verify(firstCall, never()).invoke()
        verify(secondCall, never()).invoke()
        verify(thirdCall).invoke()
    }
}
