/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import androidx.annotation.CallSuper
import androidx.annotation.IdRes
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PROTECTED
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavDestination
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.NavigationUI
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.launch
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.toSafeIntent
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.DefaultBrowsingModeManager
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.isSentryEnabled
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.SentryBreadcrumbsRecorder
import org.mozilla.fenix.exceptions.ExceptionsFragmentDirections
import org.mozilla.fenix.ext.alreadyOnDestination
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.intent.CrashReporterIntentProcessor
import org.mozilla.fenix.home.intent.DeepLinkIntentProcessor
import org.mozilla.fenix.home.intent.OpenBrowserIntentProcessor
import org.mozilla.fenix.home.intent.SpeechProcessingIntentProcessor
import org.mozilla.fenix.home.intent.StartSearchIntentProcessor
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentDirections
import org.mozilla.fenix.library.history.HistoryFragmentDirections
import org.mozilla.fenix.search.SearchFragmentDirections
import org.mozilla.fenix.settings.SettingsFragmentDirections
import org.mozilla.fenix.share.ShareFragment
import org.mozilla.fenix.theme.DefaultThemeManager
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.Settings

@SuppressWarnings("TooManyFunctions", "LargeClass")
open class HomeActivity : AppCompatActivity(), ShareFragment.TabsSharedCallback {

    lateinit var themeManager: ThemeManager
    lateinit var browsingModeManager: BrowsingModeManager

    private var sessionObserver: SessionManager.Observer? = null

    private val navHost by lazy {
        supportFragmentManager.findFragmentById(R.id.container) as NavHostFragment
    }

    private val externalSourceIntentProcessors by lazy {
        listOf(
            SpeechProcessingIntentProcessor(this),
            StartSearchIntentProcessor(components.analytics.metrics),
            DeepLinkIntentProcessor(this),
            OpenBrowserIntentProcessor(this, ::getIntentSessionId)
        )
    }

    final override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        components.publicSuffixList.prefetch()
        setupThemeAndBrowsingMode()

        setContentView(R.layout.activity_home)

        setupToolbarAndNavigation()

