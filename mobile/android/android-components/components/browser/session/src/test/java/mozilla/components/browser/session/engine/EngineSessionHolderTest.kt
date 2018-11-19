/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine

import mozilla.components.concept.engine.EngineSession
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.mock
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class EngineSessionHolderTest {

    @Test
    fun `engine session holder changes are visible across threads`() {
        val engineSessionHolder = EngineSessionHolder()
        val countDownLatch = CountDownLatch(1)

        val executor = Executors.newScheduledThreadPool(2)

        executor.submit {
            engineSessionHolder.engineObserver = mock(EngineObserver::class.java)
            engineSessionHolder.engineSession = mock(EngineSession::class.java)
        }

        executor.submit {
            while (engineSessionHolder.engineObserver == null || engineSessionHolder.engineSession == null) { }
            countDownLatch.countDown()
        }

        // Setting a timeout in case this test fails in the future. As long as
        // the engine session holder fields are volatile, await will return
        // true immediately, otherwise false after the timeout expired.
        assertTrue(countDownLatch.await(10, TimeUnit.SECONDS))
    }
}