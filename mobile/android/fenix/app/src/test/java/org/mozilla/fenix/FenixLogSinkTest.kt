/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.base.log.sink.AndroidLogSink
import org.junit.Before
import org.junit.Test

class FenixLogSinkTest {

    private lateinit var androidLogSink: AndroidLogSink

    @Before
    fun setup() {
        androidLogSink = spyk(AndroidLogSink())
    }

    @Test
    fun `GIVEN we're in a release build WHEN we log debug statements THEN logs should not be forwarded`() {
        val logSink = FenixLogSink(false, androidLogSink)
        logSink.log(
            mozilla.components.support.base.log.Log.Priority.DEBUG,
            "test",
            message = "test",
        )
        verify(exactly = 0) { androidLogSink.log(any(), any(), any(), any()) }
    }

    @Test
    fun `GIVEN we're in a release build WHEN we log error statements THEN logs should be forwarded`() {
        val logSink = FenixLogSink(false, androidLogSink)
        logSink.log(
            mozilla.components.support.base.log.Log.Priority.ERROR,
            "test",
            message = "test",
        )

        verify(exactly = 1) {
            androidLogSink.log(
                mozilla.components.support.base.log.Log.Priority.ERROR,
                "test",
                message = "test",
            )
        }
    }

    @Test
    fun `GIVEN we're in a release build WHEN we log warn statements THEN logs should be forwarded`() {
        val logSink = FenixLogSink(false, androidLogSink)
        logSink.log(
            mozilla.components.support.base.log.Log.Priority.WARN,
            "test",
            message = "test",
        )
        verify(exactly = 1) {
            androidLogSink.log(
                mozilla.components.support.base.log.Log.Priority.WARN,
                "test",
                message = "test",
            )
        }
    }

    @Test
    fun `GIVEN we're in a release build WHEN we log info statements THEN logs should be forwarded`() {
        val logSink = FenixLogSink(false, androidLogSink)
        logSink.log(
            mozilla.components.support.base.log.Log.Priority.INFO,
            "test",
            message = "test",
        )
        verify(exactly = 1) {
            androidLogSink.log(
                mozilla.components.support.base.log.Log.Priority.INFO,
                "test",
                message = "test",
            )
        }
    }

    @Test
    fun `GIVEN we're in a debug build WHEN we log debug statements THEN logs should be forwarded`() {
        val logSink = FenixLogSink(true, androidLogSink)
        logSink.log(
            mozilla.components.support.base.log.Log.Priority.DEBUG,
            "test",
            message = "test",
        )

        verify(exactly = 1) {
            androidLogSink.log(
                mozilla.components.support.base.log.Log.Priority.DEBUG,
                "test",
                message = "test",
            )
        }
    }
}
