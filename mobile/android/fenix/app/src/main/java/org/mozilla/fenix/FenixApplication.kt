/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.annotation.SuppressLint
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.StrictMode
import android.util.Log.INFO
import androidx.annotation.CallSuper
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.content.getSystemService
import androidx.lifecycle.ProcessLifecycleOwner
import androidx.work.Configuration.Builder
import androidx.work.Configuration.Provider
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.appservices.Megazord
import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.isUnsupported
import mozilla.components.concept.push.PushProcessor
import mozilla.components.feature.addons.migration.DefaultSupportedAddonsChecker
import mozilla.components.feature.addons.update.GlobalAddonDependencyProvider
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.net.ConceptFetchHttpUploader
import mozilla.components.support.base.facts.register
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.isMainProcess
import mozilla.components.support.ktx.android.content.runOnlyInMainProcess
import mozilla.components.support.locale.LocaleAwareApplication
import mozilla.components.support.rusthttp.RustHttpConfig
import mozilla.components.support.rustlog.RustLog
import mozilla.components.support.utils.logElapsedTime
import mozilla.components.support.webextensions.WebExtensionSupport
import org.mozilla.fenix.GleanMetrics.GleanBuildInfo
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.PerfStartup
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.metrics.MetricServiceType
import org.mozilla.fenix.components.metrics.SecurePrefsTelemetry
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.perf.ProfilerMarkerFactProcessor
import org.mozilla.fenix.perf.StartupTimeline
import org.mozilla.fenix.perf.StorageStatsMetrics
import org.mozilla.fenix.perf.runBlockingIncrement
import org.mozilla.fenix.push.PushFxaIntegration
import org.mozilla.fenix.push.WebPushEngineIntegration
import org.mozilla.fenix.session.PerformanceActivityLifecycleCallbacks
import org.mozilla.fenix.session.VisibilityLifecycleCallback
import org.mozilla.fenix.telemetry.TelemetryLifecycleObserver
import org.mozilla.fenix.utils.BrowsersCache
import java.util.concurrent.TimeUnit
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.autofill.AutofillUseCases
import mozilla.components.feature.search.ext.buildSearchUrl
import mozilla.components.feature.search.ext.waitForSelectedOrDefaultSearchEngine
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.GleanMetrics.AndroidAutofill
import org.mozilla.fenix.GleanMetrics.Preferences
import org.mozilla.fenix.GleanMetrics.SearchDefaultEngine
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MozillaProductDetector
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.utils.Settings

/**
 *The main application class for Fenix. Records data to measure initialization performance.
 *  Installs [CrashReporter], initializes [Glean]  in fenix builds and setup Megazord in the main process.
 */
@Suppress("Registered", "TooManyFunctions", "LargeClass")
open class FenixApplication : LocaleAwareApplication(), Provider {
    init {
        recordOnInit() // DO NOT MOVE ANYTHING ABOVE HERE: the timing of this measurement is critical.
    }

    private val logger = Logger("FenixApplication")

    open val components by lazy { Components(this) }

    var visibilityLifecycleCallback: VisibilityLifecycleCallback? = null
        private set

    override fun onCreate() {
        // We use start/stop instead of measure so we don't measure outside the main process.
        val completeMethodDurationTimerId = PerfStartup.applicationOnCreate.start() // DO NOT MOVE ANYTHING ABOVE HERE.

        super.onCreate()

        setupInAllProcesses()

        if (!isMainProcess()) {
            // If this is not the main process then do not continue with the initialization here. Everything that
            // follows only needs to be done in our app's main process and should not be done in other processes like
            // a GeckoView child process or the crash handling process. Most importantly we never want to end up in a
            // situation where we create a GeckoRuntime from the Gecko child process.
            return
        }

        if (Config.channel.isFenix) {
            // We need to always initialize Glean and do it early here.
            // Note that we are only initializing Glean here for "fenix" builds. "fennec" builds
            // will initialize in MigratingFenixApplication because we first need to migrate the
            // user's choice from Fennec.
            initializeGlean()
        }

        setupInMainProcessOnly()

        // DO NOT MOVE ANYTHING BELOW THIS stop CALL.
        PerfStartup.applicationOnCreate.stopAndAccumulate(completeMethodDurationTimerId)
    }

    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    protected open fun initializeGlean() {
        val telemetryEnabled = settings().isTelemetryEnabled

        logger.debug("Initializing Glean (uploadEnabled=$telemetryEnabled, isFennec=${Config.channel.isFennec})")

        Glean.initialize(
            applicationContext = this,
            configuration = Configuration(
                channel = BuildConfig.BUILD_TYPE,
                httpClient = ConceptFetchHttpUploader(
                    lazy(LazyThreadSafetyMode.NONE) { components.core.client }
                )
            ),
            uploadEnabled = telemetryEnabled,
            buildInfo = GleanBuildInfo.buildInfo
        )

        // We avoid blocking the main thread on startup by setting startup metrics on the background thread.
        val store = components.core.store
        GlobalScope.launch(Dispatchers.IO) {
            setStartupMetrics(store, settings())
        }
    }

