/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.crash

import android.app.Application
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.widget.Toast
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.lib.crash.service.GleanCrashReporterService
import mozilla.components.service.glean.Glean

class CrashApplication : Application() {
    internal lateinit var crashReporter: CrashReporter

    override fun onCreate() {
        super.onCreate()

        // We want the log messages of all builds to go to Android logcat
        Log.addSink(AndroidLogSink())

        crashReporter = CrashReporter(
            services = listOf(createDummyCrashService(this), GleanCrashReporterService(applicationContext)),
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            promptConfiguration = CrashReporter.PromptConfiguration(
                appName = "Sample App",
                organizationName = "Mozilla",
                message = "As a private browser, we never save and cannot restore your last browsing session.",
                theme = R.style.CrashDialogTheme
            ),
            nonFatalCrashIntent = createNonFatalPendingIntent(this),
            enabled = true
        ).install(this)

        // Initialize Glean for recording by the GleanCrashReporterService
        Glean.setUploadEnabled(true)
        Glean.initialize(applicationContext)
    }

    companion object {
        const val NON_FATAL_CRASH_BROADCAST = "org.mozilla.samples.crash.CRASH"
    }
}

private fun createDummyCrashService(context: Context): CrashReporterService {
    // For this sample we create a dummy service. In a real application this would be an instance of SentryCrashService
    // or SocorroCrashService.
    return object : CrashReporterService {
        override fun report(crash: Crash.UncaughtExceptionCrash) {
            GlobalScope.launch(Dispatchers.Main) {
                Toast.makeText(context, "Uploading uncaught exception crash..", Toast.LENGTH_SHORT).show()
            }
        }

        override fun report(crash: Crash.NativeCodeCrash) {
            GlobalScope.launch(Dispatchers.Main) {
                Toast.makeText(context, "Uploading native crash..", Toast.LENGTH_SHORT).show()
            }
        }
    }
}

private fun createNonFatalPendingIntent(context: Context): PendingIntent {
    // The PendingIntent can launch whatever you want - an activity, a service... Here we pick a broadcast. Our main
    // activity will listener for the broadcast and show an in-app snackbar to ask the user whether we should send
    // this crash report.
    return PendingIntent.getBroadcast(context, 0, Intent(CrashApplication.NON_FATAL_CRASH_BROADCAST), 0)
}

val Context.crashReporter: CrashReporter
    get() = (applicationContext as CrashApplication).crashReporter
