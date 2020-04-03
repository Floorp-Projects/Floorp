/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

private const val STAT_CONTENTS = "32250 (a.fennec_aurora) S 831 831 0 0 -1 1077952832 670949 0 184936 0 15090 5387 0 0 20 0 119 0 166636813 9734365184 24664 18446744073709551615 1 1 0 0 0 0 4612 4097 1073792254 0 0 0 17 1 0 0 0 0 0 0 0 0 0 0 0 0 0"
private const val CLOCK_TICKS_PER_SECOND = 100L // actual value on the Pixel 2.

class StatTest {

    private lateinit var stat: StatTestImpl

    @Before
    fun setUp() {
        stat = StatTestImpl()
    }

    @Test
    fun `WHEN getting the process start time THEN the correct value is returned`() {
        val actual = stat.getProcessStartTimeTicks(pid = -1) // pid behavior is overridden.
        assertEquals(166636813, actual) // expected value calculated by hand.
    }

    @Test
    fun `WHEN converting ticks to nanos THEN the correct value is returned`() {
        val actual = stat.convertTicksToNanos(166_636_813)
        assertEquals(1_666_368_130_000_000.0, actual, 0.0) // expected value calculated by hand.
    }

    @Test
    fun `WHEN converting nanos to ticks THEN the correct value is returned`() {
        val actual = stat.convertNanosToTicks(1_666_368_135_432_102)
        assertEquals(166_636_813.5432102, actual, 0.0) // expected value calculated by hand.
    }
}

class StatTestImpl : Stat() {
    override fun getStatText(pid: Int): String = STAT_CONTENTS
    override val clockTicksPerSecond: Long get() = CLOCK_TICKS_PER_SECOND
}
