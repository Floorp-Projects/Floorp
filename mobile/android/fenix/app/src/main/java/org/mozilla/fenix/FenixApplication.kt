/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.annotation.SuppressLint
import android.app.Application
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.StrictMode
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.content.getSystemService
import io.reactivex.plugins.RxJavaPlugins
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.appservices.Megazord
import mozilla.components.concept.push.PushProcessor
import mozilla.components.service.experiments.Experiments
import mozilla.components.service.fretboard.Fretboard
import mozilla.components.service.fretboard.source.kinto.KintoExperimentSource
import mozilla.components.service.fretboard.storage.flatfile.FlatFileExperimentStorage
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.ktx.android.content.isMainProcess
import mozilla.components.support.ktx.android.content.runOnlyInMainProcess
import mozilla.components.support.rusthttp.RustHttpConfig
import mozilla.components.support.rustlog.RustLog
import org.mozilla.fenix.GleanMetrics.ExperimentsMetrics
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.session.NotificationSessionObserver
import org.mozilla.fenix.session.VisibilityLifecycleCallback
import java.io.File

@SuppressLint("Registered")
@Suppress("TooManyFunctions")
open class FenixApplication : Application() {
    lateinit var fretboard: Fretboard
    lateinit var experimentLoader: Deferred<Boolean>
    var experimentLoaderComplete: Boolean = false

    open val components by lazy { Components(this) }

    var visibilityLifecycleCallback: VisibilityLifecycleCallback? = null
        private set

    override fun onCreate() {
        super.onCreate()

        setupApplication()
    }

    open fun setupApplication() {
        setupCrashReporting()
        setDayNightTheme()

        setupMegazord()
        setupLogging()
        registerRxExceptionHandling()
        enableStrictMode()

        applicationContext.settings().totalUriCount = 0

        if (!isMainProcess()) {
            // If this is not the main process then do not continue with the initialization here. Everything that
            // follows only needs to be done in our app's main process and should not be done in other processes like
            // a GeckoView child process or the crash handling process. Most importantly we never want to end up in a
            // situation where we create a GeckoRuntime from the Gecko child process.
            return
        }

        // Make sure the engine is initialized and ready to use.
        components.core.engine.warmUp()

        // We want to call this function as early as possible, but only once and
        // on the main process, as it uses Gecko to fetch experiments from the server.
        experimentLoader = loadExperiments()

        // Enable the service-experiments component
        if (settings().isExperimentationEnabled) {
            Experiments.initialize(
                applicationContext,
                mozilla.components.service.experiments.Configuration(
                    httpClient = lazy(LazyThreadSafetyMode.NONE) { components.core.client }
                )
            )
        } else {
            // We should make a better way to opt out for when we have more experiments
            // See https://github.com/mozilla-mobile/fenix/issues/6278
            ExperimentsManager.optOutEtpExperiment(this)
        }

        // When the `fenix-test-2019-08-05` experiment is active, record its branch in Glean
        // telemetry. This will be used to validate that the experiment system correctly enrolls
        // clients and segments them into branches. Note that this will not take effect the first
        // time the application has launched, since there won't be enough time for the experiments
        // library to get a list of experiments. It will take effect the second time the
        // application is launched.
        Experiments.withExperiment("fenix-test-2019-08-05") { branchName ->
            ExperimentsMetrics.activeExperiment.set(branchName)
        }

        ExperimentsManager.initEtpExperiment(this)

        setupLeakCanary()
        if (settings().isTelemetryEnabled) {
            components.analytics.metrics.start()
        }

        setupPush()

        visibilityLifecycleCallback = VisibilityLifecycleCallback(getSystemService())
        registerActivityLifecycleCallbacks(visibilityLifecycleCallback)

        components.core.sessionManager.register(NotificationSessionObserver(this))

        if ((System.currentTimeMillis() - settings().lastPlacesStorageMaintenance) > ONE_DAY_MILLIS) {
            runStorageMaintenance()
        }
    }

    private fun runStorageMaintenance() {
        GlobalScope.launch(Dispatchers.IO) {
            // Bookmarks and history storage sit on top of the same db file so we only need to
            // run maintenance on one - arbitrarily using bookmarks.
            components.core.bookmarksStorage.runMaintenance()
        }
        settings().lastPlacesStorageMaintenance = System.currentTimeMillis()
    }

    private fun registerRxExceptionHandling() {
        RxJavaPlugins.setErrorHandler {
            throw it.cause ?: it
        }
    }

    protected open fun setupLeakCanary() {
        // no-op, LeakCanary is disabled by default
    }

    open fun updateLeakCanaryState(isEnabled: Boolean) {
        // no-op, LeakCanary is disabled by default
    }

    private fun setupLogging() {
        // We want the log messages of all builds to go to Android logcat
        Log.addSink(AndroidLogSink())
        // We want rust logging to go through the log sinks.
        // This has to happen after initializing the megazord.
        RustLog.enable()
    }

