/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.ext

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.lang.RuntimeException

@RunWith(AndroidJUnit4::class)
class ThrowableTest {
    @Test
    fun `throwable stack trace to string is limited to max length`() {
        val throwable = RuntimeException("TEST ONLY")

        assertEquals(throwable.getStacktraceAsString(1).length, 1)
        assertEquals(throwable.getStacktraceAsString(10).length, 10)
    }

    @Test
    fun `throwable stack trace to string works correctly`() {
        val throwable = RuntimeException("TEST ONLY")

        assert(throwable.getStacktraceAsString().contains("mozilla.components.support.base.ext.ThrowableTest.throwable stack trace to string works correctly(ThrowableTest.kt:"))
    }

    @Test
    fun `throwable stack trace to json string works correctly`() {
        val throwable = RuntimeException("TEST ONLY")

        val json = JSONObject(throwable.getStacktraceAsJsonString())
        val exception = json.getJSONObject("exception")
        val values = exception.getJSONArray("values")
        assertEquals(values.length(), 1)
        val stacktrace = values.getJSONObject(0).getJSONObject("stacktrace")
        assertEquals(stacktrace.getString("type"), "RuntimeException")
        assertEquals(stacktrace.getString("module"), "java.lang")
        assertEquals(stacktrace.getString("value"), "TEST ONLY")
        val frames = stacktrace.getJSONArray("frames")
        val firstFrame = frames.getJSONObject(0)
        assert(firstFrame.getBoolean("in_app"))
        assertEquals(firstFrame.getString("filename"), "ThrowableTest.kt")
        assertEquals(firstFrame.getString("module"), "mozilla.components.support.base.ext.ThrowableTest")
        assertEquals(firstFrame.getString("function"), "throwable stack trace to json string works correctly")
    }

    @Test
    fun `throwable stack trace to json string max frames works correctly`() {
        val throwable = RuntimeException("TEST ONLY")

        var json = JSONObject(throwable.getStacktraceAsJsonString(1))
        var frames = json.getJSONObject("exception").getJSONArray("values").getJSONObject(0)
            .getJSONObject("stacktrace").getJSONArray("frames")
        assertEquals(frames.length(), 1)

        json = JSONObject(throwable.getStacktraceAsJsonString(5))
        frames = json.getJSONObject("exception").getJSONArray("values").getJSONObject(0)
            .getJSONObject("stacktrace").getJSONArray("frames")
        assertEquals(frames.length(), 5)

        json = JSONObject(throwable.getStacktraceAsJsonString(10))
        frames = json.getJSONObject("exception").getJSONArray("values").getJSONObject(0)
            .getJSONObject("stacktrace").getJSONArray("frames")
        assertEquals(frames.length(), 10)
    }
}
