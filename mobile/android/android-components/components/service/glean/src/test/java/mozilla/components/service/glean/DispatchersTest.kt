/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import kotlinx.coroutines.runBlocking
import org.junit.Test

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertSame

@Suppress("EXPERIMENTAL_API_USAGE")
class DispatchersTest {

    @Test
    fun `API scope runs off the main thread`() {
        val mainThread = Thread.currentThread()
        var threadCanary = false
        Dispatchers.API.setTestingMode(false)
        Dispatchers.API.setTaskQueueing(false)

        runBlocking {
            Dispatchers.API.launch {
                assertNotSame(mainThread, Thread.currentThread())
                // Use the canary bool to make sure this is getting called before
                // the test completes.
                assertEquals(false, threadCanary)
                threadCanary = true
            }!!.join()
        }

        Dispatchers.API.setTestingMode(true)
        assertEquals(true, threadCanary)
        assertSame(mainThread, Thread.currentThread())
    }

    @Test
    fun `launch() correctly adds tests to queue if queueTasks is true`() {
        var threadCanary = 0

        Dispatchers.API.setTestingMode(true)
        Dispatchers.API.setTaskQueueing(true)

        // Add 3 tasks to queue each one setting threadCanary to true to indicate if any task has ran
        repeat(3) {
            Dispatchers.API.launch {
                threadCanary += 1
            }
        }

        assertEquals("Task queue contains the correct number of tasks",
            3, Dispatchers.API.taskQueue.size)
        assertEquals("Tasks have not run while in queue", 0, threadCanary)

        // Now trigger execution to ensure the tasks fired
        Dispatchers.API.flushQueuedInitialTasks()

        assertEquals("Tasks have executed", 3, threadCanary)
        assertEquals("Task queue is cleared", 0, Dispatchers.API.taskQueue.size)
    }
}
