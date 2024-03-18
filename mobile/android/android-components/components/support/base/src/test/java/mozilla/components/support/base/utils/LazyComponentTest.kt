/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.utils

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class LazyComponentTest {

    private val initCount: Int get() = LazyComponent.initCount.get()

    @Before
    fun setUp() {
        LazyComponent.initCount.set(0)
    }

    @Test
    fun `WHEN accessing a lazy component THEN it returns the initializer value`() {
        val actual = LazyComponent { 4 }
        assertEquals(4, actual.get())
    }

    @Test
    fun `WHEN accessing a lazy component THEN the init count is incremented`() {
        assertEquals(0, initCount)

        val monitored = LazyComponent { 4 }
        monitored.get()

        assertEquals(1, initCount)
    }

    @Test
    fun `WHEN initializing a lazy component THEN the lazy value is not initialized`() {
        val component = LazyComponent { 4 }
        assertFalse(component.isInitialized())
    }

    @Test
    fun `WHEN initializing and getting a lazy component THEN the lazy value is initialized`() {
        val component = LazyComponent { 4 }
        component.get()
        assertTrue(component.isInitialized())
    }

    @Test // potentially flaky because it's testing a concurrency condition.
    fun `WHEN multiple threads try to initialize the same lazy component THEN only one component is initialized`() {
        var componentsInitialized = 0
        val component = LazyComponent {
            componentsInitialized += 1
            4
        }

        // coroutines add complex behavior - suspension, test APIs affecting true concurrency, etc. -
        // so we use traditional threads for a simpler implementation.
        val executorService = Executors.newFixedThreadPool(12)
        for (i in 1..100) {
            executorService.submit { component.get() }
        }
        executorService.shutdown()
        executorService.awaitTermination(1, TimeUnit.SECONDS)

        assertEquals(1, componentsInitialized)
    }
}
