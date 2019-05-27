/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class GeckoResultTest {

    @Test
    fun awaitWithResult() {
        val result = runBlocking {
            GeckoResult.fromValue(42).await()
        }
        assertEquals(42, result)
    }

    @Test(expected = IllegalStateException::class)
    fun awaitWithException() {
        runBlocking {
            GeckoResult.fromException<Unit>(IllegalStateException()).await()
        }
    }

    @Test
    fun fromResult() {
        runBlocking {
            val result = launchGeckoResult { 42 }

            result.then {
                assertEquals(42, it)
                GeckoResult.fromValue(null)
            }.await()
        }
    }

    @Test
    fun fromException() {
        runBlocking {
            val result = launchGeckoResult { throw IllegalStateException() }

            result.then({
                assertTrue("Invalid branch", false)
                GeckoResult.fromValue(null)
            }, {
                assertTrue(it is IllegalStateException)
                GeckoResult.fromValue(null)
            }).await()
        }
    }
}
