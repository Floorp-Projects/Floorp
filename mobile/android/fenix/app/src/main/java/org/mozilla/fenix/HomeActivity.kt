/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.StrictMode
import android.text.format.DateUtils
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.View
import android.view.ViewConfiguration
import android.view.WindowManager
import androidx.annotation.CallSuper
import androidx.annotation.IdRes
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PROTECTED
import androidx.appcompat.app.ActionBar
import androidx.appcompat.widget.Toolbar
import androidx.core.view.doOnPreDraw
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.NavigationUI
import kotlinx.android.synthetic.main.activity_home.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.contextmenu.DefaultSelectionActionDelegate
import mozilla.components.feature.search.BrowserStoreSearchAdapter
import mozilla.components.feature.search.SearchAdapter
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import mozilla.components.support.ktx.android.content.call
import mozilla.components.support.ktx.android.content.email
import mozilla.components.support.ktx.android.content.share
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import mozilla.components.support.locale.LocaleAwareAppCompatActivity
import mozilla.components.support.utils.RunWhenReadyQueue
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.toSafeIntent
import mozilla.components.support.webextensions.WebExtensionPopupFeature
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.addons.AddonDetailsFragmentDirections
import org.mozilla.fenix.addons.AddonPermissionsDetailsFragmentDirections
import org.mozilla.fenix.browser.UriOpenedObserver
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.DefaultBrowsingModeManager
import org.mozilla.fenix.components.metrics.BreadcrumbsRecorder
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.exceptions.trackingprotection.TrackingProtectionExceptionsFragmentDirections
import org.mozilla.fenix.ext.alreadyOnDestination
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.resetPoliciesAfter
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.intent.CrashReporterIntentProcessor
import org.mozilla.fenix.home.intent.DeepLinkIntentProcessor
import org.mozilla.fenix.home.intent.OpenBrowserIntentProcessor
import org.mozilla.fenix.home.intent.OpenSpecificTabIntentProcessor
import org.mozilla.fenix.home.intent.SpeechProcessingIntentProcessor
import org.mozilla.fenix.home.intent.StartSearchIntentProcessor
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentDirections
import org.mozilla.fenix.library.history.HistoryFragmentDirections
import org.mozilla.fenix.perf.Performance
import org.mozilla.fenix.perf.StartupTimeline
import org.mozilla.fenix.search.SearchFragmentDirections
import org.mozilla.fenix.session.NotificationSessionObserver
import org.mozilla.fenix.settings.SettingsFragmentDirections
import org.mozilla.fenix.settings.TrackingProtectionFragmentDirections
import org.mozilla.fenix.settings.about.AboutFragmentDirections
import org.mozilla.fenix.settings.logins.fragment.LoginDetailFragmentDirections
import org.mozilla.fenix.settings.logins.fragment.SavedLoginsAuthFragmentDirections
import org.mozilla.fenix.settings.search.AddSearchEngineFragmentDirections
import org.mozilla.fenix.settings.search.EditCustomSearchEngineFragmentDirections
import org.mozilla.fenix.share.AddNewDeviceFragmentDirections
import org.mozilla.fenix.sync.SyncedTabsFragmentDirections
import org.mozilla.fenix.tabtray.TabTrayDialogFragment
import org.mozilla.fenix.theme.DefaultThemeManager
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.BrowsersCache

/**
 * The main activity of the application. The application is primarily a single Activity (this one)
 * with fragments switching out to display different views. The most important views shown here are the:
 * - home screen
 * - browser screen
 */
@OptIn(ExperimentalCoroutinesApi::class)
@SuppressWarnings("TooManyFunctions", "LargeClass")
open class HomeActivity : LocaleAwareAppCompatActivity(), NavHostActivity {

    private var webExtScope: CoroutineScope? = null
    lateinit var themeManager: ThemeManager
    lateinit var browsingModeManager: BrowsingModeManager
    private lateinit var sessionObserver: SessionManager.Observer

