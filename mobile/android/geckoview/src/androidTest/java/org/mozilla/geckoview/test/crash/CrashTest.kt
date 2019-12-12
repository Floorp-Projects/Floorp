package org.mozilla.geckoview.test.crash

import android.content.Intent
import android.os.Message
import android.os.Messenger
import android.support.test.InstrumentationRegistry
import android.support.test.filters.MediumTest
import android.support.test.rule.ServiceTestRule
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.equalTo
import org.hamcrest.Matchers.notNullValue
import org.junit.Assert.assertThat
import org.junit.Assume
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.BuildConfig
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.test.util.Environment
import java.io.File
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
@MediumTest
class CrashTest {
    lateinit var messenger: Messenger
    val env = Environment()

    @get:Rule val rule = ServiceTestRule()

    @Before
    fun setup() {
        CrashTestHandler.queue.clear()

        val context = InstrumentationRegistry.getTargetContext()
        val binder = rule.bindService(Intent(context, RemoteGeckoService::class.java))
        messenger = Messenger(binder)
        assertThat("messenger should not be null", binder, notNullValue())
    }

    fun assertCrashIntent(intent: Intent, fatal: Boolean) {
        assertThat("Action should match",
                intent.action, equalTo(GeckoRuntime.ACTION_CRASHED))
        assertThat("Dump file should exist",
                File(intent.getStringExtra(GeckoRuntime.EXTRA_MINIDUMP_PATH)).exists(),
                equalTo(true))
        assertThat("Extras file should exist",
                File(intent.getStringExtra(GeckoRuntime.EXTRA_EXTRAS_PATH)).exists(),
                equalTo(true))

        assertThat("Fatality should match",
                intent.getBooleanExtra(GeckoRuntime.EXTRA_CRASH_FATAL, !fatal), equalTo(fatal))
    }

    @Test
    fun crashParent() {
        Assume.assumeFalse(env.isX86) // Too flaky on x86

        messenger.send(Message.obtain(null, RemoteGeckoService.CMD_CRASH_PARENT_NATIVE))
        assertCrashIntent(CrashTestHandler.queue.take(), true)
    }

    @Test
    fun crashContent() {
        // We need the crash reporter for this test
        Assume.assumeTrue(BuildConfig.MOZ_CRASHREPORTER)

        messenger.send(Message.obtain(null, RemoteGeckoService.CMD_CRASH_CONTENT_NATIVE))

        // This test is really slow so we allow double the usual timeout
        assertCrashIntent(CrashTestHandler.queue.poll(
                env.defaultTimeoutMillis * 2, TimeUnit.MILLISECONDS), false)
    }
}
