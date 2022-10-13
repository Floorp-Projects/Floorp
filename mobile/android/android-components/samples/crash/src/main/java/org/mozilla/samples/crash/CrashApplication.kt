/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.crash

import android.app.Application
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.widget.Toast
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.lib.crash.service.GleanCrashReporterService
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.glean.BuildInfo
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.net.ConceptFetchHttpUploader
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.utils.PendingIntentUtils
import java.util.Calendar
import java.util.TimeZone
import java.util.UUID

@Suppress("MagicNumber")
internal object GleanBuildInfo {
    val buildInfo: BuildInfo by lazy {
        BuildInfo(
            versionCode = "0.0.1",
            versionName = "0.0.1",
            buildDate = Calendar.getInstance(
                TimeZone.getTimeZone("GMT+0"),
            ).also { cal -> cal.set(2019, 9, 23, 12, 52, 8) },
        )
    }
}

class CrashApplication : Application() {
    internal lateinit var crashReporter: CrashReporter

    override fun onCreate() {
        super.onCreate()

        // We want the log messages of all builds to go to Android logcat
        Log.addSink(AndroidLogSink())

        crashReporter = CrashReporter(
            context = this,
            services = listOf(
                createDummyCrashService(this),
            ),
            telemetryServices = listOf(GleanCrashReporterService(applicationContext)),
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            promptConfiguration = CrashReporter.PromptConfiguration(
                appName = "Sample App",
                organizationName = "Mozilla",
                message = "As a private browser, we never save and cannot restore your last browsing session.",
                theme = R.style.CrashDialogTheme,
            ),
            nonFatalCrashIntent = createNonFatalPendingIntent(this),
            enabled = true,
        ).install(this)

        // Initialize Glean for recording by the GleanCrashReporterService
        val httpClient = ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() })
        val config = Configuration(httpClient = httpClient)
        Glean.initialize(
            applicationContext,
            uploadEnabled = true,
            configuration = config,
            buildInfo = GleanBuildInfo.buildInfo,
        )
    }

    companion object {
        const val NON_FATAL_CRASH_BROADCAST = "org.mozilla.samples.crash.CRASH"
    }
}

@OptIn(DelicateCoroutinesApi::class)
private fun createDummyCrashService(context: Context): CrashReporterService {
    // For this sample we create a dummy service. In a real application this would be an instance of SentryCrashService
    // or SocorroCrashService.
    return object : CrashReporterService {
        override val id: String = "dummy"

        override val name: String = "Dummy"

        override fun createCrashReportUrl(identifier: String): String? {
            return "https://example.org/$identifier"
        }

        override fun report(crash: Crash.UncaughtExceptionCrash): String? {
            GlobalScope.launch(Dispatchers.Main) {
                Toast.makeText(context, "Uploading uncaught exception crash...", Toast.LENGTH_SHORT).show()
            }
            return createDummyId()
        }

        override fun report(crash: Crash.NativeCodeCrash): String? {
            GlobalScope.launch(Dispatchers.Main) {
                Toast.makeText(context, "Uploading native crash...", Toast.LENGTH_SHORT).show()
            }
            return createDummyId()
        }

        override fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String? {
            GlobalScope.launch(Dispatchers.Main) {
                Toast.makeText(context, "Uploading caught exception...", Toast.LENGTH_SHORT).show()
            }
            return createDummyId()
        }

        private fun createDummyId(): String {
            return "dummy${UUID.randomUUID().toString().hashCode()}"
        }
    }
}

private fun createNonFatalPendingIntent(context: Context): PendingIntent {
    // The PendingIntent can launch whatever you want - an activity, a service... Here we pick a broadcast. Our main
    // activity will listener for the broadcast and show an in-app snackbar to ask the user whether we should send
    // this crash report.
    return PendingIntent.getBroadcast(
        context,
        0,
        Intent(CrashApplication.NON_FATAL_CRASH_BROADCAST),
        PendingIntentUtils.defaultFlags,
    )
}

val Context.crashReporter: CrashReporter
    get() = (applicationContext as CrashApplication).crashReporter
