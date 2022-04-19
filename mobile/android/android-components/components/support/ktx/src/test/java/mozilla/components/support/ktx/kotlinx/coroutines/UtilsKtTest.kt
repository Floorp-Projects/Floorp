/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlinx.coroutines

import kotlinx.coroutines.delay
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test

class UtilsKtTest {

    @Test
    fun throttle() = runTest(UnconfinedTestDispatcher()) {
        val skipTime = 300L
        var value = 0
        val throttleBlock = throttleLatest<Int>(skipTime, coroutineScope = this) {
            value = it
        }

        for (n in 1..300) {
            throttleBlock(n)
        }
        assertNotEquals(300, value)

        value = 0

        for (n in 1..300) {
            delay(skipTime)
            throttleBlock(n)
        }

        assertEquals(300, value)
    }
}