    @CallSuper
    open fun setupInAllProcesses() {
        setupCrashReporting()

        // We want the log messages of all builds to go to Android logcat
        Log.addSink(FenixLogSink(logsDebug = Config.channel.isDebug))
    }

    @CallSuper
    open fun setupInMainProcessOnly() {
        ProfilerMarkerFactProcessor.create { components.core.engine.profiler }.register()

        run {
            // Attention: Do not invoke any code from a-s in this scope.
            val megazordSetup = setupMegazord()

            setDayNightTheme()
            components.strictMode.enableStrictMode(true)
            warmBrowsersCache()

            // Make sure the engine is initialized and ready to use.
            components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
                components.core.engine.warmUp()
            }
            initializeWebExtensionSupport()
            restoreBrowserState()
            restoreDownloads()

            // Just to make sure it is impossible for any application-services pieces
            // to invoke parts of itself that require complete megazord initialization
            // before that process completes, we wait here, if necessary.
            if (!megazordSetup.isCompleted) {
                runBlockingIncrement { megazordSetup.await() }
            }
        }

        setupLeakCanary()
        startMetricsIfEnabled()
        setupPush()

        visibilityLifecycleCallback = VisibilityLifecycleCallback(getSystemService())
        registerActivityLifecycleCallbacks(visibilityLifecycleCallback)

        // Storage maintenance disabled, for now, as it was interfering with background migrations.
        // See https://github.com/mozilla-mobile/fenix/issues/7227 for context.
        // if ((System.currentTimeMillis() - settings().lastPlacesStorageMaintenance) > ONE_DAY_MILLIS) {
        //    runStorageMaintenance()
        // }

        components.appStartReasonProvider.registerInAppOnCreate(this)
        components.startupActivityLog.registerInAppOnCreate(this)
        initVisualCompletenessQueueAndQueueTasks()

