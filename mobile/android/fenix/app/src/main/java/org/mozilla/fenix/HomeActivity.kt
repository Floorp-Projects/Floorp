/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_MAIN
import android.content.Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
import android.content.res.Configuration
import android.os.Build
import android.os.Bundle
import android.os.StrictMode
import android.os.SystemClock
import android.text.TextUtils
import android.text.format.DateUtils
import android.util.AttributeSet
import android.view.ActionMode
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.view.WindowManager.LayoutParams.FLAG_SECURE
import androidx.annotation.CallSuper
import androidx.annotation.IdRes
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PROTECTED
import androidx.appcompat.app.ActionBar
import androidx.appcompat.widget.Toolbar
import androidx.core.app.NotificationManagerCompat
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.NavigationUI
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.feature.contextmenu.DefaultSelectionActionDelegate
import mozilla.components.feature.media.ext.findActiveMediaTab
import mozilla.components.feature.privatemode.notification.PrivateNotificationFeature
import mozilla.components.feature.search.BrowserStoreSearchAdapter
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.base.feature.ActivityResultHandler
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import mozilla.components.support.ktx.android.content.call
import mozilla.components.support.ktx.android.content.email
import mozilla.components.support.ktx.android.content.share
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import mozilla.components.support.locale.LocaleAwareAppCompatActivity
import mozilla.components.support.utils.ManufacturerCodes
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.toSafeIntent
import mozilla.components.support.webextensions.WebExtensionPopupFeature
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.StartOnHome
import org.mozilla.fenix.addons.AddonDetailsFragmentDirections
import org.mozilla.fenix.addons.AddonPermissionsDetailsFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.DefaultBrowsingModeManager
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.metrics.BreadcrumbsRecorder
import org.mozilla.fenix.databinding.ActivityHomeBinding
import org.mozilla.fenix.exceptions.trackingprotection.TrackingProtectionExceptionsFragmentDirections
import org.mozilla.fenix.ext.alreadyOnDestination
import org.mozilla.fenix.ext.areNotificationsEnabledSafe
import org.mozilla.fenix.ext.breadcrumb
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.hasTopDestination
import org.mozilla.fenix.ext.isNotificationChannelEnabled
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.setNavigationIcon
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.gleanplumb.MessageNotificationWorker
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.intent.AssistIntentProcessor
import org.mozilla.fenix.home.intent.CrashReporterIntentProcessor
import org.mozilla.fenix.home.intent.DefaultBrowserIntentProcessor
import org.mozilla.fenix.home.intent.HomeDeepLinkIntentProcessor
import org.mozilla.fenix.home.intent.OpenBrowserIntentProcessor
import org.mozilla.fenix.home.intent.OpenSpecificTabIntentProcessor
import org.mozilla.fenix.home.intent.SpeechProcessingIntentProcessor
import org.mozilla.fenix.home.intent.StartSearchIntentProcessor
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentDirections
import org.mozilla.fenix.library.bookmarks.DesktopFolders
import org.mozilla.fenix.library.history.HistoryFragmentDirections
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentDirections
import org.mozilla.fenix.library.recentlyclosed.RecentlyClosedFragmentDirections
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.onboarding.DefaultBrowserNotificationWorker
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.onboarding.MARKETING_CHANNEL_ID
import org.mozilla.fenix.onboarding.ReEngagementNotificationWorker
import org.mozilla.fenix.onboarding.ensureMarketingChannelExists
import org.mozilla.fenix.perf.MarkersActivityLifecycleCallbacks
import org.mozilla.fenix.perf.MarkersFragmentLifecycleCallbacks
import org.mozilla.fenix.perf.Performance
import org.mozilla.fenix.perf.PerformanceInflater
import org.mozilla.fenix.perf.ProfilerMarkers
import org.mozilla.fenix.perf.StartupPathProvider
import org.mozilla.fenix.perf.StartupTimeline
import org.mozilla.fenix.perf.StartupTypeTelemetry
import org.mozilla.fenix.search.SearchDialogFragmentDirections
import org.mozilla.fenix.session.PrivateNotificationService
import org.mozilla.fenix.settings.CookieBannersFragmentDirections
import org.mozilla.fenix.settings.HttpsOnlyFragmentDirections
import org.mozilla.fenix.settings.SettingsFragmentDirections
import org.mozilla.fenix.settings.TrackingProtectionFragmentDirections
import org.mozilla.fenix.settings.about.AboutFragmentDirections
import org.mozilla.fenix.settings.logins.fragment.LoginDetailFragmentDirections
import org.mozilla.fenix.settings.logins.fragment.SavedLoginsAuthFragmentDirections
import org.mozilla.fenix.settings.search.AddSearchEngineFragmentDirections
import org.mozilla.fenix.settings.search.EditCustomSearchEngineFragmentDirections
import org.mozilla.fenix.settings.studies.StudiesFragmentDirections
import org.mozilla.fenix.settings.wallpaper.WallpaperSettingsFragmentDirections
import org.mozilla.fenix.share.AddNewDeviceFragmentDirections
import org.mozilla.fenix.tabhistory.TabHistoryDialogFragment
import org.mozilla.fenix.tabstray.TabsTrayFragment
import org.mozilla.fenix.tabstray.TabsTrayFragmentDirections
import org.mozilla.fenix.theme.DefaultThemeManager
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.trackingprotection.TrackingProtectionPanelDialogFragmentDirections
import org.mozilla.fenix.utils.BrowsersCache
import org.mozilla.fenix.utils.Settings
import java.lang.ref.WeakReference
import java.util.Locale

