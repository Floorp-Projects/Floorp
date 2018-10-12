/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.handler

import android.app.IntentService
import android.content.Intent
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.Crash

private const val WORKER_THREAD_NAME = "CrashHandlerService"

/**
 * Service receiving native code crashes (from GeckoView).
 */
class CrashHandlerService : IntentService(WORKER_THREAD_NAME) {
    private val logger by lazy { CrashReporter
        .requireInstance
        .logger
    }

    public override fun onHandleIntent(intent: Intent?) {
        if (intent == null) {
            return
        }

        logger.error("CrashHandlerService received native code crash")

        intent.extras?.let { extras ->
            val crash = Crash.NativeCodeCrash.fromBundle(extras)

            CrashReporter
                .requireInstance
                .onCrash(this, crash)
        } ?: logger.error("Received intent with null extras")

        kill()
    }

    internal fun kill() {
        // Avoid ANR due to background limitations on Oreo+
        System.exit(0)
    }
}