        ProcessLifecycleOwner.get().lifecycle.addObserver(TelemetryLifecycleObserver(components.core.store))
    }

    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    private fun restoreBrowserState() = GlobalScope.launch(Dispatchers.Main) {
        val store = components.core.store
        val sessionStorage = components.core.sessionStorage

        components.useCases.tabsUseCases.restore(sessionStorage, settings().getTabTimeout())

        // Now that we have restored our previous state (if there's one) let's setup auto saving the state while
        // the app is used.
        sessionStorage.autoSave(store)
            .periodicallyInForeground(interval = 30, unit = TimeUnit.SECONDS)
            .whenGoingToBackground()
            .whenSessionsChange()
    }

    private fun restoreDownloads() {
        components.useCases.downloadUseCases.restoreDownloads()
    }

    private fun initVisualCompletenessQueueAndQueueTasks() {
        val queue = components.performance.visualCompletenessQueue.queue

        fun initQueue() {
            registerActivityLifecycleCallbacks(PerformanceActivityLifecycleCallbacks(queue))
        }

        @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
        fun queueInitStorageAndServices() {
            components.performance.visualCompletenessQueue.queue.runIfReadyOrQueue {
                GlobalScope.launch(Dispatchers.IO) {
                    logger.info("Running post-visual completeness tasks...")
                    logElapsedTime(logger, "Storage initialization") {
                        components.core.historyStorage.warmUp()
                        components.core.bookmarksStorage.warmUp()
                        components.core.passwordsStorage.warmUp()
                        components.core.autofillStorage.warmUp()
                    }

                    SecurePrefsTelemetry(this@FenixApplication, components.analytics.experiments).startTests()
                }
                // Account manager initialization needs to happen on the main thread.
                GlobalScope.launch(Dispatchers.Main) {
                    logElapsedTime(logger, "Kicking-off account manager") {
                        components.backgroundServices.accountManager
                    }
                }
            }
        }

        fun queueMetrics() {
            if (SDK_INT >= Build.VERSION_CODES.O) { // required by StorageStatsMetrics.
                queue.runIfReadyOrQueue {
                    // Because it may be slow to capture the storage stats, it might be preferred to
                    // create a WorkManager task for this metric, however, I ran out of
                    // implementation time and WorkManager is harder to test.
                    StorageStatsMetrics.report(this.applicationContext)
                }
            }
        }

        @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
        fun queueReviewPrompt() {
            GlobalScope.launch(Dispatchers.IO) {
                components.reviewPromptController.trackApplicationLaunch()
            }
        }

        @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
        fun queueRestoreLocale() {
            components.performance.visualCompletenessQueue.queue.runIfReadyOrQueue {
                GlobalScope.launch(Dispatchers.IO) {
                    components.useCases.localeUseCases.restore()
                }
            }
        }

        initQueue()

        // We init these items in the visual completeness queue to avoid them initing in the critical
        // startup path, before the UI finishes drawing (i.e. visual completeness).
        queueInitStorageAndServices()
        queueMetrics()
        queueReviewPrompt()
        queueRestoreLocale()
    }

    private fun startMetricsIfEnabled() {
        if (settings().isTelemetryEnabled) {
            components.analytics.metrics.start(MetricServiceType.Data)
        }

        if (settings().isMarketingTelemetryEnabled) {
            components.analytics.metrics.start(MetricServiceType.Marketing)
        }
    }

    // See https://github.com/mozilla-mobile/fenix/issues/7227 for context.
    // To re-enable this, we need to do so in a way that won't interfere with any startup operations
    // which acquire reserved+ sqlite lock. Currently, Fennec migrations need to write to storage
    // on startup, and since they run in a background service we can't simply order these operations.

    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
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
        components.push.feature?.let {
            Logger.info("AutoPushFeature is configured, initializing it...")

            // Install the AutoPush singleton to receive messages.
            PushProcessor.install(it)

            WebPushEngineIntegration(components.core.engine, it).start()

            // Perform a one-time initialization of the account manager if a message is received.
            PushFxaIntegration(it, lazy { components.backgroundServices.accountManager }).launch()

            // Initialize the service. This could potentially be done in a coroutine in the future.
            it.initialize()
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
     * The application-services combined libraries are known as the "megazord". We use the default `full`
     * megazord - it contains everything that fenix needs, and (currently) nothing more.
     *
     * Documentation on what megazords are, and why they're needed:
     * - https://github.com/mozilla/application-services/blob/master/docs/design/megazords.md
     * - https://mozilla.github.io/application-services/docs/applications/consuming-megazord-libraries.html
     */
    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    private fun setupMegazord(): Deferred<Unit> {
        // Note: Megazord.init() must be called as soon as possible ...
        Megazord.init()

        return GlobalScope.async(Dispatchers.IO) {
            // ... but RustHttpConfig.setClient() and RustLog.enable() can be called later.
            RustHttpConfig.setClient(lazy { components.core.client })
            RustLog.enable(components.analytics.crashReporter)
            // We want to ensure Nimbus is initialized as early as possible so we can
            // experiment on features close to startup.
            // But we need viaduct (the RustHttp client) to be ready before we do.
            components.analytics.experiments.initialize()
        }
    }

    override fun onTrimMemory(level: Int) {
        super.onTrimMemory(level)

        // Additional logging and breadcrumb to debug memory issues:
        // https://github.com/mozilla-mobile/fenix/issues/12731

        logger.info("onTrimMemory(), level=$level, main=${isMainProcess()}")

        components.analytics.crashReporter.recordCrashBreadcrumb(
            Breadcrumb(
                category = "Memory",
                message = "onTrimMemory()",
                data = mapOf(
                    "level" to level.toString(),
                    "main" to isMainProcess().toString()
                ),
                level = Breadcrumb.Level.INFO
            )
        )

        runOnlyInMainProcess {
            components.core.icons.onTrimMemory(level)
            components.core.store.dispatch(SystemAction.LowMemoryAction(level))
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

    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    private fun warmBrowsersCache() {
        // We avoid blocking the main thread for BrowsersCache on startup by loading it on
        // background thread.
        GlobalScope.launch(Dispatchers.Default) {
            BrowsersCache.all(this@FenixApplication)
        }
    }

    private fun initializeWebExtensionSupport() {
        try {
            GlobalAddonDependencyProvider.initialize(
                components.addonManager,
                components.addonUpdater,
                onCrash = { exception ->
                    components.analytics.crashReporter.submitCaughtException(exception)
                }
            )
            WebExtensionSupport.initialize(
                components.core.engine,
                components.core.store,
                onNewTabOverride = {
                    _, engineSession, url ->
                    val shouldCreatePrivateSession =
                        components.core.store.state.selectedTab?.content?.private
                            ?: components.settings.openLinksInAPrivateTab

                    components.useCases.tabsUseCases.addTab(
                        url = url,
                        selectTab = true,
                        engineSession = engineSession,
                        private = shouldCreatePrivateSession
                    )
                },
                onCloseTabOverride = {
                    _, sessionId ->
                    components.useCases.tabsUseCases.removeTab(sessionId)
                },
                onSelectTabOverride = {
                    _, sessionId ->
                    components.useCases.tabsUseCases.selectTab(sessionId)
                },
                onExtensionsLoaded = { extensions ->
                    components.addonUpdater.registerForFutureUpdates(extensions)
                    subscribeForNewAddonsIfNeeded(components.supportedAddonsChecker, extensions)
                },
                onUpdatePermissionRequest = components.addonUpdater::onUpdatePermissionRequest
            )
        } catch (e: UnsupportedOperationException) {
            Logger.error("Failed to initialize web extension support", e)
        }
    }

    @VisibleForTesting
    internal fun subscribeForNewAddonsIfNeeded(
        checker: DefaultSupportedAddonsChecker,
        installedExtensions: List<WebExtension>
    ) {
        val hasUnsupportedAddons = installedExtensions.any { it.isUnsupported() }
        if (hasUnsupportedAddons) {
            checker.registerForChecks()
        } else {
            // As checks are a persistent subscriptions, we have to make sure
            // we remove any previous subscriptions.
            checker.unregisterForChecks()
        }
    }

    /**
     * This function is called right after Glean is initialized. Part of this function depends on
     * shared preferences to be updated so the correct value is sent with the metrics ping.
     *
     * The reason we're using shared preferences to track these values is due to the limitations of
     * the current metrics ping design. The values set here will be sent in every metrics ping even
     * if these values have not changed since the last startup.
     */
    @Suppress("ComplexMethod", "LongMethod")
    @VisibleForTesting
    internal fun setStartupMetrics(
        browserStore: BrowserStore,
        settings: Settings,
        browsersCache: BrowsersCache = BrowsersCache,
        mozillaProductDetector: MozillaProductDetector = MozillaProductDetector
    ) {
        setPreferenceMetrics(settings)
        with(Metrics) {
            // Set this early to guarantee it's in every ping from here on.
            distributionId.set(
                when (Config.channel.isMozillaOnline) {
                    true -> "MozillaOnline"
                    false -> "Mozilla"
                }
            )

            defaultBrowser.set(browsersCache.all(applicationContext).isDefaultBrowser)
            mozillaProductDetector.getMozillaBrowserDefault(applicationContext)?.also {
                defaultMozBrowser.set(it)
            }

            mozillaProducts.set(mozillaProductDetector.getInstalledMozillaProducts(applicationContext))

            adjustCampaign.set(settings.adjustCampaignId)
            adjustAdGroup.set(settings.adjustAdGroup)
            adjustCreative.set(settings.adjustCreative)
            adjustNetwork.set(settings.adjustNetwork)

            searchWidgetInstalled.set(settings.searchWidgetInstalled)

            val openTabsCount = settings.openTabsCount
            hasOpenTabs.set(openTabsCount > 0)
            if (openTabsCount > 0) {
                tabsOpenCount.add(openTabsCount)
            }

            val topSitesSize = settings.topSitesSize
            hasTopSites.set(topSitesSize > 0)
            if (topSitesSize > 0) {
                topSitesCount.add(topSitesSize)
            }

            val installedAddonSize = settings.installedAddonsCount
            Addons.hasInstalledAddons.set(installedAddonSize > 0)
            if (installedAddonSize > 0) {
                Addons.installedAddons.set(settings.installedAddonsList.split(','))
            }

            val enabledAddonSize = settings.enabledAddonsCount
            Addons.hasEnabledAddons.set(enabledAddonSize > 0)
            if (enabledAddonSize > 0) {
                Addons.enabledAddons.set(settings.enabledAddonsList.split(','))
            }

            val desktopBookmarksSize = settings.desktopBookmarksSize
            hasDesktopBookmarks.set(desktopBookmarksSize > 0)
            if (desktopBookmarksSize > 0) {
                desktopBookmarksCount.add(desktopBookmarksSize)
            }

            val mobileBookmarksSize = settings.mobileBookmarksSize
            hasMobileBookmarks.set(mobileBookmarksSize > 0)
            if (mobileBookmarksSize > 0) {
                mobileBookmarksCount.add(mobileBookmarksSize)
            }

            toolbarPosition.set(
                when (settings.toolbarPosition) {
                    ToolbarPosition.BOTTOM -> Event.ToolbarPositionChanged.Position.BOTTOM.name
                    ToolbarPosition.TOP -> Event.ToolbarPositionChanged.Position.TOP.name
                }
            )

            tabViewSetting.set(settings.getTabViewPingString())
            closeTabSetting.set(settings.getTabTimeoutPingString())
        }

        with(AndroidAutofill) {
            val autofillUseCases = AutofillUseCases()
            supported.set(autofillUseCases.isSupported(applicationContext))
            enabled.set(autofillUseCases.isEnabled(applicationContext))
        }

        browserStore.waitForSelectedOrDefaultSearchEngine { searchEngine ->
            if (searchEngine != null) {
                SearchDefaultEngine.apply {
                    code.set(searchEngine.id)
                    name.set(searchEngine.name)
                    submissionUrl.set(searchEngine.buildSearchUrl(""))
                }
            }
        }
    }

    @Suppress("ComplexMethod")
    private fun setPreferenceMetrics(
        settings: Settings
    ) {
        with(Preferences) {
            searchSuggestionsEnabled.set(settings.shouldShowSearchSuggestions)
            remoteDebuggingEnabled.set(settings.isRemoteDebuggingEnabled)
            telemetryEnabled.set(settings.isTelemetryEnabled)
            browsingHistorySuggestion.set(settings.shouldShowHistorySuggestions)
            bookmarksSuggestion.set(settings.shouldShowBookmarkSuggestions)
            clipboardSuggestionsEnabled.set(settings.shouldShowClipboardSuggestions)
            searchShortcutsEnabled.set(settings.shouldShowSearchShortcuts)
            openLinksInPrivate.set(settings.openLinksInAPrivateTab)
            privateSearchSuggestions.set(settings.shouldShowSearchSuggestionsInPrivate)
            voiceSearchEnabled.set(settings.shouldShowVoiceSearch)
            openLinksInAppEnabled.set(settings.openLinksInExternalApp)
            signedInSync.set(settings.signedInFxaAccount)

            val syncedItems = SyncEnginesStorage(applicationContext).getStatus().entries.filter {
                it.value
            }.map { it.key.nativeName }
            syncItems.set(syncedItems)

            toolbarPositionSetting.set(
                when {
                    settings.shouldUseFixedTopToolbar -> "fixed_top"
                    settings.shouldUseBottomToolbar -> "bottom"
                    else -> "top"
                }
            )

            enhancedTrackingProtection.set(
                when {
                    !settings.shouldUseTrackingProtection -> ""
                    settings.useStandardTrackingProtection -> "standard"
                    settings.useStrictTrackingProtection -> "strict"
                    settings.useCustomTrackingProtection -> "custom"
                    else -> ""
                }
            )

            val accessibilitySelection = mutableListOf<String>()

            if (settings.switchServiceIsEnabled) {
                accessibilitySelection.add("switch")
            }

            if (settings.touchExplorationIsEnabled) {
                accessibilitySelection.add("touch exploration")
            }

            accessibilityServices.set(accessibilitySelection.toList())

            userTheme.set(
                when {
                    settings.shouldUseLightTheme -> "light"
                    settings.shouldUseDarkTheme -> "dark"
                    settings.shouldFollowDeviceTheme -> "system"
                    settings.shouldUseAutoBatteryTheme -> "battery"
                    else -> ""
                }
            )
        }
    }

    protected fun recordOnInit() {
        // This gets called by more than one process. Ideally we'd only run this in the main process
        // but the code to check which process we're in crashes because the Context isn't valid yet.
        //
        // This method is not covered by our internal crash reporting: be very careful when modifying it.
        StartupTimeline.onApplicationInit() // DO NOT MOVE ANYTHING ABOVE HERE: the timing is critical.
    }

    override fun onConfigurationChanged(config: android.content.res.Configuration) {
        // Workaround for androidx appcompat issue where follow system day/night mode config changes
        // are not triggered when also using createConfigurationContext like we do in LocaleManager
        // https://issuetracker.google.com/issues/143570309#comment3
        applicationContext.resources.configuration.uiMode = config.uiMode

        if (isMainProcess()) {
            // We can only do this on the main process as resetAfter will access components.core, which
            // will initialize the engine and create an additional GeckoRuntime from the Gecko
            // child process, causing a crash.

            // There's a strict mode violation in A-Cs LocaleAwareApplication which
            // reads from shared prefs: https://github.com/mozilla-mobile/android-components/issues/8816
            components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
                super.onConfigurationChanged(config)
            }
        } else {
            super.onConfigurationChanged(config)
        }
    }

    override fun getWorkManagerConfiguration() = Builder().setMinimumLoggingLevel(INFO).build()
}