    private var isVisuallyComplete = false

    private var visualCompletenessQueue: RunWhenReadyQueue? = null
    private var privateNotificationObserver: NotificationSessionObserver? = null

    private var isToolbarInflated = false

    private val webExtensionPopupFeature by lazy {
        WebExtensionPopupFeature(components.core.store, ::openPopup)
    }

    private val navHost by lazy {
        supportFragmentManager.findFragmentById(R.id.container) as NavHostFragment
    }

    private val externalSourceIntentProcessors by lazy {
        listOf(
            SpeechProcessingIntentProcessor(this, components.analytics.metrics),
            StartSearchIntentProcessor(components.analytics.metrics),
            DeepLinkIntentProcessor(this),
            OpenBrowserIntentProcessor(this, ::getIntentSessionId),
            OpenSpecificTabIntentProcessor(this)
        )
    }

    // See onKeyDown for why this is necessary
    private var backLongPressJob: Job? = null

    private lateinit var navigationToolbar: Toolbar

    final override fun onCreate(savedInstanceState: Bundle?) {
        StrictModeManager.changeStrictModePolicies(supportFragmentManager)
        // There is disk read violations on some devices such as samsung and pixel for android 9/10
        StrictMode.allowThreadDiskReads().resetPoliciesAfter {
            super.onCreate(savedInstanceState)
        }

        components.publicSuffixList.prefetch()

        setupThemeAndBrowsingMode(getModeFromIntentOrLastKnown(intent))
        setContentView(R.layout.activity_home)

        // Must be after we set the content view
        if (isVisuallyComplete) {
            rootContainer.doOnPreDraw {
                // This delay is temporary. We are delaying 5 seconds until the performance
                // team can locate the real point of visual completeness.
                it.postDelayed({
                    visualCompletenessQueue!!.ready()
                }, delay)
            }
        }

        sessionObserver = UriOpenedObserver(this)

        checkPrivateShortcutEntryPoint(intent)
        privateNotificationObserver = NotificationSessionObserver(applicationContext).also {
            it.start()
        }

        if (isActivityColdStarted(intent, savedInstanceState)) {
            externalSourceIntentProcessors.any {
                it.process(
                    intent,
                    navHost.navController,
                    this.intent
                )
            }
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

        setAppAllStartTelemetry(intent.toSafeIntent())

        StartupTimeline.onActivityCreateEndHome(this) // DO NOT MOVE ANYTHING BELOW HERE.
    }

    protected open fun setAppAllStartTelemetry(safeIntent: SafeIntent) {
        components.appAllSourceStartTelemetry.receivedIntentInHomeActivity(safeIntent)
    }

    @CallSuper
    override fun onResume() {
        super.onResume()

        components.backgroundServices.accountManagerAvailableQueue.runIfReadyOrQueue {
            lifecycleScope.launch {
                // Make sure accountManager is initialized.
                components.backgroundServices.accountManager.initAsync().await()
                // If we're authenticated, kick-off a sync and a device state refresh.
                components.backgroundServices.accountManager.authenticatedAccount()?.let {
                    components.backgroundServices.accountManager.syncNowAsync(
                        SyncReason.Startup,
                        debounce = true
                    )
                }
            }
        }

        // Launch this on a background thread so as not to affect startup performance
        lifecycleScope.launch(IO) {
            if (
                settings().isDefaultBrowser() &&
                settings().wasDefaultBrowserOnLastPause != settings().isDefaultBrowser()
            ) {
                metrics.track(Event.ChangedToDefaultBrowser)
            }
        }
    }

    final override fun onPause() {
        if (settings().lastKnownMode.isPrivate) {
            window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        }

        if (settings().wasDefaultBrowserOnLastPause != settings().isDefaultBrowser()
        ) {
            settings().wasDefaultBrowserOnLastPause = settings().isDefaultBrowser()
        }

        super.onPause()

        // Every time the application goes into the background, it is possible that the user
        // is about to change the browsers installed on their system. Therefore, we reset the cache of
        // all the installed browsers.
        //
        // NB: There are ways for the user to install new products without leaving the browser.
        BrowsersCache.resetAll()
    }

    override fun onDestroy() {
        super.onDestroy()
        privateNotificationObserver?.stop()
    }

    /**
     * Handles intents received when the activity is open.
     */
    final override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        intent ?: return

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
                ?.let { it as? TabTrayDialogFragment }
                ?.also { it.dismissAllowingStateLoss() }
        }

