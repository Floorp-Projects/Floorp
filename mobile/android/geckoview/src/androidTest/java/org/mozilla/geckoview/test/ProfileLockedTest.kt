package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import androidx.test.platform.app.InstrumentationRegistry
import org.hamcrest.CoreMatchers.equalTo
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.TestRuntimeService.RuntimeInstance
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ClosedSessionAtStart

@RunWith(AndroidJUnit4::class)
@MediumTest
class ProfileLockedTest : BaseSessionTest() {
    private val targetContext
        get() = InstrumentationRegistry.getInstrumentation().targetContext

    @Test
    @ClosedSessionAtStart
    fun profileLocked() {
        val runtime0 = RuntimeInstance.start(
            targetContext,
            TestRuntimeService.instance0::class.java,
            temporaryProfile.get(),
        )

        // Start the first runtime and wait until it's ready
        sessionRule.waitForResult(runtime0.started)

        assertThat("The service should be connected now", runtime0.isConnected, equalTo(true))

        // Now start a _second_ runtime with the same profile folder, this will kill the first
        // runtime
        val runtime1 = RuntimeInstance.start(
            targetContext,
            TestRuntimeService.instance1::class.java,
            temporaryProfile.get(),
        )

        // Wait for the first runtime to disconnect
        sessionRule.waitForResult(runtime0.disconnected)

        // GeckoRuntime will quit after killing the offending process
        sessionRule.waitForResult(runtime1.quitted)

        assertThat(
            "The service shouldn't be connected anymore",
            runtime0.isConnected,
            equalTo(false),
        )
    }
}
