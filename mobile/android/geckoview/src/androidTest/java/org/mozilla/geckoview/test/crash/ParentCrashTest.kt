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
import org.junit.Assert.assertThat
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeThat
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.TestCrashHandler
import org.mozilla.geckoview.test.util.Environment
import org.mozilla.geckoview.test.util.RuntimeCreator

@RunWith(AndroidJUnit4::class)
@MediumTest
class ParentCrashTest {
    lateinit var messenger: Messenger
    val env = Environment()

    @get:Rule val rule = ServiceTestRule()

    @get:Rule
    var folder = TemporaryFolder()

    @Before
    fun setup() {
        // Since this test starts up its own GeckoRuntime via
        // RemoteGeckoService, we need to shutdown any runtime already running
        // in the RuntimeCreator.
        RuntimeCreator.shutdownRuntime()

        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val intent = Intent(context, RemoteGeckoService::class.java)

        // We need to run in a different profile so we don't conflict with other tests running
        // in parallel in other processes.
        val profileFolder = folder.newFolder("remote-gecko-test")
        intent.putExtra("args", "-profile ${profileFolder.absolutePath}")

        val binder = rule.bindService(intent)
        messenger = Messenger(binder)
        assertThat("messenger should not be null", binder, notNullValue())
    }

    @Test
    @UiThreadTest
    fun crashParent() {
        // TODO: Bug 1673956
        assumeThat(env.isFission, equalTo(false))
        val client = TestCrashHandler.Client(InstrumentationRegistry.getInstrumentation().targetContext)

        assertTrue(client.connect(env.defaultTimeoutMillis))
        client.setEvalNextCrashDump(/* expectFatal */ true)

        messenger.send(Message.obtain(null, RemoteGeckoService.CMD_CRASH_PARENT_NATIVE))

        var evalResult = client.getEvalResult(env.defaultTimeoutMillis)
        assertTrue(evalResult.mMsg, evalResult.mResult)

        client.disconnect()
    }
}
