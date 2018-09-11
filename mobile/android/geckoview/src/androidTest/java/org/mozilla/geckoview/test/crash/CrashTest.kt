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
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.Timeout
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.test.util.Environment
import java.io.File

@RunWith(AndroidJUnit4::class)
@MediumTest
class CrashTest {
    companion object {
        val DEFAULT_X86_EMULATOR_TIMEOUT_MILLIS = 30000L
        val DEFAULT_X86_DEVICE_TIMEOUT_MILLIS = 30000L
        val DEFAULT_ARM_EMULATOR_TIMEOUT_MILLIS = 60000L
        val DEFAULT_ARM_DEVICE_TIMEOUT_MILLIS = 60000L
    }

    lateinit var messenger: Messenger

    @get:Rule val rule = ServiceTestRule()

    @get:Rule val timeoutRule = Timeout.millis(getTimeoutMillis())

    fun getTimeoutMillis(): Long {
        val env = Environment()
        if ("x86" == env.cpuArch) {
            return if (env.isEmulator)
                CrashTest.DEFAULT_X86_EMULATOR_TIMEOUT_MILLIS
            else
                CrashTest.DEFAULT_X86_DEVICE_TIMEOUT_MILLIS
        }
        return if (env.isEmulator)
            CrashTest.DEFAULT_ARM_EMULATOR_TIMEOUT_MILLIS
        else
            CrashTest.DEFAULT_ARM_DEVICE_TIMEOUT_MILLIS
    }

    @Before
    fun setup() {
        CrashTestHandler.queue.clear()

        val context = InstrumentationRegistry.getTargetContext();
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
        assertThat("Dump should be succcesful",
                intent.getBooleanExtra(GeckoRuntime.EXTRA_MINIDUMP_SUCCESS, false),
                equalTo(true))

        assertThat("Fatality should match",
                intent.getBooleanExtra(GeckoRuntime.EXTRA_CRASH_FATAL, !fatal), equalTo(fatal))
    }

    @Test
    fun crashParent() {
        messenger.send(Message.obtain(null, RemoteGeckoService.CMD_CRASH_PARENT_NATIVE))
        assertCrashIntent(CrashTestHandler.queue.take(), true)
    }

    @Test
    fun crashContent() {
        messenger.send(Message.obtain(null, RemoteGeckoService.CMD_CRASH_CONTENT_NATIVE))
        assertCrashIntent(CrashTestHandler.queue.take(), false)
    }
}