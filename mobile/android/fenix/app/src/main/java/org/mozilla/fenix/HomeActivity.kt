/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.os.StrictMode
import android.util.AttributeSet
import android.view.View
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
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.synthetic.main.activity_home.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.BrowserTabsTray
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.contextmenu.DefaultSelectionActionDelegate
import mozilla.components.feature.search.BrowserStoreSearchAdapter
import mozilla.components.feature.search.SearchAdapter
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import mozilla.components.support.ktx.android.content.share
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import mozilla.components.support.locale.LocaleAwareAppCompatActivity
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.toSafeIntent
import mozilla.components.support.webextensions.WebExtensionPopupFeature
import org.mozilla.fenix.browser.UriOpenedObserver
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.DefaultBrowsingModeManager
import org.mozilla.fenix.components.metrics.BreadcrumbsRecorder
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.exceptions.ExceptionsFragmentDirections
import org.mozilla.fenix.ext.alreadyOnDestination
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.resetPoliciesAfter
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
import org.mozilla.fenix.settings.SettingsFragmentDirections
import org.mozilla.fenix.settings.TrackingProtectionFragmentDirections
import org.mozilla.fenix.settings.about.AboutFragmentDirections
import org.mozilla.fenix.settings.logins.SavedLoginsAuthFragmentDirections
import org.mozilla.fenix.settings.search.AddSearchEngineFragmentDirections
import org.mozilla.fenix.settings.search.EditCustomSearchEngineFragmentDirections
import org.mozilla.fenix.share.AddNewDeviceFragmentDirections
import org.mozilla.fenix.sync.SyncedTabsFragmentDirections
import org.mozilla.fenix.tabtray.FenixTabsAdapter
import org.mozilla.fenix.tabtray.TabTrayDialogFragment
import org.mozilla.fenix.theme.DefaultThemeManager
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.BrowsersCache
import org.mozilla.fenix.utils.RunWhenReadyQueue

/**
 * The main activity of the application. The application is primarily a single Activity (this one)
 * with fragments switching out to display different views. The most important views shown here are the:
 * - home screen
 * - browser screen
 */
@SuppressWarnings("TooManyFunctions", "LargeClass")
open class HomeActivity : LocaleAwareAppCompatActivity() {

    private var webExtScope: CoroutineScope? = null
    lateinit var themeManager: ThemeManager
    lateinit var browsingModeManager: BrowsingModeManager
    private lateinit var sessionObserver: SessionManager.Observer

    private var isVisuallyComplete = false

    private var visualCompletenessQueue: RunWhenReadyQueue? = null

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

        if (isActivityColdStarted(intent, savedInstanceState)) {
            externalSourceIntentProcessors.any { it.process(intent, navHost.navController, this.intent) }
        }

        Performance.processIntentIfPerformanceTest(intent, this)

        if (settings().isTelemetryEnabled) {
            lifecycle.addObserver(BreadcrumbsRecorder(components.analytics.crashReporter,
                navHost.navController, ::getBreadcrumbMessage))

            intent
                ?.toSafeIntent()
                ?.let(::getIntentSource)
                ?.also { components.analytics.metrics.track(Event.OpenedApp(it)) }
        }
        supportActionBar?.hide()

        lifecycle.addObservers(
            webExtensionPopupFeature,
            StartupTimeline.homeActivityLifecycleObserver
        )
        StartupTimeline.onActivityCreateEndHome(this)
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
                    components.backgroundServices.accountManager.syncNowAsync(SyncReason.Startup, debounce = true)
                }
            }
        }
    }

    final override fun onPause() {
        if (settings().lastKnownMode.isPrivate) {
            window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        }
        super.onPause()

        // Every time the application goes into the background, it is possible that the user
        // is about to change the browsers installed on their system. Therefore, we reset the cache of
        // all the installed browsers.
        //
        // NB: There are ways for the user to install new products without leaving the browser.
        BrowsersCache.resetAll()
    }

    /**
     * Handles intents received when the activity is open.
     */
    final override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        intent ?: return

        val intentProcessors = listOf(CrashReporterIntentProcessor()) + externalSourceIntentProcessors
        val intentHandled = intentProcessors.any { it.process(intent, navHost.navController, this.intent) }
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
                appName = getString(R.string.app_name)
            ) {
                share(it)
            }
        }.asView()
        TabsTray::class.java.name -> {
            val layout = LinearLayoutManager(context).apply {
                reverseLayout = true
                stackFromEnd = true
            }

            val thumbnailLoader = ThumbnailLoader(components.core.thumbnailStorage)
            val adapter = FenixTabsAdapter(context, thumbnailLoader)

            BrowserTabsTray(context, attrs, 0, adapter, layout)
        }
        else -> super.onCreateView(parent, name, context, attrs)
    }

    final override fun onBackPressed() {
        supportFragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is UserInteractionHandler && it.onBackPressed()) {
                return
            }
        }
        super.onBackPressed()
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
    fun getSupportActionBarAndInflateIfNecessary(): ActionBar {
        // Add ids to this that we don't want to have a toolbar back button
        if (!isToolbarInflated) {
            navigationToolbar = navigationToolbarStub.inflate() as Toolbar

            setSupportActionBar(navigationToolbar)
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
        BrowserDirection.FromExceptions ->
            ExceptionsFragmentDirections.actionGlobalBrowser(customTabSessionId)
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
    }

    private fun load(
        searchTermOrURL: String,
        newTab: Boolean,
        engine: SearchEngine?,
        forceSearch: Boolean
    ) {
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
                        Session.Source.USER_ENTERED,
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
    }

    fun updateThemeForSession(session: Session) {
        val sessionMode = BrowsingMode.fromBoolean(session.private)
        browsingModeManager.mode = sessionMode
    }

    override fun attachBaseContext(base: Context) {
        StrictMode.allowThreadDiskReads().resetPoliciesAfter {
            super.attachBaseContext(base)
        }
    }

    protected open fun createBrowsingModeManager(initialMode: BrowsingMode): BrowsingModeManager {
        return DefaultBrowsingModeManager(initialMode) { newMode ->
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

    @VisibleForTesting
    internal fun isActivityColdStarted(startingIntent: Intent, activityIcicle: Bundle?): Boolean =
        // First time opening this activity in the task.
        // Cold start / start from Recents after back press.
        activityIcicle == null &&
        // Activity was restarted from Recents after it was destroyed by Android while in background
        // in cases of memory pressure / "Don't keep activities".
        startingIntent.flags and Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY == 0

    companion object {
        const val OPEN_TO_BROWSER = "open_to_browser"
        const val OPEN_TO_BROWSER_AND_LOAD = "open_to_browser_and_load"
        const val OPEN_TO_SEARCH = "open_to_search"
        const val PRIVATE_BROWSING_MODE = "private_browsing_mode"
        const val EXTRA_DELETE_PRIVATE_TABS = "notification_delete_and_open"
        const val EXTRA_OPENED_FROM_NOTIFICATION = "notification_open"
        const val delay = 5000L
    }
}
