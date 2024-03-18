/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.prompt

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import androidx.appcompat.app.AppCompatActivity
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.app.NotificationManagerCompat
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R
import mozilla.components.lib.crash.databinding.MozacLibCrashCrashreporterBinding
import mozilla.components.lib.crash.notification.CrashNotification
import mozilla.components.lib.crash.notification.NOTIFICATION_ID
import mozilla.components.lib.crash.notification.NOTIFICATION_TAG
import kotlin.coroutines.CoroutineContext
import kotlin.coroutines.EmptyCoroutineContext

/**
 * Activity showing the crash reporter prompt asking the user for confirmation before submitting a crash report.
 */
class CrashReporterActivity : AppCompatActivity() {

    private val crashReporter: CrashReporter by lazy { CrashReporter.requireInstance }
    private val crash: Crash by lazy { Crash.fromIntent(intent) }
    private val sharedPreferences by lazy {
        getSharedPreferences(SHARED_PREFERENCES_NAME, Context.MODE_PRIVATE)
    }

    /**
     * Coroutine context for crash reporter operations. Can be used to setup dispatcher for tests.
     */
    @VisibleForTesting(otherwise = PRIVATE)
    internal var reporterCoroutineContext: CoroutineContext = EmptyCoroutineContext

    @VisibleForTesting(otherwise = PRIVATE)
    internal lateinit var binding: MozacLibCrashCrashreporterBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        // if the activity is started by user tapping on the crash notification's report button,
        // remove the crash notification.
        if (CrashNotification.shouldShowNotificationInsteadOfPrompt(crash)) {
            NotificationManagerCompat.from(applicationContext).cancel(NOTIFICATION_TAG, NOTIFICATION_ID)
        }

        setTheme(crashReporter.promptConfiguration.theme)

        super.onCreate(savedInstanceState)

        binding = MozacLibCrashCrashreporterBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupViews()
    }

    private fun setupViews() {
        val appName = crashReporter.promptConfiguration.appName
        val organizationName = crashReporter.promptConfiguration.organizationName

        binding.titleView.text = when (isRecoverableBackgroundCrash(crash)) {
            true -> getString(
                R.string.mozac_lib_crash_background_process_notification_title,
                appName,
            )
            false -> getString(R.string.mozac_lib_crash_dialog_title, appName)
        }

        binding.sendCheckbox.text = getString(R.string.mozac_lib_crash_dialog_checkbox, organizationName)
        binding.sendCheckbox.isChecked = sharedPreferences.getBoolean(PREFERENCE_KEY_SEND_REPORT, true)

        binding.restartButton.apply {
            text = getString(R.string.mozac_lib_crash_dialog_button_restart, appName)
            setOnClickListener { restart() }
        }
        binding.closeButton.setOnClickListener { close() }

        // For background crashes show just the close button. Otherwise show close and restart.
        if (isRecoverableBackgroundCrash(crash)) {
            binding.restartButton.visibility = View.GONE
            val closeButtonParams = binding.closeButton.layoutParams as ConstraintLayout.LayoutParams
            closeButtonParams.startToStart = ConstraintLayout.LayoutParams.UNSET
            closeButtonParams.endToEnd = ConstraintLayout.LayoutParams.PARENT_ID
        } else {
            binding.restartButton.visibility = View.VISIBLE
            val closeButtonParams = binding.closeButton.layoutParams as ConstraintLayout.LayoutParams
            closeButtonParams.startToStart = ConstraintLayout.LayoutParams.PARENT_ID
            closeButtonParams.endToEnd = ConstraintLayout.LayoutParams.UNSET
        }

        if (crashReporter.promptConfiguration.message == null) {
            binding.messageView.visibility = View.GONE
        } else {
            binding.messageView.text = crashReporter.promptConfiguration.message
        }
    }

    private fun close() {
        sendCrashReportIfNeeded {
            finish()
        }
    }

    private fun restart() {
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
        sharedPreferences.edit().putBoolean(PREFERENCE_KEY_SEND_REPORT, binding.sendCheckbox.isChecked).apply()

        if (!binding.sendCheckbox.isChecked) {
            then()
            return
        }

        crashReporter.submitReport(crash) {
            then()
        }
    }

    override fun onBackPressed() {
        sendCrashReportIfNeeded {
            finish()
        }
    }

    /*
     * Return true if the crash occurred in the background and is recoverable. (ex: GPU process crash)
     */
    @VisibleForTesting
    internal fun isRecoverableBackgroundCrash(crash: Crash): Boolean {
        return crash is Crash.NativeCodeCrash &&
            crash.processType == Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD
    }

    companion object {

        @VisibleForTesting(otherwise = PRIVATE)
        internal const val SHARED_PREFERENCES_NAME = "mozac_lib_crash_settings"

        @VisibleForTesting(otherwise = PRIVATE)
        internal const val PREFERENCE_KEY_SEND_REPORT = "sendCrashReport"
    }
}
