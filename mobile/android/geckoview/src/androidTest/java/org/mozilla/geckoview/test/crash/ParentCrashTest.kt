package org.mozilla.geckoview.test.crash

import android.content.Intent
import android.os.Message
import android.os.Messenger
import androidx.test.annotation.UiThreadTest
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.filters.MediumTest
import androidx.test.rule.ServiceTestRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.equalTo
import org.hamcrest.Matchers.notNullValue
import org.junit.After
import org.junit.Assert.assertThat
import org.junit.Assert.assertTrue
import org.junit.Assume
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.BuildConfig
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.test.TestCrashHandler
import org.mozilla.geckoview.test.util.Environment
import java.io.File
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
@MediumTest
class ParentCrashTest {
    lateinit var messenger: Messenger
    val env = Environment()

    @get:Rule val rule = ServiceTestRule()

    @Before
    fun setup() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val binder = rule.bindService(Intent(context, RemoteGeckoService::class.java))
        messenger = Messenger(binder)
        assertThat("messenger should not be null", binder, notNullValue())
    }

    @Test
    @UiThreadTest
    fun crashParent() {
        val client = TestCrashHandler.Client(InstrumentationRegistry.getInstrumentation().targetContext)

        assertTrue(client.connect(env.defaultTimeoutMillis))
        client.setEvalNextCrashDump(/* expectFatal */ true)

        messenger.send(Message.obtain(null, RemoteGeckoService.CMD_CRASH_PARENT_NATIVE))

        var evalResult = client.getEvalResult(env.defaultTimeoutMillis)
        assertTrue(evalResult.mMsg, evalResult.mResult)

        client.disconnect()
    }
}
