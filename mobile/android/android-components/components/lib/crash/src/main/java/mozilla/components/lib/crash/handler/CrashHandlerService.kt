/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.handler

import android.content.Intent
import androidx.core.app.JobIntentService
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.Crash

/**
 * Service receiving native code crashes (from GeckoView).
 */
class CrashHandlerService : JobIntentService() {
    private val logger by lazy {
        CrashReporter.requireInstance.logger
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        intent?.apply {
            handleNativeCodeCrash(this)
        }

        // Using this as a intent service, not calling super.onStartCommand
        return START_NOT_STICKY
    }

    public override fun onHandleWork(intent: Intent) {
        // Not handling work from enqueueWork
    }

    private fun handleNativeCodeCrash(intent: Intent) {
        logger.error("CrashHandlerService received native code crash")

        intent.extras?.let { extras ->
            val crash = Crash.NativeCodeCrash.fromBundle(extras)

            CrashReporter.requireInstance.onCrash(this, crash)
        } ?: logger.error("Received intent with null extras")
    }
}
