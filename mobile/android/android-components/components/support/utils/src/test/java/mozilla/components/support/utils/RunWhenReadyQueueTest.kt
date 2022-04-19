/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.inOrder
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
class RunWhenReadyQueueTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Test
    fun `task should not run until ready is called`() = runTestOnMain {
        val task = mock<() -> Unit>()
        val queue = RunWhenReadyQueue(scope)

        verify(task, never()).invoke()
        assertFalse(queue.isReady())

        queue.runIfReadyOrQueue(task)
        queue.ready()

        verify(task).invoke()
        assertTrue(queue.isReady())
    }

    @Test
    fun `task should run if ready was called`() = runTestOnMain {
        val task = mock<() -> Unit>()
        val queue = RunWhenReadyQueue(scope)
        queue.ready()

        verify(task, never()).invoke()

        queue.runIfReadyOrQueue(task)

        verify(task).invoke()
    }

    @Test
    fun `tasks should run in the order they were queued`() = runTestOnMain {
        val task1 = mock<() -> Unit>()
        val task2 = mock<() -> Unit>()
        val task3 = mock<() -> Unit>()
        val queue = RunWhenReadyQueue(scope)

        queue.runIfReadyOrQueue(task1)
        queue.runIfReadyOrQueue(task2)
        queue.runIfReadyOrQueue(task3)
        queue.ready()

        val inOrder = inOrder(task1, task2, task3)
        inOrder.verify(task1).invoke()
        inOrder.verify(task2).invoke()
        inOrder.verify(task3).invoke()
    }
}