    private fun loadExperiments(): Deferred<Boolean> {
        val experimentsFile = File(filesDir, EXPERIMENTS_JSON_FILENAME)
        val experimentSource = KintoExperimentSource(
            EXPERIMENTS_BASE_URL,
            EXPERIMENTS_BUCKET_NAME,
            EXPERIMENTS_COLLECTION_NAME,
            components.core.client
        )
        // TODO add ValueProvider to keep clientID in sync with Glean when ready
        fretboard = Fretboard(experimentSource, FlatFileExperimentStorage(experimentsFile))

        return GlobalScope.async(Dispatchers.IO) {
            fretboard.loadExperiments()
            Logger.debug("Bucket is ${fretboard.getUserBucket(this@FenixApplication)}")
            Logger.debug("Experiments active: ${fretboard.getExperimentsMap(this@FenixApplication)}")
            fretboard.updateExperiments()
            return@async true
        }
    }

    private fun setupPush() {
        // Sets the PushFeature as the singleton instance for push messages to go to.
        // We need the push feature setup here to deliver messages in the case where the service
        // starts up the app first.
        components.backgroundServices.push?.let { autoPushFeature ->
            Logger.info("AutoPushFeature is configured, initializing it...")

            // Install the AutoPush singleton to receive messages.
            PushProcessor.install(autoPushFeature)

            // Initialize the service. This could potentially be done in a coroutine in the future.
            autoPushFeature.initialize()
        }
    }

    private fun setupCrashReporting() {
        components
            .analytics
            .crashReporter
            .install(this)
    }

    /**
     * Initiate Megazord sequence! Megazord Battle Mode!
     *
     * The application-services combined libraries are known as the "megazord". The default megazord
     * contains several features that fenix doesn't need, and so we swap out with a customized fenix-specific
     * version of the megazord. The best explanation for what this is, and why it's done is the a-s
     * documentation on the topic:
     * - https://github.com/mozilla/application-services/blob/master/docs/design/megazords.md
     * - https://mozilla.github.io/application-services/docs/applications/consuming-megazord-libraries.html
     */
    private fun setupMegazord() {
        // Note: This must be called as soon as possible
        Megazord.init()
        // This (and enabling RustLog) may be delayed if needed for performance reasons
        RustHttpConfig.setClient(lazy { components.core.client })
    }

    override fun onTrimMemory(level: Int) {
        super.onTrimMemory(level)
        runOnlyInMainProcess {
            components.core.sessionManager.onLowMemory()
        }
    }

    @SuppressLint("WrongConstant")
    // Suppressing erroneous lint warning about using MODE_NIGHT_AUTO_BATTERY, a likely library bug
    private fun setDayNightTheme() {
        val settings = this.settings()
        when {
            settings.shouldUseLightTheme -> {
                AppCompatDelegate.setDefaultNightMode(
                    AppCompatDelegate.MODE_NIGHT_NO
                )
            }
            settings.shouldUseDarkTheme -> {
                AppCompatDelegate.setDefaultNightMode(
                    AppCompatDelegate.MODE_NIGHT_YES
                )
            }
            SDK_INT < Build.VERSION_CODES.P && settings.shouldUseAutoBatteryTheme -> {
                AppCompatDelegate.setDefaultNightMode(
                    AppCompatDelegate.MODE_NIGHT_AUTO_BATTERY
                )
            }
            SDK_INT >= Build.VERSION_CODES.P && settings.shouldFollowDeviceTheme -> {
                AppCompatDelegate.setDefaultNightMode(
                    AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
                )
            }
            // First run of app no default set, set the default to Follow System for 28+ and Normal Mode otherwise
            else -> {
                if (SDK_INT >= Build.VERSION_CODES.P) {
                    AppCompatDelegate.setDefaultNightMode(
                        AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
                    )
                    settings.shouldFollowDeviceTheme = true
                } else {
                    AppCompatDelegate.setDefaultNightMode(
                        AppCompatDelegate.MODE_NIGHT_NO
                    )
                    settings.shouldUseLightTheme = true
                }
            }
        }
    }

    private fun enableStrictMode() {
        if (BuildConfig.DEBUG) {
            StrictMode.setThreadPolicy(
                StrictMode.ThreadPolicy.Builder()
                    .detectAll()
                    .penaltyLog()
                    .build()
            )
            var builder = StrictMode.VmPolicy.Builder()
                .detectLeakedSqlLiteObjects()
                .detectLeakedClosableObjects()
                .detectLeakedRegistrationObjects()
                .detectActivityLeaks()
                .detectFileUriExposure()
                .penaltyLog()
            if (SDK_INT >= Build.VERSION_CODES.O) builder =
                builder.detectContentUriWithoutPermission()
            if (SDK_INT >= Build.VERSION_CODES.P) builder = builder.detectNonSdkApiUsage()
            StrictMode.setVmPolicy(builder.build())
        }
    }

    companion object {
        private const val ONE_DAY_MILLIS = 24 * 60 * 60 * 1000
    }
}
