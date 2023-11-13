package org.mozilla.geckoview.test.crash

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.test.BaseSessionTest
import org.mozilla.geckoview.test.TestCrashHandler
import org.mozilla.geckoview.test.TestRuntimeService
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ClosedSessionAtStart

@RunWith(AndroidJUnit4::class)
@MediumTest
class ParentCrashTest : BaseSessionTest() {
    private val targetContext
        get() = InstrumentationRegistry.getInstrumentation().targetContext

    private val timeout
        get() = sessionRule.env.defaultTimeoutMillis

    @Test
    @ClosedSessionAtStart
    fun crashParent() {
        val client = TestCrashHandler.Client(targetContext)

        assertTrue(client.connect(timeout))
        client.setEvalNextCrashDump(GeckoRuntime.CRASHED_PROCESS_TYPE_MAIN, null)

        val runtime = TestRuntimeService.RuntimeInstance.start(
            targetContext,
            RuntimeCrashTestService::class.java,
            temporaryProfile.get(),
        )
        runtime.loadUri("about:crashparent")

        val evalResult = client.getEvalResult(timeout)
        assertTrue(evalResult.mMsg, evalResult.mResult)

        client.disconnect()
    }
}
