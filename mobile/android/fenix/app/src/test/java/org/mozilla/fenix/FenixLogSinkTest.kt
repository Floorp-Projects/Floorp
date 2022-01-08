/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.util.Log
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import org.junit.After
import org.junit.Before
import org.junit.Test

class FenixLogSinkTest {

    @Before
    fun setup() {
        mockkStatic(Log::class)
    }

    @After
    fun teardown() {
        unmockkStatic(Log::class)
    }

    @Test
    fun `GIVEN we're in a release build WHEN we log debug statements THEN logs should not be forwarded`() {
        val logSink = FenixLogSink(false)
        logSink.log(
            mozilla.components.support.base.log.Log.Priority.DEBUG,
            "test",
            message = "test"
        )
        verify(exactly = 0) { Log.println(Log.DEBUG, "test", "test") }
    }

    @Test
    fun `GIVEN we're in a release build WHEN we log error statements THEN logs should be forwarded`() {
        val logSink = FenixLogSink(false)
        logSink.log(
            mozilla.components.support.base.log.Log.Priority.ERROR,
            "test",
            message = "test"
        )
        verify(exactly = 1) { Log.println(Log.ERROR, "test", "test") }
    }

    @Test
    fun `GIVEN we're in a release build WHEN we log warn statements THEN logs should be forwarded`() {
        val logSink = FenixLogSink(false)
        logSink.log(
            mozilla.components.support.base.log.Log.Priority.WARN,
            "test",
            message = "test"
        )
        verify(exactly = 1) { Log.println(Log.WARN, "test", "test") }
    }

    @Test
    fun `GIVEN we're in a release build WHEN we log info statements THEN logs should be forwarded`() {
        val logSink = FenixLogSink(false)
        logSink.log(
            mozilla.components.support.base.log.Log.Priority.INFO,
            "test",
            message = "test"
        )
        verify(exactly = 1) { Log.println(Log.INFO, "test", "test") }
    }

    @Test
    fun `GIVEN we're in a debug build WHEN we log debug statements THEN logs should be forwarded`() {
        val logSink = FenixLogSink(true)
        logSink.log(
            mozilla.components.support.base.log.Log.Priority.DEBUG,
            "test",
            message = "test"
        )
        verify(exactly = 1) { Log.println(Log.DEBUG, "test", "test") }
    }
}
