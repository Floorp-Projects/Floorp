/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_MAIN
import android.content.res.Configuration
import android.os.Build
import android.os.Bundle
import android.os.StrictMode
import android.os.SystemClock
import android.text.format.DateUtils
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ActionMode
import android.view.ViewConfiguration
import android.view.WindowManager.LayoutParams.FLAG_SECURE
import androidx.annotation.CallSuper
import androidx.annotation.IdRes
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PROTECTED
import androidx.appcompat.app.ActionBar
import androidx.appcompat.widget.Toolbar
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.NavigationUI
import kotlinx.android.synthetic.main.activity_home.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.Dispatchers.IO
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.feature.contextmenu.DefaultSelectionActionDelegate
import mozilla.components.feature.privatemode.notification.PrivateNotificationFeature
import mozilla.components.feature.search.BrowserStoreSearchAdapter
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.base.feature.ActivityResultHandler
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import mozilla.components.support.ktx.android.content.call
import mozilla.components.support.ktx.android.content.email
import mozilla.components.support.ktx.android.content.share
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import mozilla.components.support.locale.LocaleAwareAppCompatActivity
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.toSafeIntent
import mozilla.components.support.webextensions.WebExtensionPopupFeature
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.PerfStartup
import org.mozilla.fenix.addons.AddonDetailsFragmentDirections
import org.mozilla.fenix.addons.AddonPermissionsDetailsFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.DefaultBrowsingModeManager
import org.mozilla.fenix.components.metrics.BreadcrumbsRecorder
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.exceptions.trackingprotection.TrackingProtectionExceptionsFragmentDirections
import org.mozilla.fenix.ext.alreadyOnDestination
import org.mozilla.fenix.ext.breadcrumb
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.measureNoInline
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.setNavigationIcon
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.intent.CrashReporterIntentProcessor
import org.mozilla.fenix.home.intent.DefaultBrowserIntentProcessor
import org.mozilla.fenix.home.intent.OpenBrowserIntentProcessor
import org.mozilla.fenix.home.intent.OpenSpecificTabIntentProcessor
import org.mozilla.fenix.home.intent.SpeechProcessingIntentProcessor
import org.mozilla.fenix.home.intent.StartSearchIntentProcessor
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentDirections
import org.mozilla.fenix.library.bookmarks.DesktopFolders
import org.mozilla.fenix.library.history.HistoryFragmentDirections
import org.mozilla.fenix.library.recentlyclosed.RecentlyClosedFragmentDirections
import org.mozilla.fenix.onboarding.DefaultBrowserNotificationWorker
import org.mozilla.fenix.perf.Performance
import org.mozilla.fenix.perf.PerformanceInflater
import org.mozilla.fenix.perf.ProfilerMarkers
import org.mozilla.fenix.perf.StartupPathProvider
import org.mozilla.fenix.perf.StartupTimeline
import org.mozilla.fenix.perf.StartupTypeTelemetry
import org.mozilla.fenix.search.SearchDialogFragmentDirections
import org.mozilla.fenix.session.PrivateNotificationService
import org.mozilla.fenix.settings.SettingsFragmentDirections
import org.mozilla.fenix.settings.TrackingProtectionFragmentDirections
import org.mozilla.fenix.settings.about.AboutFragmentDirections
import org.mozilla.fenix.settings.logins.fragment.LoginDetailFragmentDirections
import org.mozilla.fenix.settings.logins.fragment.SavedLoginsAuthFragmentDirections
import org.mozilla.fenix.settings.search.AddSearchEngineFragmentDirections
import org.mozilla.fenix.settings.search.EditCustomSearchEngineFragmentDirections
import org.mozilla.fenix.share.AddNewDeviceFragmentDirections
import org.mozilla.fenix.tabstray.TabsTrayFragment
import org.mozilla.fenix.tabstray.TabsTrayFragmentDirections
import org.mozilla.fenix.theme.DefaultThemeManager
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.BrowsersCache
import org.mozilla.fenix.utils.Settings
import java.lang.ref.WeakReference

/**
 * The main activity of the application. The application is primarily a single Activity (this one)
 * with fragments switching out to display different views. The most important views shown here are the:
 * - home screen
 * - browser screen
 */
@OptIn(ExperimentalCoroutinesApi::class)
@SuppressWarnings("TooManyFunctions", "LargeClass", "LongParameterList")
open class HomeActivity : LocaleAwareAppCompatActivity(), NavHostActivity {
    // DO NOT MOVE ANYTHING ABOVE THIS, GETTING INIT TIME IS CRITICAL
    // we need to store startup timestamp for warm startup. we cant directly store
    // inside AppStartupTelemetry since that class lives inside components and
    // components requires context to access.
    protected val homeActivityInitTimeStampNanoSeconds = SystemClock.elapsedRealtimeNanos()

