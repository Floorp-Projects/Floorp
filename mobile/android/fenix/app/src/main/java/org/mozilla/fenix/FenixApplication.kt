/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Application
import android.content.Context
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
import org.mozilla.fenix.components.Components
import java.io.File

class FenixApplication : Application() {
    lateinit var fretboard: Fretboard

    val components by lazy { Components(this) }

    override fun onCreate() {
        super.onCreate()
        Log.addSink(AndroidLogSink())

        setupCrashReporting()
        setupGlean(this)
        loadExperiments()
    }

    private fun setupGlean(context: Context) {
        Glean.initialize(context)
        Glean.setUploadEnabled(BuildConfig.TELEMETRY)
    }

    private fun loadExperiments() {
        val experimentsFile = File(filesDir, EXPERIMENTS_JSON_FILENAME)
        val experimentSource = KintoExperimentSource(
            EXPERIMENTS_BASE_URL, EXPERIMENTS_BUCKET_NAME, EXPERIMENTS_COLLECTION_NAME
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
}