/**
 * The main activity of the application. The application is primarily a single Activity (this one)
 * with fragments switching out to display different views. The most important views shown here are the:
 * - home screen
 * - browser screen
 */
@SuppressWarnings("TooManyFunctions", "LargeClass", "LongParameterList", "LongMethod")
open class HomeActivity : LocaleAwareAppCompatActivity(), NavHostActivity {
    // DO NOT MOVE ANYTHING ABOVE THIS, GETTING INIT TIME IS CRITICAL
    // we need to store startup timestamp for warm startup. we cant directly store
    // inside AppStartupTelemetry since that class lives inside components and
    // components requires context to access.
    protected val homeActivityInitTimeStampNanoSeconds = SystemClock.elapsedRealtimeNanos()

    private lateinit var binding: ActivityHomeBinding
    lateinit var themeManager: ThemeManager
    lateinit var browsingModeManager: BrowsingModeManager

    private var isVisuallyComplete = false

    private var privateNotificationObserver: PrivateNotificationFeature<PrivateNotificationService>? =
        null

    private var isToolbarInflated = false

    private val webExtensionPopupFeature by lazy {
        WebExtensionPopupFeature(components.core.store, ::openPopup)
    }

    private val serviceWorkerSupport by lazy {
        ServiceWorkerSupportFeature(this)
    }

    private var inflater: LayoutInflater? = null

    private val navHost by lazy {
        supportFragmentManager.findFragmentById(R.id.container) as NavHostFragment
    }

    private val onboarding by lazy { FenixOnboarding(applicationContext) }

    private val externalSourceIntentProcessors by lazy {
        listOf(
            HomeDeepLinkIntentProcessor(this),
            SpeechProcessingIntentProcessor(this, components.core.store),
            AssistIntentProcessor(),
            StartSearchIntentProcessor(),
            OpenBrowserIntentProcessor(this, ::getIntentSessionId),
            OpenSpecificTabIntentProcessor(this),
            DefaultBrowserIntentProcessor(this),
        )
    }

    // See onKeyDown for why this is necessary
    private var backLongPressJob: Job? = null

    private lateinit var navigationToolbar: Toolbar

    // Tracker for contextual menu (Copy|Search|Select all|etc...)
    private var actionMode: ActionMode? = null

    private val startupPathProvider = StartupPathProvider()
    private lateinit var startupTypeTelemetry: StartupTypeTelemetry

    final override fun onCreate(savedInstanceState: Bundle?) {
        // DO NOT MOVE ANYTHING ABOVE THIS getProfilerTime CALL.
        val startTimeProfiler = components.core.engine.profiler?.getProfilerTime()

        components.strictMode.attachListenerToDisablePenaltyDeath(supportFragmentManager)
        MarkersFragmentLifecycleCallbacks.register(supportFragmentManager, components.core.engine)

        // There is disk read violations on some devices such as samsung and pixel for android 9/10
        components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
            // Theme setup should always be called before super.onCreate
            setupThemeAndBrowsingMode(getModeFromIntentOrLastKnown(intent))
            super.onCreate(savedInstanceState)
        }

