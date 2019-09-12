package org.mozilla.fenix.ext

import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.mozilla.fenix.TestApplication
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import android.util.Log

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)

class LogTest {

    @Test
    fun `Test log debug function`() {
        mockkStatic(Log::class)
        logDebug("hi", "hi")
        verify { (Log.d("hi", "hi")) }
    }

    @Test
    fun `Test log warn function with tag and message args`() {
        mockkStatic(Log::class)
        logWarn("hi", "hi")
        verify { (Log.w("hi", "hi")) }
    }

    @Test
    fun `Test log warn function with tag, message, and exception args`() {
        mockkStatic(Log::class)
        val mockThrowable: Throwable = mockk(relaxed = true)
        logWarn("hi", "hi", mockThrowable)
        verify { (Log.w("hi", "hi", mockThrowable)) }
    }

    @Test
    fun `Test log error function with tag, message, and exception args`() {
        mockkStatic(Log::class)
        val mockThrowable: Throwable = mockk(relaxed = true)
        logErr("hi", "hi", mockThrowable)
        verify { (Log.e("hi", "hi", mockThrowable)) }
    }
}
