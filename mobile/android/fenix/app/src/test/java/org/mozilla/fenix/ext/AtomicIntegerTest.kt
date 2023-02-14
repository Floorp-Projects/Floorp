/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import java.util.concurrent.atomic.AtomicInteger

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

class AtomicIntegerTest {

    @Test
    fun `Safely increment an AtomicInteger from different coroutines`() {
        val integer = AtomicInteger(0)
        runBlocking {
            for (i in 1..2) {
                launch(Dispatchers.Default) {
                    integer.getAndIncrementNoOverflow()
                }
            }
        }

        assertEquals(integer.get(), 2)
    }

    @Test
    fun `Incrementing the AtomicInteger should not overflow`() {
        val integer = AtomicInteger(Integer.MAX_VALUE)
        integer.getAndIncrementNoOverflow()
        assertEquals(integer.get(), Integer.MAX_VALUE)
    }
}
