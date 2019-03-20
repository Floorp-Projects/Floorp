/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import org.junit.Test

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertSame

class DispatchersTest {

    @Test
    fun `API scope runs off the main thread`() {
        val mainThread = Thread.currentThread()
        var threadCanary = false
        Dispatchers.API.testingMode = false

        runBlocking {
            @Suppress("EXPERIMENTAL_API_USAGE")
            Dispatchers.API.launch {
                assertNotSame(mainThread, Thread.currentThread())
                // Use the canary bool to make sure this is getting called before
                // the test completes.
                assertEquals(false, threadCanary)
                threadCanary = true
            }!!.join()
        }

        Dispatchers.API.testingMode = true
        assertEquals(true, threadCanary)
        assertSame(mainThread, Thread.currentThread())
    }
}