    lateinit var themeManager: ThemeManager
    lateinit var browsingModeManager: BrowsingModeManager

    private var isVisuallyComplete = false

    private var privateNotificationObserver: PrivateNotificationFeature<PrivateNotificationService>? =
        null

    private var isToolbarInflated = false

    private val webExtensionPopupFeature by lazy {
        WebExtensionPopupFeature(components.core.store, ::openPopup)
    }

    private var inflater: LayoutInflater? = null

    private val navHost by lazy {
        supportFragmentManager.findFragmentById(R.id.container) as NavHostFragment
    }

    private val externalSourceIntentProcessors by lazy {
        listOf(
            SpeechProcessingIntentProcessor(this, components.core.store, components.analytics.metrics),
            StartSearchIntentProcessor(components.analytics.metrics),
            OpenBrowserIntentProcessor(this, ::getIntentSessionId),
            OpenSpecificTabIntentProcessor(this),
            DefaultBrowserIntentProcessor(this, components.analytics.metrics)
        )
    }

    // See onKeyDown for why this is necessary
    private var backLongPressJob: Job? = null

    private lateinit var navigationToolbar: Toolbar

    // Tracker for contextual menu (Copy|Search|Select all|etc...)
    private var actionMode: ActionMode? = null

    private val startupPathProvider = StartupPathProvider()
    private lateinit var startupTypeTelemetry: StartupTypeTelemetry

