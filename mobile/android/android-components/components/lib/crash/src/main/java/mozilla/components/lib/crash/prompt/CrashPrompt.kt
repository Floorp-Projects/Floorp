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
    private val crash: Crash
) {
    fun show() {
        context.startActivity(createIntent(context, crash))
    }

    companion object {
        fun createIntent(context: Context, crash: Crash): Intent {
            val intent = Intent(context, CrashReporterActivity::class.java)
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK)
            intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION)

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