        // Note: This does not work in case of an user sending an intent with ACTION_VIEW
        // for example, launch the application, and than use adb to send an intent with
        // ACTION_VIEW to open a link. In this case, we will get multiple telemetry events.
        intent
            .toSafeIntent()
            .let(::getIntentAllSource)
            ?.also { components.analytics.metrics.track(Event.AppReceivedIntent(it)) }

        setAppAllStartTelemetry(intent.toSafeIntent())
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
                getSearchAdapter(components.core.store),
                resources = context.resources,
                shareTextClicked = { share(it) },
                emailTextClicked = { email(it) },
                callTextClicked = { call(it) },
                actionSorter = ::actionSorter
            )
        }.asView()
        else -> super.onCreateView(parent, name, context, attrs)
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
        order["org.mozilla.geckoview.SELECT_ALL"] = 5
        order["CUSTOM_CONTEXT_MENU_SHARE"] = 6

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

    private fun isAndroidN(): Boolean =
        Build.VERSION.SDK_INT == Build.VERSION_CODES.N || Build.VERSION.SDK_INT == Build.VERSION_CODES.N_MR1

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
        // Android N has broken passing onKeyLongPress events for the back button, so we
        // instead implement the long press behavior ourselves
        // - For short presses, we cancel the callback in onKeyUp
        // - For long presses, the normal keypress is marked as cancelled, hence won't be handled elsewhere
        //   (but Android still provides the haptic feedback), and the long press action is run
        if (isAndroidN() && keyCode == KeyEvent.KEYCODE_BACK) {
            backLongPressJob = lifecycleScope.launch {
                delay(ViewConfiguration.getLongPressTimeout().toLong())
                handleBackLongPress()
            }
        }
        return super.onKeyDown(keyCode, event)
    }

    final override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        if (isAndroidN() && keyCode == KeyEvent.KEYCODE_BACK) {
            backLongPressJob?.cancel()
        }
        return super.onKeyUp(keyCode, event)
    }

    final override fun onKeyLongPress(keyCode: Int, event: KeyEvent?): Boolean {
        // onKeyLongPress is broken in Android N so we don't handle back button long presses here
        // for N. The version check ensures we don't handle back button long presses twice.
        if (!isAndroidN() && keyCode == KeyEvent.KEYCODE_BACK) {
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

    protected open fun getSearchAdapter(store: BrowserStore): SearchAdapter =
        BrowserStoreSearchAdapter(store)

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

    private fun checkPrivateShortcutEntryPoint(intent: Intent) {
        if (intent.hasExtra(OPEN_TO_SEARCH) &&
            (intent.getStringExtra(OPEN_TO_SEARCH) ==
                    StartSearchIntentProcessor.STATIC_SHORTCUT_NEW_PRIVATE_TAB ||
                    intent.getStringExtra(OPEN_TO_SEARCH) ==
                    StartSearchIntentProcessor.PRIVATE_BROWSING_PINNED_SHORTCUT)
        ) {
            NotificationSessionObserver.isStartedFromPrivateShortcut = true
        }
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

    @Suppress("LongParameterList")
    fun openToBrowserAndLoad(
        searchTermOrURL: String,
        newTab: Boolean,
        from: BrowserDirection,
        customTabSessionId: String? = null,
        engine: SearchEngine? = null,
        forceSearch: Boolean = false
    ) {
        openToBrowser(from, customTabSessionId)
        load(searchTermOrURL, newTab, engine, forceSearch)
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
            HomeFragmentDirections.actionHomeFragmentToBrowserFragment(customTabSessionId, true)
        BrowserDirection.FromSearch ->
            SearchFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromSettings ->
            SettingsFragmentDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromSyncedTabs ->
            SyncedTabsFragmentDirections.actionGlobalBrowser(customTabSessionId)
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
    }

    private fun load(
        searchTermOrURL: String,
        newTab: Boolean,
        engine: SearchEngine?,
        forceSearch: Boolean
    ) {
        val startTime = components.core.engine.profiler?.getProfilerTime()
        val mode = browsingModeManager.mode

        val loadUrlUseCase = if (newTab) {
            when (mode) {
                BrowsingMode.Private -> components.useCases.tabsUseCases.addPrivateTab
                BrowsingMode.Normal -> components.useCases.tabsUseCases.addTab
            }
        } else components.useCases.sessionUseCases.loadUrl

        val searchUseCase: (String) -> Unit = { searchTerms ->
            if (newTab) {
                components.useCases.searchUseCases.newTabSearch
                    .invoke(
                        searchTerms,
                        SessionState.Source.USER_ENTERED,
                        true,
                        mode.isPrivate,
                        searchEngine = engine
                    )
            } else components.useCases.searchUseCases.defaultSearch.invoke(searchTerms, engine)
        }

        if (!forceSearch && searchTermOrURL.isUrl()) {
            loadUrlUseCase.invoke(searchTermOrURL.toNormalizedUrl())
        } else {
            searchUseCase.invoke(searchTermOrURL)
        }

        if (components.core.engine.profiler?.isProfilerActive() == true) {
            // Wrapping the `addMarker` method with `isProfilerActive` even though it's no-op when
            // profiler is not active. That way, `text` argument will not create a string builder all the time.
            components.core.engine.profiler?.addMarker("HomeActivity.load", startTime, "newTab: $newTab")
        }
    }

    override fun attachBaseContext(base: Context) {
        StrictMode.allowThreadDiskReads().resetPoliciesAfter {
            super.attachBaseContext(base)
        }
    }

    protected open fun createBrowsingModeManager(initialMode: BrowsingMode): BrowsingModeManager {
        return DefaultBrowsingModeManager(initialMode, components.settings) { newMode ->
            themeManager.currentTheme = newMode
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
    fun postVisualCompletenessQueue(visualCompletenessQueue: RunWhenReadyQueue) {
        isVisuallyComplete = true
        this.visualCompletenessQueue = visualCompletenessQueue
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

    @VisibleForTesting
    internal fun isActivityColdStarted(startingIntent: Intent, activityIcicle: Bundle?): Boolean {
        // First time opening this activity in the task.
        // Cold start / start from Recents after back press.
        return activityIcicle == null &&
                // Activity was restarted from Recents after it was destroyed by Android while in background
                // in cases of memory pressure / "Don't keep activities".
                startingIntent.flags and Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY == 0
    }

    companion object {
        const val OPEN_TO_BROWSER = "open_to_browser"
        const val OPEN_TO_BROWSER_AND_LOAD = "open_to_browser_and_load"
        const val OPEN_TO_SEARCH = "open_to_search"
        const val PRIVATE_BROWSING_MODE = "private_browsing_mode"
        const val EXTRA_DELETE_PRIVATE_TABS = "notification_delete_and_open"
        const val EXTRA_OPENED_FROM_NOTIFICATION = "notification_open"
        const val delay = 5000L
        const val START_IN_RECENTS_SCREEN = "start_in_recents_screen"

        // PWA must have been used within last 30 days to be considered "recently used" for the
        // telemetry purposes.
        const val PWA_RECENTLY_USED_THRESHOLD = DateUtils.DAY_IN_MILLIS * 30L
    }
}
