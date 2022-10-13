/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.prompt

import android.content.Context
import android.content.Intent
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter

internal class CrashPrompt(
    private val context: Context,
    private val crash: Crash,
) {
    fun show() {
        context.startActivity(createIntent(context, crash))
    }

    companion object {
        fun createIntent(context: Context, crash: Crash): Intent {
            val intent = Intent(context, CrashReporterActivity::class.java)
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION)
            // For background process native crashes we want to keep the browser visible in the
            // background behind the prompt. For other types we want to clear the existing task.
            if (crash is Crash.NativeCodeCrash &&
                crash.processType == Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD
            ) {
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
            } else {
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK)
            }

            crash.fillIn(intent)

            return intent
        }

        fun shouldPromptForCrash(shouldPrompt: CrashReporter.Prompt, crash: Crash): Boolean {
            return when (shouldPrompt) {
                CrashReporter.Prompt.ALWAYS -> true
                CrashReporter.Prompt.NEVER -> false
                CrashReporter.Prompt.ONLY_NATIVE_CRASH -> crash is Crash.NativeCodeCrash
            }
        }
    }
}
