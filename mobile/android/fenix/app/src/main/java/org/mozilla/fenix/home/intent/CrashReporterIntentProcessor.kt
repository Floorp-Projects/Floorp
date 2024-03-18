/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import android.util.Log
import androidx.navigation.NavController
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.Crash.NativeCodeCrash
import mozilla.components.lib.crash.CrashReporter
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction

/**
 * Process the [Intent] from [CrashReporter] through which the app is informed about
 * recoverable native crashes.
 */
class CrashReporterIntentProcessor(private val appStore: AppStore) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        return if (Crash.isCrashIntent(intent)) {
            val crash = Crash.fromIntent(intent)
            // If only a child process crashed we can handle this gracefully.
            if ((crash as? NativeCodeCrash)?.isFatal == false) {
                appStore.dispatch(AppAction.AddNonFatalCrash(crash))
            } else {
                // A fatal crash means the app's main process is affected.
                // An UncaughtExceptionCrash refers to a [Throwable] that would otherwise crash the app
                // but is intercepted to allow us to gather more info and crash more gracefully.
                //
                // In both cases the app is left in a bad state so the main process is killed
                // but not before gathering more info about the crashes to form and persist a crash report and
                // not before "CrashHandlerService" is started in a separate process to be able to
                // show a dialog allowing users to send the crash report and maybe restart the app.

                // Log that an unexpected crash was sent but avoid leaking potential sensitive information.
                // We expect other types of crashes to be handled by "CrashHandlerService".
                Log.e("CrashReporterProcessor", "Invalid crash to process: ${crash::class}")
            }
            true
        } else {
            false
        }
    }
}
