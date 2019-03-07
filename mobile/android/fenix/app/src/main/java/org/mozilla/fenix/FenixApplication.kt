/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.app.Application
import android.content.Context
import android.os.Process
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.service.fretboard.Fretboard
import mozilla.components.service.fretboard.source.kinto.KintoExperimentSource
import mozilla.components.service.fretboard.storage.flatfile.FlatFileExperimentStorage
import mozilla.components.service.glean.Glean
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.rustlog.RustLog
import org.mozilla.fenix.AdjustHelper.setupAdjustIfNeeded
import org.mozilla.fenix.LeanplumHelper.setupLeanplumIfNeeded
import org.mozilla.fenix.components.Components
import java.io.File

@SuppressLint("Registered")
open class FenixApplication : Application() {
    lateinit var fretboard: Fretboard

    val components by lazy { Components(this) }

    override fun onCreate() {
        super.onCreate()

        val megazordEnabled = setupMegazord()
        setupLogging(megazordEnabled)
        setupCrashReporting()

        if (!isMainProcess(this)) {
            // If this is not the main process then do not continue with the initialization here. Everything that
            // follows only needs to be done in our app's main process and should not be done in other processes like
            // a GeckoView child process or the crash handling process. Most importantly we never want to end up in a
            // situation where we create a GeckoRuntime from the Gecko child process (
            return
        }

        setupLeakCanary()
        setupGlean(this)
        loadExperiments()
        setupAdjustIfNeeded(this)
        setupLeanplumIfNeeded(this)
    }

    protected open fun setupLeakCanary() {
        // no-op, LeakCanary is disabled by default
    }

    open fun toggleLeakCanary(newValue: Boolean) {
        // no-op, LeakCanary is disabled by default
    }

    private fun setupLogging(megazordEnabled: Boolean) {
        // We want the log messages of all builds to go to Android logcat
        Log.addSink(AndroidLogSink())

        if (megazordEnabled) {
            // We want rust logging to go through the log sinks.
            // This has to happen after initializing the megazord, and
            // it's only worth doing in the case that we are a megazord.
            RustLog.enable()
        }
    }

    private fun setupGlean(context: Context) {
        Glean.initialize(context)
        Glean.setUploadEnabled(BuildConfig.TELEMETRY)
    }

    private fun loadExperiments() {
        val experimentsFile = File(filesDir, EXPERIMENTS_JSON_FILENAME)
        val experimentSource = KintoExperimentSource(
            EXPERIMENTS_BASE_URL,
            EXPERIMENTS_BUCKET_NAME,
            EXPERIMENTS_COLLECTION_NAME,
            components.core.client
        )
        // TODO add ValueProvider to keep clientID in sync with Glean when ready
        fretboard = Fretboard(experimentSource, FlatFileExperimentStorage(experimentsFile))
        fretboard.loadExperiments()
        Logger.debug("Bucket is ${fretboard.getUserBucket(this@FenixApplication)}")
        Logger.debug("Experiments active: ${fretboard.getExperimentsMap(this@FenixApplication)}")
        GlobalScope.launch(Dispatchers.IO) {
            fretboard.updateExperiments()
        }
    }

    private fun setupCrashReporting() {
        @Suppress("ConstantConditionIf")
        if (!BuildConfig.CRASH_REPORTING || BuildConfig.BUILD_TYPE != "release") {
            // Only enable crash reporting if this is a release build and if crash reporting was explicitly enabled
            // via a Gradle command line flag.
            return
        }

        components
            .analytics
            .crashReporter
            .install(this)
    }

    /**
     * Initiate Megazord sequence! Megazord Battle Mode!
     *
     * Mozilla Application Services publishes many native (Rust) code libraries that stand alone: each published Android
     * ARchive (AAR) contains managed code (classes.jar) and multiple .so library files (one for each supported
     * architecture). That means consuming multiple such libraries entails at least two .so libraries, and each of those
     * libraries includes the entire Rust standard library as well as (potentially many) duplicated dependencies. To
     * save space and allow cross-component native-code Link Time Optimization (LTO, i.e., inlining, dead code
     * elimination, etc).
     * Application Services also publishes composite libraries -- so called megazord libraries or just megazords -- that
     * compose multiple Rust components into a single optimized .so library file.
     *
     * @return Boolean indicating if we're in a megazord.
     */
    private fun setupMegazord(): Boolean {
        // mozilla.appservices.FenixMegazord will be missing if we're doing an application-services
        // dependency substitution locally. That class is supplied dynamically by the org.mozilla.appservices
        // gradle plugin, and that won't happen if we're not megazording. We won't megazord if we're
        // locally substituting every module that's part of the megazord's definition, which is what
        // happens during a local substitution of application-services.
        // As a workaround, use reflections to conditionally initialize the megazord in case it's present.
        return try {
            val megazordClass = Class.forName("mozilla.appservices.FenixMegazord")
            val megazordInitMethod = megazordClass.getDeclaredMethod("init")
            megazordInitMethod.invoke(megazordClass)
            true
        } catch (e: ClassNotFoundException) {
            Logger.info("mozilla.appservices.FenixMegazord not found; skipping megazord init.")
            false
        }
    }
}

/**
 * Are we running in the main process?
 *
 * Let's move this code to Android Components:
 * https://github.com/mozilla-mobile/android-components/issues/2207
 */
private fun isMainProcess(context: Context): Boolean {
    val pid = Process.myPid()

    val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE)
        as ActivityManager

    activityManager.runningAppProcesses?.forEach { processInfo ->
        if (processInfo.pid == pid) {
            return processInfo.processName == context.packageName
        }
    }

    return false
}