        if (Settings.getInstance(this).isTelemetryEnabled) {
            if (isSentryEnabled()) {
                lifecycle.addObserver(SentryBreadcrumbsRecorder(navHost.navController, ::getSentryBreadcrumbMessage))
            }

            intent
                ?.toSafeIntent()
                ?.let(::getIntentSource)
                ?.also { components.analytics.metrics.track(Event.OpenedApp(it)) }
        }
        supportActionBar?.hide()
    }

    @CallSuper
    override fun onResume() {
        super.onResume()
        lifecycleScope.launch {
            with(components.backgroundServices) {
                // Make sure accountManager is initialized.
                accountManager.initAsync().await()
                // If we're authenticated, kick-off a sync and a device state refresh.
                accountManager.authenticatedAccount()?.let {
                    accountManager.syncNowAsync(startup = true, debounce = true)
                    it.deviceConstellation().pollForEventsAsync().await()
                }
            }
        }
    }

    /**
     * Handles intents received when the activity is open.
     */
    final override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        intent ?: return

        val intentProcessors = listOf(CrashReporterIntentProcessor()) + externalSourceIntentProcessors
        intentProcessors.any { it.process(intent, navHost.navController, this.intent) }
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
        EngineView::class.java.name -> components.core.engine.createView(context, attrs).asView()
        else -> super.onCreateView(parent, name, context, attrs)
    }

    final override fun onBackPressed() {
        supportFragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is BackHandler && it.onBackPressed()) {
                return
            }
        }
        super.onBackPressed()
    }

    protected open fun getSentryBreadcrumbMessage(destination: NavDestination): String {
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

    private fun setupThemeAndBrowsingMode() {
        browsingModeManager = createBrowsingModeManager()
        themeManager = createThemeManager()
        themeManager.setActivityTheme(this)
        themeManager.applyStatusBarTheme(this)
    }

    private fun setupToolbarAndNavigation() {
        // Add ids to this that we don't want to have a toolbar back button
        val appBarConfiguration = AppBarConfiguration.Builder().build()
        val navigationToolbar = findViewById<Toolbar>(R.id.navigationToolbar)
        setSupportActionBar(navigationToolbar)
        NavigationUI.setupWithNavController(navigationToolbar, navHost.navController, appBarConfiguration)
        navigationToolbar.setNavigationOnClickListener {
            onBackPressed()
        }

        externalSourceIntentProcessors.any { it.process(intent, navHost.navController, this.intent) }
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
        if (sessionObserver == null)
            sessionObserver = subscribeToSessions()

        if (navHost.navController.alreadyOnDestination(R.id.browserFragment)) return
        @IdRes val fragmentId = if (from.fragmentId != 0) from.fragmentId else null
        val directions = getNavDirections(from, customTabSessionId)

        navHost.navController.nav(fragmentId, directions)
    }

    protected open fun getNavDirections(
        from: BrowserDirection,
        customTabSessionId: String?
    ) = when (from) {
        BrowserDirection.FromGlobal ->
            NavGraphDirections.actionGlobalBrowser(customTabSessionId)
        BrowserDirection.FromHome ->
            HomeFragmentDirections.actionHomeFragmentToBrowserFragment(customTabSessionId)
        BrowserDirection.FromSearch ->
            SearchFragmentDirections.actionSearchFragmentToBrowserFragment(customTabSessionId)
        BrowserDirection.FromSettings ->
            SettingsFragmentDirections.actionSettingsFragmentToBrowserFragment(customTabSessionId)
        BrowserDirection.FromBookmarks ->
            BookmarkFragmentDirections.actionBookmarkFragmentToBrowserFragment(customTabSessionId)
        BrowserDirection.FromHistory ->
            HistoryFragmentDirections.actionHistoryFragmentToBrowserFragment(customTabSessionId)
        BrowserDirection.FromExceptions ->
            ExceptionsFragmentDirections.actionExceptionsFragmentToBrowserFragment(
                customTabSessionId
            )
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
        if (sessionMode != browsingModeManager.mode) {
            browsingModeManager.mode = sessionMode
        }
    }

    protected open fun createBrowsingModeManager(): BrowsingModeManager {
        return DefaultBrowsingModeManager(Settings.getInstance(this)) { mode ->
            themeManager.currentTheme = mode
        }
    }

    protected open fun createThemeManager(): ThemeManager {
        return DefaultThemeManager(browsingModeManager.mode, this)
    }

    @Suppress("ComplexMethod")
    private fun subscribeToSessions(): SessionManager.Observer {
        val singleSessionObserver = object : Session.Observer {
            var urlLoading: String? = null

            override fun onLoadingStateChanged(session: Session, loading: Boolean) {
                if (loading) {
                    urlLoading = session.url
                } else if (urlLoading != null && !session.private) {
                    components.analytics.metrics.track(Event.UriOpened)
                }
            }
        }

        return object : SessionManager.Observer {
            override fun onAllSessionsRemoved() {
                components.core.sessionManager.sessions.forEach {
                    it.unregister(singleSessionObserver)
                }
            }

            override fun onSessionAdded(session: Session) {
                session.register(singleSessionObserver, this@HomeActivity)
            }

            override fun onSessionRemoved(session: Session) {
                session.unregister(singleSessionObserver)
            }

            override fun onSessionsRestored() {
                components.core.sessionManager.sessions.forEach {
                    it.register(singleSessionObserver, this@HomeActivity)
                }
            }
        }.also { components.core.sessionManager.register(it, this) }
    }

    override fun onTabsShared(tabsSize: Int) {
        getRootView()?.let {
            FenixSnackbar.make(it, Snackbar.LENGTH_SHORT).setText(
                getString(
                    if (tabsSize == 1) R.string.sync_sent_tab_snackbar else
                        R.string.sync_sent_tabs_snackbar
                )
            ).show()
        }
    }

    companion object {
        const val OPEN_TO_BROWSER = "open_to_browser"
        const val OPEN_TO_BROWSER_AND_LOAD = "open_to_browser_and_load"
        const val OPEN_TO_SEARCH = "open_to_search"
    }
}
