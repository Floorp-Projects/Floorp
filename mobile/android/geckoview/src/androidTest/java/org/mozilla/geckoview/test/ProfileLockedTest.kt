package org.mozilla.geckoview.test

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.*
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import androidx.test.platform.app.InstrumentationRegistry
import org.hamcrest.CoreMatchers.equalTo
import org.junit.Assert.assertThat
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import java.io.File


@RunWith(AndroidJUnit4::class)
@MediumTest
class ProfileLockedTest {
    private val targetContext
            get() = InstrumentationRegistry.getInstrumentation().targetContext

    @get:Rule
    var folder = TemporaryFolder()

    /**
     * Starts GeckoRuntime in the process given in input, and waits for the MESSAGE_PAGE_STOP
     * event that's fired when the first GeckoSession receives the onPageStop event.
     *
     * We wait for a page load to make sure that everything started up correctly (as opposed
     * to quitting during the startup procedure).
     */
    class RuntimeInstance<T>(
            private val context: Context,
            private val service: Class<T>,
            private val profileFolder: File) : ServiceConnection {

        var isConnected = false
        var disconnected = GeckoResult<Void>()
        var started = GeckoResult<Void>()
        var quitted = GeckoResult<Void>()

        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            val handler = object : Handler(Looper.getMainLooper()) {
                override fun handleMessage(msg: Message) {
                    when (msg.what) {
                        TestProfileLockService.MESSAGE_PAGE_STOP -> started.complete(null)
                        TestProfileLockService.MESSAGE_QUIT -> {
                            quitted.complete(null)
                            // No reason to keep the service around anymore
                            context.unbindService(this@RuntimeInstance)
                        }
                    }
                }
            }

            val messenger = Messenger(handler)
            val service = Messenger(binder)

            val msg = Message.obtain(null, TestProfileLockService.MESSAGE_REGISTER)
            msg.replyTo = messenger
            service.send(msg)

            isConnected = true
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            isConnected = false
            context.unbindService(this)
            disconnected.complete(null)
        }

        fun start() {
            val intent = Intent(context, service)
            intent.putExtra("args", "-profile ${profileFolder.absolutePath}")

            context.bindService(intent, this, Context.BIND_AUTO_CREATE)
        }
    }

    @Test
    fun profileLocked() {
        val profileFolder = folder.newFolder("profile-locked-test")

        val runtime0 = RuntimeInstance(targetContext,
                TestProfileLockService.p0::class.java,
                profileFolder)
        val runtime1 = RuntimeInstance(targetContext,
                TestProfileLockService.p1::class.java,
                profileFolder)

        // Start the first runtime and wait until it's ready
        runtime0.start()
        runtime0.started.pollDefault()

        assertThat("The service should be connected now", runtime0.isConnected, equalTo(true))

        // Now start a _second_ runtime with the same profile folder, this will kill the first
        // runtime
        runtime1.start()

        // Wait for the first runtime to disconnect
        runtime0.disconnected.pollDefault()

        // GeckoRuntime will quit after killing the offending process
        runtime1.quitted.pollDefault()

        assertThat("The service shouldn't be connected anymore",
                runtime0.isConnected, equalTo(false))
    }
}
