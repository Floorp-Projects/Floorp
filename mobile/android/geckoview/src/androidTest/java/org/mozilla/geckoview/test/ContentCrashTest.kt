package org.mozilla.geckoview.test

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeThat
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.BuildConfig
import org.mozilla.geckoview.GeckoSession.ContentDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.IgnoreCrash


@RunWith(AndroidJUnit4::class)
@MediumTest
class ContentCrashTest : BaseSessionTest() {
    val client = TestCrashHandler.Client(InstrumentationRegistry.getInstrumentation().targetContext)

    @Before
    fun setup() {
        assertTrue(client.connect(env.defaultTimeoutMillis))
        client.setEvalNextCrashDump(/* expectFatal */ false)
    }

    @IgnoreCrash
    @Test
    fun crashContent() {
        // We need the crash reporter for this test
        assumeTrue(BuildConfig.MOZ_CRASHREPORTER)

        // TODO: bug 1710940
        assumeThat(sessionRule.env.isIsolatedProcess, Matchers.equalTo(false))

        mainSession.loadUri(CONTENT_CRASH_URL)
        mainSession.waitUntilCalled(ContentDelegate::class, "onCrash")

        // This test is really slow so we allow double the usual timeout
        var evalResult = client.getEvalResult(env.defaultTimeoutMillis * 2)
        assertTrue(evalResult.mMsg, evalResult.mResult)
    }

    @After
    fun teardown() {
        client.disconnect()
    }
}
