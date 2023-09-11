package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.BuildConfig
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.IgnoreCrash
import org.mozilla.geckoview.test.util.UiThreadUtils

@RunWith(AndroidJUnit4::class)
@MediumTest
class GpuCrashTest : BaseSessionTest() {
    val client = TestCrashHandler.Client(InstrumentationRegistry.getInstrumentation().targetContext)

    @Before
    fun setup() {
        assertTrue(client.connect(sessionRule.env.defaultTimeoutMillis))
        client.setEvalNextCrashDump(GeckoRuntime.CRASHED_PROCESS_TYPE_BACKGROUND_CHILD, null)
    }

    @IgnoreCrash
    @Test
    fun crashGpu() {
        // We need the crash reporter for this test
        assumeTrue(BuildConfig.MOZ_CRASHREPORTER)

        // We need the GPU process for this test
        assumeTrue(sessionRule.usingGpuProcess())

        // Cause the GPU process to crash.
        sessionRule.crashGpuProcess()

        val evalResult = client.getEvalResult(sessionRule.env.defaultTimeoutMillis)
        assertTrue(evalResult.mMsg, evalResult.mResult)
    }

    @Test(expected = UiThreadUtils.TimeoutException::class)
    fun killGpuNoCrashReport() {
        // We need the crash reporter for this test
        assumeTrue(BuildConfig.MOZ_CRASHREPORTER)

        // We need the GPU process for this test
        assumeTrue(sessionRule.usingGpuProcess())

        // Cleanly kill GPU process
        sessionRule.killGpuProcess()

        // Expect this to time out as no crash should be reported
        client.getEvalResult(sessionRule.env.defaultTimeoutMillis)
    }

    @After
    fun teardown() {
        client.disconnect()
    }
}
