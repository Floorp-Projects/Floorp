/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.prompt

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.View
import kotlinx.android.synthetic.main.mozac_lib_crash_crashreporter.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R

private const val PREFERENCE_KEY_SEND_REPORT = "sendCrashReport"

/**
 * Activity showing the crash reporter prompt asking the user for confirmation before submitting a crash report.
 */
class CrashReporterActivity : AppCompatActivity() {
    private val crashReporter: CrashReporter by lazy { CrashReporter.requireInstance }
    private val crash: Crash by lazy { Crash.fromIntent(intent) }
    private val sharedPreferences by lazy {
        getSharedPreferences("mozac_lib_crash_settings", Context.MODE_PRIVATE)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        setTheme(crashReporter.promptConfiguration.theme)

        super.onCreate(savedInstanceState)

        setContentView(R.layout.mozac_lib_crash_crashreporter)

        setupViews()
    }

    private fun setupViews() {
        val appName = crashReporter.promptConfiguration.appName
        val organizationName = crashReporter.promptConfiguration.organizationName

        titleView.text = getString(R.string.mozac_lib_crash_dialog_title, appName)
        sendCheckbox.text = getString(R.string.mozac_lib_crash_dialog_checkbox, organizationName)

        sendCheckbox.isChecked = sharedPreferences.getBoolean(PREFERENCE_KEY_SEND_REPORT, true)

        restartButton.apply {
            text = getString(R.string.mozac_lib_crash_dialog_button_restart, appName)
            setOnClickListener { restart() }
        }
        closeButton.setOnClickListener { close() }

        if (crashReporter.promptConfiguration.message == null) {
            messageView.visibility = View.GONE
        } else {
            messageView.text = crashReporter.promptConfiguration.message
        }
    }

    private fun close() {
        sendCrashReportIfNeeded {
            finish()
        }
    }

    private fun restart() {
        println(packageName)

        sendCrashReportIfNeeded {
            val launchIntent = packageManager.getLaunchIntentForPackage(packageName)
            if (launchIntent != null) {
                launchIntent.flags = launchIntent.flags or Intent.FLAG_ACTIVITY_NEW_TASK
                startActivity(launchIntent)
            }

            finish()
        }
    }

    private fun sendCrashReportIfNeeded(then: () -> Unit) {
        sharedPreferences.edit().putBoolean(PREFERENCE_KEY_SEND_REPORT, sendCheckbox.isChecked).apply()

        if (!sendCheckbox.isChecked) {
            then()
            return
        }

        GlobalScope.launch {
            crashReporter.submitReport(crash)

            withContext(Dispatchers.Main) {
                then()
            }
        }
    }

    override fun onBackPressed() {
        sendCrashReportIfNeeded {
            finish()
        }
    }
}
