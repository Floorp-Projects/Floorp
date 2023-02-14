/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class RunBlockingCounterTest {

    @Before
    fun setup() {
        RunBlockingCounter.count.set(0)
    }

    @Test
    fun `GIVEN we call our custom runBlocking method with counter THEN the latter should increase`() {
        assertEquals(0, RunBlockingCounter.count.get())
        runBlockingIncrement {}
        assertEquals(1, RunBlockingCounter.count.get())
    }
}
