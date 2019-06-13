/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.crash

import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import androidx.core.content.ContextCompat
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.activity_crash.*
import mozilla.components.lib.crash.Crash

class CrashActivity : AppCompatActivity(), View.OnClickListener {
    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (!Crash.isCrashIntent(intent)) {
                return
            }

            val crash = Crash.fromIntent(intent)

            Snackbar.make(findViewById(android.R.id.content), "Sorry. We crashed.", Snackbar.LENGTH_LONG)
                .setAction("Report") { crashReporter.submitReport(crash) }
                .show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_crash)

        fatalCrashButton.setOnClickListener(this)
        crashButton.setOnClickListener(this)
        fatalServiceCrashButton.setOnClickListener(this)
    }

    override fun onResume() {
        super.onResume()

        registerReceiver(receiver, IntentFilter(CrashApplication.NON_FATAL_CRASH_BROADCAST))
    }

    override fun onPause() {
        super.onPause()

        unregisterReceiver(receiver)
    }

    @Suppress("TooGenericExceptionThrown")
    override fun onClick(view: View) {
        when (view) {
            fatalCrashButton -> throw RuntimeException("Boom!")

            crashButton -> {
                // Pretend GeckoView has crashed by re-building a crash Intent and launching the CrashHandlerService.
                val intent = Intent("org.mozilla.gecko.ACTION_CRASHED")
                intent.component = ComponentName(
                    packageName,
                    "mozilla.components.lib.crash.handler.CrashHandlerService"
                )
                intent.putExtra(
                    "minidumpPath",
                    "${filesDir.path}/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp"
                )
                intent.putExtra("fatal", false)
                intent.putExtra(
                    "extrasPath",
                    "${filesDir.path}/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra"
                )
                intent.putExtra("minidumpSuccess", true)

                ContextCompat.startForegroundService(this, intent)
            }

            fatalServiceCrashButton -> {
                startService(Intent(this, CrashService::class.java))
                finish()
            }
        }
    }
}
