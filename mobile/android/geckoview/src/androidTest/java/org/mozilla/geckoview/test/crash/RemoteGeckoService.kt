package org.mozilla.geckoview.test.crash

import android.app.Service
import android.content.Intent
import android.os.*
import org.hamcrest.MatcherAssert.assertThat
import org.hamcrest.Matchers.equalTo
import org.mozilla.gecko.GeckoProfile
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings

class RemoteGeckoService : Service() {
    companion object {
        val LOGTAG = "RemoteGeckoService"
        val CMD_CRASH_PARENT_NATIVE = 1
        val CMD_CRASH_CONTENT_NATIVE = 2
        var runtime: GeckoRuntime? = null
    }

    var session: GeckoSession? = null;

    class TestHandler: Handler() {
        override fun handleMessage(msg: Message) {
            when (msg.what) {
                CMD_CRASH_PARENT_NATIVE -> {
                    val settings = GeckoSessionSettings()
                    settings.setBoolean(GeckoSessionSettings.USE_MULTIPROCESS, false)
                    val session = GeckoSession(settings)
                    session.open(runtime!!)
                    session.loadUri("about:crashparent")
                }
                CMD_CRASH_CONTENT_NATIVE -> {
                    val settings = GeckoSessionSettings()
                    settings.setBoolean(GeckoSessionSettings.USE_MULTIPROCESS, true)
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
            // We need to run in a different profile so we don't conflict with other tests running
            // in parallel in other processes.
            val extras = Bundle(1)
            extras.putString("args", "-P remote")

            runtime = GeckoRuntime.create(this.applicationContext,
                    GeckoRuntimeSettings.Builder()
                            .extras(extras)
                            .crashHandler(CrashTestHandler::class.java).build())
        }

        return handler.binder

    }
}
