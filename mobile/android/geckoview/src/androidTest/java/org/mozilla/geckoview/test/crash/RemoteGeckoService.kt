package org.mozilla.geckoview.test.crash

import android.app.Service
import android.content.Intent
import android.os.*
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.test.TestCrashHandler

class RemoteGeckoService : Service() {
    companion object {
        val CMD_CRASH_PARENT_NATIVE = 1
        val CMD_CRASH_CONTENT_NATIVE = 2
        var runtime: GeckoRuntime? = null
    }

    var session: GeckoSession? = null;

    class TestHandler: Handler(Looper.getMainLooper()) {
        override fun handleMessage(msg: Message) {
            when (msg.what) {
                CMD_CRASH_PARENT_NATIVE -> {
                    val settings = GeckoSessionSettings()
                    val session = GeckoSession(settings)
                    session.open(runtime!!)
                    session.loadUri("about:crashparent")
                }
                CMD_CRASH_CONTENT_NATIVE -> {
                    val settings = GeckoSessionSettings.Builder()
                            .build()
                    val session = GeckoSession(settings)
                    session.open(runtime!!)
                    session.loadUri("about:crashcontent")
                }
                else -> {
                    throw RuntimeException("Unhandled command")
                }
            }
        }
    }

    val handler = Messenger(TestHandler())

    override fun onBind(intent: Intent): IBinder {
        if (runtime == null) {
            runtime = GeckoRuntime.create(this.applicationContext,
                    GeckoRuntimeSettings.Builder()
                            .extras(intent.extras!!)
                            .crashHandler(TestCrashHandler::class.java).build())
        }

        return handler.binder

    }
}