        // Checks if Activity is currently in PiP mode if launched from external intents, then exits it
        checkAndExitPiP()

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onCreate()",
            data = mapOf(
                "recreated" to (savedInstanceState != null).toString(),
                "intent" to (intent?.action ?: "null"),
            ),
        )

        components.publicSuffixList.prefetch()

        // Changing a language on the Language screen restarts the activity, but the activity keeps
        // the old layout direction. We have to update the direction manually.
        window.decorView.layoutDirection = TextUtils.getLayoutDirectionFromLocale(Locale.getDefault())

        binding = ActivityHomeBinding.inflate(layoutInflater)
        setContentView(binding.root)
        ProfilerMarkers.addListenerForOnGlobalLayout(components.core.engine, this, binding.root)

        // Must be after we set the content view
        if (isVisuallyComplete) {
            components.performance.visualCompletenessQueue
                .attachViewToRunVisualCompletenessQueueLater(WeakReference(binding.rootContainer))
        }

        privateNotificationObserver = PrivateNotificationFeature(
            applicationContext,
            components.core.store,
            PrivateNotificationService::class,
        ).also {
            it.start()
        }

        // Unless the activity is recreated, navigate to home first (without rendering it)
        // to add it to the back stack.
        if (savedInstanceState == null) {
            navigateToHome()
        }

        if (!shouldStartOnHome() && shouldNavigateToBrowserOnColdStart(savedInstanceState)) {
            navigateToBrowserOnColdStart()
        } else {
            StartOnHome.enterHomeScreen.record(NoExtras())
        }

        if (settings().showHomeOnboardingDialog && onboarding.userHasBeenOnboarded()) {
            navHost.navController.navigate(NavGraphDirections.actionGlobalHomeOnboardingDialog())
        }

        Performance.processIntentIfPerformanceTest(intent, this)

        if (settings().isTelemetryEnabled) {
            lifecycle.addObserver(
                BreadcrumbsRecorder(
                    components.analytics.crashReporter,
                    navHost.navController,
                    ::getBreadcrumbMessage,
                ),
            )

            val safeIntent = intent?.toSafeIntent()
            safeIntent
                ?.let(::getIntentSource)
                ?.also {
                    Events.appOpened.record(Events.AppOpenedExtra(it))
                    // This will record an event in Nimbus' internal event store. Used for behavioral targeting
                    components.analytics.experiments.recordEvent("app_opened")
                }
        }
        supportActionBar?.hide()

        lifecycle.addObservers(webExtensionPopupFeature, serviceWorkerSupport)

        if (shouldAddToRecentsScreen(intent)) {
            intent.removeExtra(START_IN_RECENTS_SCREEN)
            moveTaskToBack(true)
        }

        captureSnapshotTelemetryMetrics()

        startupTelemetryOnCreateCalled(intent.toSafeIntent())
        startupPathProvider.attachOnActivityOnCreate(lifecycle, intent)
        startupTypeTelemetry = StartupTypeTelemetry(components.startupStateProvider, startupPathProvider).apply {
            attachOnHomeActivityOnCreate(lifecycle)
        }

        components.core.requestInterceptor.setNavigationController(navHost.navController)

        if (settings().showContileFeature) {
            components.core.contileTopSitesUpdater.startPeriodicWork()
        }

        // To assess whether the Pocket stories are to be downloaded or not multiple SharedPreferences
        // are read possibly needing to load them on the current thread. Move that to a background thread.
        lifecycleScope.launch(IO) {
            if (settings().showPocketRecommendationsFeature) {
                components.core.pocketStoriesService.startPeriodicStoriesRefresh()
            }
            if (settings().showPocketSponsoredStories) {
                components.core.pocketStoriesService.startPeriodicSponsoredStoriesRefresh()
            }
        }

        showNotificationPermissionPromptIfRequired()

        components.backgroundServices.accountManagerAvailableQueue.runIfReadyOrQueue {
            lifecycleScope.launch(IO) {
                // If we're authenticated, kick-off a sync and a device state refresh.
                components.backgroundServices.accountManager.authenticatedAccount()?.let {
                    components.backgroundServices.accountManager.syncNow(reason = SyncReason.Startup)
                }
            }
        }

        components.core.engine.profiler?.addMarker(
            MarkersActivityLifecycleCallbacks.MARKER_NAME,
            startTimeProfiler,
            "HomeActivity.onCreate",
        )
        StartupTimeline.onActivityCreateEndHome(this) // DO NOT MOVE ANYTHING BELOW HERE.
    }

    /**
     * On Android 13 or above, prompt the user for notification permission at the start.
     * Show the pre permission dialog to the user once if the notification are not enabled.
     */
    private fun showNotificationPermissionPromptIfRequired() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            !NotificationManagerCompat.from(applicationContext).areNotificationsEnabledSafe() &&
            settings().numberOfAppLaunches <= 1
        ) {
            // Recording the exposure event here to capture all users who met all criteria to receive
            // the pre permission notification prompt
            FxNimbus.features.prePermissionNotificationPrompt.recordExposure()

            if (settings().notificationPrePermissionPromptEnabled) {
                if (!settings().isNotificationPrePermissionShown) {
                    navHost.navController.navigate(NavGraphDirections.actionGlobalHomeNotificationPermissionDialog())
                }
            } else {
                // This will trigger the notification permission system dialog as app targets sdk 32.
                ensureMarketingChannelExists(applicationContext)
            }
        }
    }

    private fun checkAndExitPiP() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && isInPictureInPictureMode && intent != null) {
            // Exit PiP mode
            moveTaskToBack(false)
            startActivity(Intent(this, this::class.java).setFlags(FLAG_ACTIVITY_REORDER_TO_FRONT))
        }
    }

    private fun startupTelemetryOnCreateCalled(safeIntent: SafeIntent) {
        // We intentionally only record this in HomeActivity and not ExternalBrowserActivity (e.g.
        // PWAs) so we don't include more unpredictable code paths in the results.
        components.performance.coldStartupDurationTelemetry.onHomeActivityOnCreate(
            components.performance.visualCompletenessQueue,
            components.startupStateProvider,
            safeIntent,
            binding.rootContainer,
        )
    }

    @CallSuper
    @Suppress("TooGenericExceptionCaught")
    override fun onResume() {
        super.onResume()

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onResume()",
        )

        lifecycleScope.launch(IO) {
            try {
                if (settings().showContileFeature) {
                    components.core.contileTopSitesProvider.refreshTopSitesIfCacheExpired()
                }
            } catch (e: Exception) {
                Logger.error("Failed to refresh contile top sites", e)
            }

            if (settings().checkIfFenixIsDefaultBrowserOnAppResume()) {
                Events.defaultBrowserChanged.record(NoExtras())
            }

            // We attempt to send metrics onResume so that the start of new user sessions is not
            // missed. Previously, this was done in FenixApplication::onCreate, but it was decided
            // that we should not rely on the application being killed between user sessions.
            components.appStore.dispatch(AppAction.ResumedMetricsAction)

            with(applicationContext) {
                // Only set up Workers if notifications are enabled
                val notificationManagerCompat = NotificationManagerCompat.from(this)
                if (notificationManagerCompat.isNotificationChannelEnabled(MARKETING_CHANNEL_ID)) {
                    DefaultBrowserNotificationWorker.setDefaultBrowserNotificationIfNeeded(this)
                    ReEngagementNotificationWorker.setReEngagementNotificationIfNeeded(this)
                    MessageNotificationWorker.setMessageNotificationWorker(this)
                }
            }
        }

        // This was done in order to refresh search engines when app is running in background
        // and the user changes the system language
        // More details here: https://github.com/mozilla-mobile/fenix/pull/27793#discussion_r1029892536
        components.core.store.dispatch(SearchAction.RefreshSearchEnginesAction)
    }

    override fun onStart() {
        // DO NOT MOVE ANYTHING ABOVE THIS getProfilerTime CALL.
        val startProfilerTime = components.core.engine.profiler?.getProfilerTime()

        super.onStart()

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onStart()",
        )

        ProfilerMarkers.homeActivityOnStart(binding.rootContainer, components.core.engine.profiler)
        components.core.engine.profiler?.addMarker(
            MarkersActivityLifecycleCallbacks.MARKER_NAME,
            startProfilerTime,
            "HomeActivity.onStart",
        ) // DO NOT MOVE ANYTHING BELOW THIS addMarker CALL.
    }

    override fun onStop() {
        super.onStop()

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onStop()",
            data = mapOf(
                "finishing" to isFinishing.toString(),
            ),
        )
    }

    final override fun onPause() {
        // We should return to the browser if there were normal tabs when we left the app
        settings().shouldReturnToBrowser =
            components.core.store.state.getNormalOrPrivateTabs(private = false).isNotEmpty()

        lifecycleScope.launch(IO) {
            components.core.bookmarksStorage.getTree(BookmarkRoot.Root.id, true)?.let {
                val desktopRootNode = DesktopFolders(
                    applicationContext,
                    showMobileRoot = false,
                ).withOptionalDesktopFolders(it)
                settings().desktopBookmarksSize = getBookmarkCount(desktopRootNode)
            }

            components.core.bookmarksStorage.getTree(BookmarkRoot.Mobile.id, true)?.let {
                settings().mobileBookmarksSize = getBookmarkCount(it)
            }
        }

        super.onPause()

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onPause()",
            data = mapOf(
                "finishing" to isFinishing.toString(),
            ),
        )

        // Every time the application goes into the background, it is possible that the user
        // is about to change the browsers installed on their system. Therefore, we reset the cache of
        // all the installed browsers.
        //
        // NB: There are ways for the user to install new products without leaving the browser.
        BrowsersCache.resetAll()
    }

    private fun getBookmarkCount(node: BookmarkNode): Int {
        val children = node.children
        return if (children == null) {
            0
        } else {
            var count = 0

            for (child in children) {
                if (child.type == BookmarkNodeType.FOLDER) {
                    count += getBookmarkCount(child)
                } else if (child.type == BookmarkNodeType.ITEM) {
                    count++
                }
            }

            count
        }
    }

    override fun onDestroy() {
        super.onDestroy()

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onDestroy()",
            data = mapOf(
                "finishing" to isFinishing.toString(),
            ),
        )

        components.core.contileTopSitesUpdater.stopPeriodicWork()
        components.core.pocketStoriesService.stopPeriodicStoriesRefresh()
        components.core.pocketStoriesService.stopPeriodicSponsoredStoriesRefresh()
        privateNotificationObserver?.stop()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onConfigurationChanged()",
        )
    }

    override fun recreate() {
        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "recreate()",
        )

        super.recreate()
    }

    /**
     * Handles intents received when the activity is open.
     */
    final override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        intent?.let {
            handleNewIntent(it)
        }
        startupPathProvider.onIntentReceived(intent)
    }

    open fun handleNewIntent(intent: Intent) {
        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onNewIntent()",
            data = mapOf(
                "intent" to intent.action.toString(),
            ),
        )

        val tab = components.core.store.state.findActiveMediaTab()
        if (tab != null) {
            components.useCases.sessionUseCases.exitFullscreen(tab.id)
        }

        val intentProcessors =
            listOf(
                CrashReporterIntentProcessor(components.appStore),
            ) + externalSourceIntentProcessors
        val intentHandled =
            intentProcessors.any { it.process(intent, navHost.navController, this.intent) }
        browsingModeManager.mode = getModeFromIntentOrLastKnown(intent)

        if (intentHandled) {
            supportFragmentManager
                .primaryNavigationFragment
                ?.childFragmentManager
                ?.fragments
                ?.lastOrNull()
                ?.let { it as? TabsTrayFragment }
                ?.also { it.dismissAllowingStateLoss() }
        }
    }

    /**
     * Overrides view inflation to inject a custom [EngineView] from [components].
     */
    final override fun onCreateView(
        parent: View?,
        name: String,
        context: Context,
        attrs: AttributeSet,
    ): View? = when (name) {
        EngineView::class.java.name -> components.core.engine.createView(context, attrs).apply {
            selectionActionDelegate = DefaultSelectionActionDelegate(
                BrowserStoreSearchAdapter(
                    components.core.store,
                    tabId = getIntentSessionId(intent.toSafeIntent()),
                ),
                resources = context.resources,
                shareTextClicked = { share(it) },
                emailTextClicked = { email(it) },
                callTextClicked = { call(it) },
                actionSorter = ::actionSorter,
            )
        }.asView()
        else -> super.onCreateView(parent, name, context, attrs)
    }

    override fun onActionModeStarted(mode: ActionMode?) {
        actionMode = mode
        super.onActionModeStarted(mode)
    }

    override fun onActionModeFinished(mode: ActionMode?) {
        actionMode = null
        super.onActionModeFinished(mode)
    }

    fun finishActionMode() {
        actionMode?.finish().also { actionMode = null }
    }

    @Suppress("MagicNumber")
    // Defining the positions as constants doesn't seem super useful here.
    private fun actionSorter(actions: Array<String>): Array<String> {
        val order = hashMapOf<String, Int>()

        order["CUSTOM_CONTEXT_MENU_EMAIL"] = 0
        order["CUSTOM_CONTEXT_MENU_CALL"] = 1
        order["org.mozilla.geckoview.COPY"] = 2
        order["CUSTOM_CONTEXT_MENU_SEARCH"] = 3
        order["CUSTOM_CONTEXT_MENU_SEARCH_PRIVATELY"] = 4
        order["org.mozilla.geckoview.PASTE"] = 5
        order["org.mozilla.geckoview.SELECT_ALL"] = 6
        order["CUSTOM_CONTEXT_MENU_SHARE"] = 7

        return actions.sortedBy { actionName ->
            // Sort the actions in our preferred order, putting "other" actions unsorted at the end
            order[actionName] ?: actions.size
        }.toTypedArray()
    }

    final override fun onBackPressed() {
        supportFragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is UserInteractionHandler && it.onBackPressed()) {
                return
            }
        }
        super.getOnBackPressedDispatcher().onBackPressed()
    }

    @Deprecated("Deprecated in Java")
    // https://github.com/mozilla-mobile/fenix/issues/19919
    final override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        supportFragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is ActivityResultHandler && it.onActivityResult(requestCode, data, resultCode)) {
                return
            }
        }
        @Suppress("DEPRECATION")
        super.onActivityResult(requestCode, resultCode, data)
    }

    private fun shouldUseCustomBackLongPress(): Boolean {
        val isAndroidN =
            Build.VERSION.SDK_INT == Build.VERSION_CODES.N || Build.VERSION.SDK_INT == Build.VERSION_CODES.N_MR1
        // Huawei devices seem to have problems with onKeyLongPress
        // See https://github.com/mozilla-mobile/fenix/issues/13498
        return isAndroidN || ManufacturerCodes.isHuawei
    }

    private fun handleBackLongPress(): Boolean {
        supportFragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is OnBackLongPressedListener && it.onBackLongPressed()) {
                return true
            }
        }
        return false
    }

    override fun dispatchTouchEvent(ev: MotionEvent?): Boolean {
        ProfilerMarkers.addForDispatchTouchEvent(components.core.engine.profiler, ev)
        return super.dispatchTouchEvent(ev)
    }

    final override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        // Inspired by https://searchfox.org/mozilla-esr68/source/mobile/android/base/java/org/mozilla/gecko/BrowserApp.java#584-613
        // Android N and Huawei devices have broken onKeyLongPress events for the back button, so we
        // instead implement the long press behavior ourselves
        // - For short presses, we cancel the callback in onKeyUp
        // - For long presses, the normal keypress is marked as cancelled, hence won't be handled elsewhere
        //   (but Android still provides the haptic feedback), and the long press action is run
        if (shouldUseCustomBackLongPress() && keyCode == KeyEvent.KEYCODE_BACK) {
            backLongPressJob = lifecycleScope.launch {
                delay(ViewConfiguration.getLongPressTimeout().toLong())
                handleBackLongPress()
            }
        }
        return super.onKeyDown(keyCode, event)
    }

    final override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        if (shouldUseCustomBackLongPress() && keyCode == KeyEvent.KEYCODE_BACK) {
            backLongPressJob?.cancel()

            // check if the key has been pressed for longer than the time needed for a press to turn into a long press
            // and if tab history is already visible we do not want to dismiss it.
            if (event.eventTime - event.downTime >= ViewConfiguration.getLongPressTimeout() &&
                navHost.navController.hasTopDestination(TabHistoryDialogFragment.NAME)
            ) {
                // returning true avoids further processing of the KeyUp event and avoids dismissing tab history.
                return true
            }
        }
        return super.onKeyUp(keyCode, event)
    }

    final override fun onKeyLongPress(keyCode: Int, event: KeyEvent?): Boolean {
        // onKeyLongPress is broken in Android N so we don't handle back button long presses here
        // for N. The version check ensures we don't handle back button long presses twice.
        if (!shouldUseCustomBackLongPress() && keyCode == KeyEvent.KEYCODE_BACK) {
            return handleBackLongPress()
        }
        return super.onKeyLongPress(keyCode, event)
    }

    final override fun onUserLeaveHint() {
        supportFragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is UserInteractionHandler && it.onHomePressed()) {
                return
            }
        }

        super.onUserLeaveHint()
    }

    protected open fun getBreadcrumbMessage(destination: NavDestination): String {
        val fragmentName = resources.getResourceEntryName(destination.id)
        return "Changing to fragment $fragmentName, isCustomTab: false"
    }

    @VisibleForTesting(otherwise = PROTECTED)
    internal open fun getIntentSource(intent: SafeIntent): String? {
        return when {
            intent.isLauncherIntent -> "APP_ICON"
            intent.action == Intent.ACTION_VIEW -> "LINK"
            else -> null
        }
    }

    /**
     * External sources such as 3rd party links and shortcuts use this function to enter
     * private mode directly before the content view is created. Returns the mode set by the intent
     * otherwise falls back to the last known mode.
     */
    internal fun getModeFromIntentOrLastKnown(intent: Intent?): BrowsingMode {
        intent?.toSafeIntent()?.let {
            if (it.hasExtra(PRIVATE_BROWSING_MODE)) {
                val startPrivateMode = it.getBooleanExtra(PRIVATE_BROWSING_MODE, false)
                return BrowsingMode.fromBoolean(isPrivate = startPrivateMode)
            }
        }
        return settings().lastKnownMode
    }

    /**
     * Determines whether the activity should be pushed to be backstack (i.e., 'minimized' to the recents
     * screen) upon starting.
     * @param intent - The intent that started this activity. Is checked for having the 'START_IN_RECENTS_SCREEN'-extra.
     * @return true if the activity should be started and pushed to the recents screen, false otherwise.
     */
    private fun shouldAddToRecentsScreen(intent: Intent?): Boolean {
        intent?.toSafeIntent()?.let {
            return it.getBooleanExtra(START_IN_RECENTS_SCREEN, false)
        }
        return false
    }

    private fun setupThemeAndBrowsingMode(mode: BrowsingMode) {
        settings().lastKnownMode = mode
        browsingModeManager = createBrowsingModeManager(mode)
        themeManager = createThemeManager()
        themeManager.setActivityTheme(this)
        themeManager.applyStatusBarTheme(this)
    }

    /**
     * Returns the [supportActionBar], inflating it if necessary.
     * Everyone should call this instead of supportActionBar.
     */
    override fun getSupportActionBarAndInflateIfNecessary(): ActionBar {
        if (!isToolbarInflated) {
            navigationToolbar = binding.navigationToolbarStub.inflate() as Toolbar

            setSupportActionBar(navigationToolbar)
            // Add ids to this that we don't want to have a toolbar back button
            setupNavigationToolbar()
            setNavigationIcon(R.drawable.ic_back_button)

            isToolbarInflated = true
        }
        return supportActionBar!!
    }

    @Suppress("SpreadOperator")
    fun setupNavigationToolbar(vararg topLevelDestinationIds: Int) {
        NavigationUI.setupWithNavController(
            navigationToolbar,
            navHost.navController,
            AppBarConfiguration.Builder(*topLevelDestinationIds).build(),
        )

        navigationToolbar.setNavigationOnClickListener {
            onBackPressed()
        }
    }

    protected open fun getIntentSessionId(intent: SafeIntent): String? = null

    /**
     * Navigates to the browser fragment and loads a URL or performs a search (depending on the
     * value of [searchTermOrURL]).
     *
     * @param flags Flags that will be used when loading the URL (not applied to searches).
     */
    @Suppress("LongParameterList")
    fun openToBrowserAndLoad(
        searchTermOrURL: String,
        newTab: Boolean,
        from: BrowserDirection,
        customTabSessionId: String? = null,
        engine: SearchEngine? = null,
        forceSearch: Boolean = false,
        flags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none(),
        requestDesktopMode: Boolean = false,
        historyMetadata: HistoryMetadataKey? = null,
    ) {
        openToBrowser(from, customTabSessionId)
        load(searchTermOrURL, newTab, engine, forceSearch, flags, requestDesktopMode, historyMetadata)
    }

    fun openToBrowser(from: BrowserDirection, customTabSessionId: String? = null) {
        if (navHost.navController.alreadyOnDestination(R.id.browserFragment)) return
        @IdRes val fragmentId = if (from.fragmentId != 0) from.fragmentId else null
        val directions = getNavDirections(from, customTabSessionId)
        if (directions != null) {
            navHost.navController.nav(fragmentId, directions)
        }
    }

    protected open fun getNavDirections(
        from: BrowserDirection,
        customTabSessionId: String?,
    ): NavDirections? = when (from) {
        BrowserDirection.FromGlobal ->
            NavGraphDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromHome ->
            HomeFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromWallpaper ->
            WallpaperSettingsFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromSearchDialog ->
            SearchDialogFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromSettings ->
            SettingsFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromBookmarks ->
            BookmarkFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromBookmarkSearchDialog ->
            SearchDialogFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromHistory ->
            HistoryFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromHistorySearchDialog ->
            SearchDialogFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromHistoryMetadataGroup ->
            HistoryMetadataGroupFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromTrackingProtectionExceptions ->
            TrackingProtectionExceptionsFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromCookieBanner ->
            CookieBannersFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromHttpsOnlyMode ->
            HttpsOnlyFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromAbout ->
            AboutFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromTrackingProtection ->
            TrackingProtectionFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromTrackingProtectionDialog ->
            TrackingProtectionPanelDialogFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromSavedLoginsFragment ->
            SavedLoginsAuthFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromAddNewDeviceFragment ->
            AddNewDeviceFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromAddSearchEngineFragment ->
            AddSearchEngineFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromEditCustomSearchEngineFragment ->
            EditCustomSearchEngineFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromAddonDetailsFragment ->
            AddonDetailsFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromAddonPermissionsDetailsFragment ->
            AddonPermissionsDetailsFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromLoginDetailFragment ->
            LoginDetailFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromTabsTray ->
            TabsTrayFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromRecentlyClosed ->
            RecentlyClosedFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromStudiesFragment -> StudiesFragmentDirections.actionGlobalBrowser(
            customTabSessionId,
        )
    }

    /**
     * Loads a URL or performs a search (depending on the value of [searchTermOrURL]).
     *
     * @param flags Flags that will be used when loading the URL (not applied to searches).
     * @param historyMetadata The [HistoryMetadataKey] of the new tab in case this tab
     * was opened from history.
     */
    private fun load(
        searchTermOrURL: String,
        newTab: Boolean,
        engine: SearchEngine?,
        forceSearch: Boolean,
        flags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none(),
        requestDesktopMode: Boolean = false,
        historyMetadata: HistoryMetadataKey? = null,
    ) {
        val startTime = components.core.engine.profiler?.getProfilerTime()
        val mode = browsingModeManager.mode

        val private = when (mode) {
            BrowsingMode.Private -> true
            BrowsingMode.Normal -> false
        }

        // In situations where we want to perform a search but have no search engine (e.g. the user
        // has removed all of them, or we couldn't load any) we will pass searchTermOrURL to Gecko
        // and let it try to load whatever was entered.
        if ((!forceSearch && searchTermOrURL.isUrl()) || engine == null) {
            val tabId = if (newTab) {
                components.useCases.tabsUseCases.addTab(
                    url = searchTermOrURL.toNormalizedUrl(),
                    flags = flags,
                    private = private,
                    historyMetadata = historyMetadata,
                )
            } else {
                components.useCases.sessionUseCases.loadUrl(
                    url = searchTermOrURL.toNormalizedUrl(),
                    flags = flags,
                )
                components.core.store.state.selectedTabId
            }

            if (requestDesktopMode && tabId != null) {
                handleRequestDesktopMode(tabId)
            }
        } else {
            if (newTab) {
                components.useCases.searchUseCases.newTabSearch
                    .invoke(
                        searchTermOrURL,
                        SessionState.Source.Internal.UserEntered,
                        true,
                        mode.isPrivate,
                        searchEngine = engine,
                    )
            } else {
                components.useCases.searchUseCases.defaultSearch.invoke(searchTermOrURL, engine)
            }
        }

        if (components.core.engine.profiler?.isProfilerActive() == true) {
            // Wrapping the `addMarker` method with `isProfilerActive` even though it's no-op when
            // profiler is not active. That way, `text` argument will not create a string builder all the time.
            components.core.engine.profiler?.addMarker(
                "HomeActivity.load",
                startTime,
                "newTab: $newTab",
            )
        }
    }

    internal fun handleRequestDesktopMode(tabId: String) {
        components.useCases.sessionUseCases.requestDesktopSite(true, tabId)
        components.core.store.dispatch(ContentAction.UpdateDesktopModeAction(tabId, true))

        // Reset preference value after opening the tab in desktop mode
        settings().openNextTabInDesktopMode = false
    }

    open fun navigateToBrowserOnColdStart() {
        // Normal tabs + cold start -> Should go back to browser if we had any tabs open when we left last
        // except for PBM + Cold Start there won't be any tabs since they're evicted so we never will navigate
        if (settings().shouldReturnToBrowser && !browsingModeManager.mode.isPrivate) {
            // Navigate to home first (without rendering it) to add it to the back stack.
            openToBrowser(BrowserDirection.FromGlobal, null)
        }
    }

    open fun navigateToHome() {
        navHost.navController.navigate(NavGraphDirections.actionStartupHome())
    }

    override fun attachBaseContext(base: Context) {
        base.components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
            super.attachBaseContext(base)
        }
    }

    override fun getSystemService(name: String): Any? {
        // Issue #17759 had a crash with the PerformanceInflater.kt on Android 5.0 and 5.1
        // when using the TimePicker. Since the inflater was created for performance monitoring
        // purposes and that we test on new android versions, this means that any difference in
        // inflation will be caught on those devices.
        if (LAYOUT_INFLATER_SERVICE == name && Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP_MR1) {
            if (inflater == null) {
                inflater = PerformanceInflater(LayoutInflater.from(baseContext), this)
            }
            return inflater
        }
        return super.getSystemService(name)
    }

    protected open fun createBrowsingModeManager(initialMode: BrowsingMode): BrowsingModeManager {
        return DefaultBrowsingModeManager(initialMode, components.settings) { newMode ->
            updateSecureWindowFlags(newMode)
            themeManager.currentTheme = newMode
        }.also {
            updateSecureWindowFlags(initialMode)
        }
    }

    private fun updateSecureWindowFlags(mode: BrowsingMode = browsingModeManager.mode) {
        if (mode == BrowsingMode.Private && !settings().allowScreenshotsInPrivateMode) {
            window.addFlags(FLAG_SECURE)
        } else {
            window.clearFlags(FLAG_SECURE)
        }
    }

    protected open fun createThemeManager(): ThemeManager {
        return DefaultThemeManager(browsingModeManager.mode, this)
    }

    private fun openPopup(webExtensionState: WebExtensionState) {
        val action = NavGraphDirections.actionGlobalWebExtensionActionPopupFragment(
            webExtensionId = webExtensionState.id,
            webExtensionTitle = webExtensionState.name,
        )
        navHost.navController.navigate(action)
    }

    /**
     * The root container is null at this point, so let the HomeActivity know that
     * we are visually complete.
     */
    fun setVisualCompletenessQueueReady() {
        isVisuallyComplete = true
    }

    private fun captureSnapshotTelemetryMetrics() = CoroutineScope(IO).launch {
        // PWA
        val recentlyUsedPwaCount = components.core.webAppShortcutManager.recentlyUsedWebAppsCount(
            activeThresholdMs = PWA_RECENTLY_USED_THRESHOLD,
        )
        if (recentlyUsedPwaCount == 0) {
            Metrics.hasRecentPwas.set(false)
        } else {
            Metrics.hasRecentPwas.set(true)
            // This metric's lifecycle is set to 'application', meaning that it gets reset upon
            // application restart. Combined with the behaviour of the metric type itself (a growing counter),
            // it's important that this metric is only set once per application's lifetime.
            // Otherwise, we're going to over-count.
            Metrics.recentlyUsedPwaCount.add(recentlyUsedPwaCount)
        }
    }

    @VisibleForTesting
    internal fun isActivityColdStarted(startingIntent: Intent, activityIcicle: Bundle?): Boolean {
        // First time opening this activity in the task.
        // Cold start / start from Recents after back press.
        return activityIcicle == null &&
            // Activity was restarted from Recents after it was destroyed by Android while in background
            // in cases of memory pressure / "Don't keep activities".
            startingIntent.flags and Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY == 0
    }

    /**
     *  Indicates if the user should be redirected to the [BrowserFragment] or to the [HomeFragment],
     *  links from an external apps should always opened in the [BrowserFragment].
     */
    fun shouldStartOnHome(intent: Intent? = this.intent): Boolean {
        return components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
            // We only want to open on home when users tap the app,
            // we want to ignore other cases when the app gets open by users clicking on links.
            getSettings().shouldStartOnHome() && intent?.action == ACTION_MAIN
        }
    }

    fun processIntent(intent: Intent): Boolean {
        return externalSourceIntentProcessors.any {
            it.process(
                intent,
                navHost.navController,
                this.intent,
            )
        }
    }

    @VisibleForTesting
    internal fun getSettings(): Settings = settings()

    private fun shouldNavigateToBrowserOnColdStart(savedInstanceState: Bundle?): Boolean {
        return isActivityColdStarted(intent, savedInstanceState) &&
            !processIntent(intent)
    }

    companion object {
        const val OPEN_TO_BROWSER = "open_to_browser"
        const val OPEN_TO_BROWSER_AND_LOAD = "open_to_browser_and_load"
        const val OPEN_TO_SEARCH = "open_to_search"
        const val PRIVATE_BROWSING_MODE = "private_browsing_mode"
        const val START_IN_RECENTS_SCREEN = "start_in_recents_screen"

        // PWA must have been used within last 30 days to be considered "recently used" for the
        // telemetry purposes.
        const val PWA_RECENTLY_USED_THRESHOLD = DateUtils.DAY_IN_MILLIS * 30L
    }
}
