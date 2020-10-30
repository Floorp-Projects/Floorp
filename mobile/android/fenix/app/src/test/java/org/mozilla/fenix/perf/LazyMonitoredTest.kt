/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class LazyMonitoredTest {

    private val componentInitCount get() = ComponentInitCount.count.get()

    @Before
    fun setUp() {
        ComponentInitCount.count.set(0)
    }

    @Test
    fun `WHEN using the convenience function THEN it returns a lazy monitored`() {
        val actual = lazyMonitored { }
        assertEquals(LazyMonitored::class, actual::class)
    }

    @Test
    fun `WHEN accessing a lazy monitored THEN it returns the initializer value`() {
        val actual by lazyMonitored { 4 }
        assertEquals(4, actual)
    }

    @Test
    fun `WHEN accessing a lazy monitored THEN the component init count is incremented`() {
        assertEquals(0, componentInitCount)

        val monitored by lazyMonitored { }
        // We must access the value to trigger init.
        @Suppress("UNUSED_EXPRESSION") monitored

        assertEquals(1, componentInitCount)
    }
}
