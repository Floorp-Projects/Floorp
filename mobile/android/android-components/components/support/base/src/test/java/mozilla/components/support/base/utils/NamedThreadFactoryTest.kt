/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.utils

import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.verify

class NamedThreadFactoryTest {

    private lateinit var runnable: Runnable

    @Before
    fun setUp() {
        runnable = mock()
    }

    @Test
    fun `WHEN creating a name thread THEN the names match the expected name including incrementing the thread number`() {
        val expectedPrefix = "A-Name"
        val factory = NamedThreadFactory(expectedPrefix)

        val thread1 = factory.newThread(runnable)
        assertEquals("$expectedPrefix-thread-1", thread1.name)

        val thread2 = factory.newThread(runnable)
        assertEquals("$expectedPrefix-thread-2", thread2.name)

        // Sanity check that we don't need to clean these threads up
        // (because these threads are not started).
        assertFalse(thread1.isAlive)
        assertFalse(thread2.isAlive)
    }

    @Test
    fun `WHEN creating a new thread THEN the given runnable is used`() {
        val factory = NamedThreadFactory("whatever")
        val thread = factory.newThread(runnable)

        thread.run()

        verify(runnable).run()

        // Sanity check that we don't need to clean up (because these threads are never started).
        assertFalse(thread.isAlive)
    }
}
