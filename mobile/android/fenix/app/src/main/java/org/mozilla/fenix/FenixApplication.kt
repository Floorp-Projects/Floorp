/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.annotation.SuppressLint
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.StrictMode
import androidx.annotation.CallSuper
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.content.getSystemService
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.appservices.Megazord
import mozilla.components.concept.push.PushProcessor
import mozilla.components.service.experiments.Experiments
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.net.ConceptFetchHttpUploader
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.ktx.android.content.isMainProcess
import mozilla.components.support.ktx.android.content.runOnlyInMainProcess
import mozilla.components.support.locale.LocaleAwareApplication
import mozilla.components.support.rusthttp.RustHttpConfig
import mozilla.components.support.rustlog.RustLog
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.session.NotificationSessionObserver
import org.mozilla.fenix.session.VisibilityLifecycleCallback

@SuppressLint("Registered")
@Suppress("TooManyFunctions")
open class FenixApplication : LocaleAwareApplication() {
    private val logger = Logger("FenixApplication")

    open val components by lazy { Components(this) }

    var visibilityLifecycleCallback: VisibilityLifecycleCallback? = null
        private set

    override fun onCreate() {
        super.onCreate()

        setupInAllProcesses()

        if (!isMainProcess()) {
            // If this is not the main process then do not continue with the initialization here. Everything that
            // follows only needs to be done in our app's main process and should not be done in other processes like
            // a GeckoView child process or the crash handling process. Most importantly we never want to end up in a
            // situation where we create a GeckoRuntime from the Gecko child process.
            return
        }

        // We need to always initialize Glean and do it early here.
        // It is important that this initialization happens *here* before calling into
        // setupInMainProcessOnly() which behaves differently for fenix and fennec builds.
        val enableGlean = if (Config.channel.isFennec) {
            // We are disabling Glean here because for Fennec builds we may not know yet whether
            // we can enable telemetry yet. We first need to migrate the setting from Fennec to
            // know the user's choice. The blocking migration in `MigratingFenixApplication` will
            // notify glean once the value has been migrated.
            false
        } else {
            // We initialize Glean with telemetry enabled (or disabled) early here so that we do not
            // end up loosing data for components that collect telemetry very early.
            settings().isTelemetryEnabled
        }
        logger.debug("Initializing Glean (uploadEnabled=$enableGlean, isFennec=${Config.channel.isFennec})")
        Glean.initialize(
            applicationContext = this,
            configuration = Configuration(
                channel = BuildConfig.BUILD_TYPE,
                httpClient = ConceptFetchHttpUploader(
                    lazy(LazyThreadSafetyMode.NONE) { components.core.client }
                )),
            uploadEnabled = enableGlean
        )

        setupInMainProcessOnly()
    }

    @CallSuper
    open fun setupInAllProcesses() {
        setupCrashReporting()

        // We want the log messages of all builds to go to Android logcat
        Log.addSink(AndroidLogSink())
    }

    @CallSuper
    open fun setupInMainProcessOnly() {
        run {
            // Attention: Do not invoke any code from a-s in this scope.
            val megazordSetup = setupMegazord()

            setDayNightTheme()
            enableStrictMode()

            // Enable the service-experiments component
            if (settings().isExperimentationEnabled && Config.channel.isReleaseOrBeta) {
                Experiments.initialize(
                    applicationContext,
                    mozilla.components.service.experiments.Configuration(
                        httpClient = lazy(LazyThreadSafetyMode.NONE) { components.core.client }
                    )
                )
            }

            // Make sure the engine is initialized and ready to use.
            components.core.engine.warmUp()

            // Just to make sure it is impossible for any application-services pieces
            // to invoke parts of itself that require complete megazord initialization
            // before that process completes, we wait here, if necessary.
            if (!megazordSetup.isCompleted) {
                runBlocking { megazordSetup.await(); }
            }
        }

        setupLeakCanary()
        if (settings().isTelemetryEnabled) {
            components.analytics.metrics.start()
        }

        setupPush()

        visibilityLifecycleCallback = VisibilityLifecycleCallback(getSystemService())
        registerActivityLifecycleCallbacks(visibilityLifecycleCallback)

        components.core.sessionManager.register(NotificationSessionObserver(this))

        // Storage maintenance disabled, for now, as it was interfering with background migrations.
        // See https://github.com/mozilla-mobile/fenix/issues/7227 for context.
        // if ((System.currentTimeMillis() - settings().lastPlacesStorageMaintenance) > ONE_DAY_MILLIS) {
        //    runStorageMaintenance()
        // }
    }

    // See https://github.com/mozilla-mobile/fenix/issues/7227 for context.
    // To re-enable this, we need to do so in a way that won't interfere with any startup operations
    // which acquire reserved+ sqlite lock. Currently, Fennec migrations need to write to storage
    // on startup, and since they run in a background service we can't simply order these operations.
    private fun runStorageMaintenance() {
        GlobalScope.launch(Dispatchers.IO) {
            // Bookmarks and history storage sit on top of the same db file so we only need to
            // run maintenance on one - arbitrarily using bookmarks.
            components.core.bookmarksStorage.runMaintenance()
        }
        settings().lastPlacesStorageMaintenance = System.currentTimeMillis()
    }

    protected open fun setupLeakCanary() {
        // no-op, LeakCanary is disabled by default
    }

    open fun updateLeakCanaryState(isEnabled: Boolean) {
        // no-op, LeakCanary is disabled by default
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
    private fun setupMegazord(): Deferred<Unit> {
        // Note: Megazord.init() must be called as soon as possible ...
        Megazord.init()

        return GlobalScope.async(Dispatchers.IO) {
            // ... but RustHttpConfig.setClient() and RustLog.enable() can be called later.
            RustHttpConfig.setClient(lazy { components.core.client })
            RustLog.enable()
        }
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
        if (Config.channel.isDebug) {
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
}