    final override fun onCreate(savedInstanceState: Bundle?): Unit = PerfStartup.homeActivityOnCreate.measureNoInline {
        // DO NOT MOVE ANYTHING ABOVE THIS addMarker CALL.
        components.core.engine.profiler?.addMarker("Activity.onCreate", "HomeActivity")

        components.strictMode.attachListenerToDisablePenaltyDeath(supportFragmentManager)
        // There is disk read violations on some devices such as samsung and pixel for android 9/10
        components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
            // Theme setup should always be called before super.onCreate
            setupThemeAndBrowsingMode(getModeFromIntentOrLastKnown(intent))
            super.onCreate(savedInstanceState)
        }

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onCreate()",
            data = mapOf(
                "recreated" to (savedInstanceState != null).toString(),
                "intent" to (intent?.action ?: "null")
            )
        )

        components.publicSuffixList.prefetch()

        setContentView(R.layout.activity_home)

        // Must be after we set the content view
        if (isVisuallyComplete) {
            components.performance.visualCompletenessQueue
                .attachViewToRunVisualCompletenessQueueLater(WeakReference(rootContainer))
        }

        privateNotificationObserver = PrivateNotificationFeature(
            applicationContext,
            components.core.store,
            PrivateNotificationService::class
        ).also {
            it.start()
        }

        if (!shouldStartOnHome() &&
            shouldNavigateBrowserFragmentOnCouldStart(savedInstanceState)
        ) {
            navigateToBrowserOnColdStart()
        } else {
            components.analytics.metrics.track(Event.StartOnHomeEnterHomeScreen)
        }

        Performance.processIntentIfPerformanceTest(intent, this)

        if (settings().isTelemetryEnabled) {
            lifecycle.addObserver(
                BreadcrumbsRecorder(
                    components.analytics.crashReporter,
                    navHost.navController, ::getBreadcrumbMessage
                )
            )

            val safeIntent = intent?.toSafeIntent()
            safeIntent
                ?.let(::getIntentSource)
                ?.also { components.analytics.metrics.track(Event.OpenedApp(it)) }
            // record on cold startup
            safeIntent
                ?.let(::getIntentAllSource)
                ?.also { components.analytics.metrics.track(Event.AppReceivedIntent(it)) }
        }
        supportActionBar?.hide()

        lifecycle.addObservers(
            webExtensionPopupFeature,
            StartupTimeline.homeActivityLifecycleObserver
        )

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

        StartupTimeline.onActivityCreateEndHome(this) // DO NOT MOVE ANYTHING BELOW HERE.
    }

    private fun startupTelemetryOnCreateCalled(safeIntent: SafeIntent) {
        // We intentionally only record this in HomeActivity and not ExternalBrowserActivity (e.g.
        // PWAs) so we don't include more unpredictable code paths in the results.
        components.performance.coldStartupDurationTelemetry.onHomeActivityOnCreate(
            components.performance.visualCompletenessQueue,
            components.startupStateProvider,
            safeIntent,
            rootContainer
        )
    }

    @CallSuper
    override fun onResume() {
        super.onResume()

        // Even if screenshots are allowed, we hide private content in the recents screen in onPause
        // only when we are in private mode, so in onResume we should go back to setting these flags
        // with the user screenshot setting only when we are in private mode.
        // See https://github.com/mozilla-mobile/fenix/issues/11153
        if (settings().lastKnownMode == BrowsingMode.Private) {
            updateSecureWindowFlags(settings().lastKnownMode)
        }

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onResume()"
        )

        components.backgroundServices.accountManagerAvailableQueue.runIfReadyOrQueue {
            lifecycleScope.launch {
                // Make sure accountManager is initialized.
                components.backgroundServices.accountManager.start()
                // If we're authenticated, kick-off a sync and a device state refresh.
                components.backgroundServices.accountManager.authenticatedAccount()?.let {
                    components.backgroundServices.accountManager.syncNow(
                        SyncReason.Startup,
                        debounce = true
                    )
                }
            }
        }

        isFenixTheDefaultBrowser()
    }

    override fun onStart() = PerfStartup.homeActivityOnStart.measureNoInline {
        super.onStart()

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onStart()"
        )

        ProfilerMarkers.homeActivityOnStart(rootContainer, components.core.engine.profiler)
    }

    override fun onStop() {
        super.onStop()

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onStop()",
            data = mapOf(
                "finishing" to isFinishing.toString()
            )
        )
    }

    final override fun onPause() {
        // We should return to the browser if there were normal tabs when we left the app
        settings().shouldReturnToBrowser =
            components.core.store.state.getNormalOrPrivateTabs(private = false).isNotEmpty()

        // Even if screenshots are allowed, we want to hide private content in the recents screen
        // only when we are in private mode
        // See https://github.com/mozilla-mobile/fenix/issues/11153
        if (settings().lastKnownMode.isPrivate) {
            window.addFlags(FLAG_SECURE)
        }

        lifecycleScope.launch(IO) {
            components.core.bookmarksStorage.getTree(BookmarkRoot.Root.id, true)?.let {
                val desktopRootNode = DesktopFolders(
                    applicationContext,
                    showMobileRoot = false
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
                "finishing" to isFinishing.toString()
            )
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
                "finishing" to isFinishing.toString()
            )
        )

        privateNotificationObserver?.stop()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "onConfigurationChanged()"
        )
    }

    override fun recreate() {
        // Diagnostic breadcrumb for "Display already aquired" crash:
        // https://github.com/mozilla-mobile/android-components/issues/7960
        breadcrumb(
            message = "recreate()"
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
                "intent" to intent.action.toString()
            )
        )

        val intentProcessors =
            listOf(CrashReporterIntentProcessor()) + externalSourceIntentProcessors
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

        // Note: This does not work in case of an user sending an intent with ACTION_VIEW
        // for example, launch the application, and than use adb to send an intent with
        // ACTION_VIEW to open a link. In this case, we will get multiple telemetry events.
        intent
            .toSafeIntent()
            .let(::getIntentAllSource)
            ?.also { components.analytics.metrics.track(Event.AppReceivedIntent(it)) }
    }

    /**
     * Overrides view inflation to inject a custom [EngineView] from [components].
     */
    final override fun onCreateView(
        parent: View?,
        name: String,
        context: Context,
        attrs: AttributeSet
    ): View? = when (name) {
        EngineView::class.java.name -> components.core.engine.createView(context, attrs).apply {
            selectionActionDelegate = DefaultSelectionActionDelegate(
                BrowserStoreSearchAdapter(
                    components.core.store,
                    tabId = getIntentSessionId(intent.toSafeIntent())
                ),
                resources = context.resources,
                shareTextClicked = { share(it) },
                emailTextClicked = { email(it) },
                callTextClicked = { call(it) },
                actionSorter = ::actionSorter
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
        super.onBackPressed()
    }

    @Suppress("DEPRECATION")
    // https://github.com/mozilla-mobile/fenix/issues/19919
    final override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        supportFragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is ActivityResultHandler && it.onActivityResult(requestCode, data, resultCode)) {
                return
            }
        }
        super.onActivityResult(requestCode, resultCode, data)
    }

    private fun shouldUseCustomBackLongPress(): Boolean {
        val isAndroidN =
            Build.VERSION.SDK_INT == Build.VERSION_CODES.N || Build.VERSION.SDK_INT == Build.VERSION_CODES.N_MR1
        // Huawei devices seem to have problems with onKeyLongPress
        // See https://github.com/mozilla-mobile/fenix/issues/13498
        val isHuawei = Build.MANUFACTURER.equals("huawei", ignoreCase = true)
        return isAndroidN || isHuawei
    }

    private fun handleBackLongPress(): Boolean {
        supportFragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is OnBackLongPressedListener && it.onBackLongPressed()) {
                return true
            }
        }
        return false
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

    final override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        if (shouldUseCustomBackLongPress() && keyCode == KeyEvent.KEYCODE_BACK) {
            backLongPressJob?.cancel()
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
    internal open fun getIntentSource(intent: SafeIntent): Event.OpenedApp.Source? {
        return when {
            intent.isLauncherIntent -> Event.OpenedApp.Source.APP_ICON
            intent.action == Intent.ACTION_VIEW -> Event.OpenedApp.Source.LINK
            else -> null
        }
    }

    protected open fun getIntentAllSource(intent: SafeIntent): Event.AppReceivedIntent.Source? {
        return when {
            intent.isLauncherIntent -> Event.AppReceivedIntent.Source.APP_ICON
            intent.action == Intent.ACTION_VIEW -> Event.AppReceivedIntent.Source.LINK
            else -> Event.AppReceivedIntent.Source.UNKNOWN
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
            navigationToolbar = navigationToolbarStub.inflate() as Toolbar

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
            AppBarConfiguration.Builder(*topLevelDestinationIds).build()
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
        requestDesktopMode: Boolean = false
    ) {
        openToBrowser(from, customTabSessionId)
        load(searchTermOrURL, newTab, engine, forceSearch, flags, requestDesktopMode)
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
        customTabSessionId: String?
    ): NavDirections? = when (from) {
        BrowserDirection.FromGlobal ->
            NavGraphDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromHome ->
            HomeFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromSearchDialog ->
            SearchDialogFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromSettings ->
            SettingsFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromBookmarks ->
            BookmarkFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromHistory ->
            HistoryFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromTrackingProtectionExceptions ->
            TrackingProtectionExceptionsFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromAbout ->
            AboutFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromTrackingProtection ->
            TrackingProtectionFragmentDirections.actionGlobalBrowser(customTabSessionId)
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
    }

    /**
     * Loads a URL or performs a search (depending on the value of [searchTermOrURL]).
     *
     * @param flags Flags that will be used when loading the URL (not applied to searches).
     */
    private fun load(
        searchTermOrURL: String,
        newTab: Boolean,
        engine: SearchEngine?,
        forceSearch: Boolean,
        flags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none(),
        requestDesktopMode: Boolean = false
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
                    private = private
                )
            } else {
                components.useCases.sessionUseCases.loadUrl(
                    url = searchTermOrURL.toNormalizedUrl(),
                    flags = flags
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
                        SessionState.Source.USER_ENTERED,
                        true,
                        mode.isPrivate,
                        searchEngine = engine
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
                "newTab: $newTab"
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
        if (settings().shouldReturnToBrowser &&
            !browsingModeManager.mode.isPrivate
        ) {
            openToBrowser(BrowserDirection.FromGlobal, null)
        }
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

    fun updateSecureWindowFlags(mode: BrowsingMode = browsingModeManager.mode) {
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
            webExtensionTitle = webExtensionState.name
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

    private fun captureSnapshotTelemetryMetrics() = CoroutineScope(Dispatchers.IO).launch {
        // PWA
        val recentlyUsedPwaCount = components.core.webAppShortcutManager.recentlyUsedWebAppsCount(
            activeThresholdMs = PWA_RECENTLY_USED_THRESHOLD
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

    private fun isFenixTheDefaultBrowser() {
        // Launch this on a background thread so as not to affect startup performance
        lifecycleScope.launch(IO) {
            if (
                settings().checkIfFenixIsDefaultBrowserOnAppResume()
            ) {
                metrics.track(Event.ChangedToDefaultBrowser)
            }

            DefaultBrowserNotificationWorker.setDefaultBrowserNotificationIfNeeded(applicationContext)
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

    @VisibleForTesting
    internal fun getSettings(): Settings = settings()

    private fun shouldNavigateBrowserFragmentOnCouldStart(savedInstanceState: Bundle?): Boolean {
        return isActivityColdStarted(intent, savedInstanceState) &&
            !externalSourceIntentProcessors.any {
                it.process(
                    intent,
                    navHost.navController,
                    this.intent
                )
            }
    }

    companion object {
        const val OPEN_TO_BROWSER = "open_to_browser"
        const val OPEN_TO_BROWSER_AND_LOAD = "open_to_browser_and_load"
        const val OPEN_TO_SEARCH = "open_to_search"
        const val PRIVATE_BROWSING_MODE = "private_browsing_mode"
        const val EXTRA_DELETE_PRIVATE_TABS = "notification_delete_and_open"
        const val EXTRA_OPENED_FROM_NOTIFICATION = "notification_open"
        const val START_IN_RECENTS_SCREEN = "start_in_recents_screen"

        // PWA must have been used within last 30 days to be considered "recently used" for the
        // telemetry purposes.
        const val PWA_RECENTLY_USED_THRESHOLD = DateUtils.DAY_IN_MILLIS * 30L
    }
}
