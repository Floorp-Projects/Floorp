/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.NavigationUI
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.intent.IntentProcessor
import mozilla.components.lib.crash.Crash
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentDirections
import org.mozilla.fenix.library.bookmarks.selectfolder.SelectBookmarkFolderFragmentDirections
import org.mozilla.fenix.library.history.HistoryFragmentDirections
import org.mozilla.fenix.search.SearchFragmentDirections
import org.mozilla.fenix.settings.PairFragmentDirections
import org.mozilla.fenix.settings.SettingsFragmentDirections
import org.mozilla.fenix.settings.TurnOnSyncFragmentDirections
import org.mozilla.fenix.utils.Settings

@SuppressWarnings("TooManyFunctions")
open class HomeActivity : AppCompatActivity() {
    open val isCustomTab = false
    private var sessionObserver: SessionManager.Observer? = null

    val themeManager = DefaultThemeManager().also {
        it.onThemeChange = { theme ->
            setTheme(theme)
            recreate()
        }
    }

    private val navHost by lazy {
        supportFragmentManager.findFragmentById(R.id.container) as NavHostFragment
    }

    lateinit var browsingModeManager: DefaultBrowsingModeManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        themeManager.temporaryThemeManagerStorage =
            when (Settings.getInstance(this).usePrivateMode) {
                true -> ThemeManager.Theme.Private
                false -> ThemeManager.Theme.Normal
            }
        setTheme(themeManager.currentTheme)
        DefaultThemeManager.applyStatusBarTheme(window, themeManager, this)
        browsingModeManager = DefaultBrowsingModeManager(this)

        setContentView(R.layout.activity_home)

        // Add ids to this that we don't want to have a toolbar back button
        val appBarConfiguration = AppBarConfiguration.Builder().build()
        val navigationToolbar = findViewById<Toolbar>(R.id.navigationToolbar)
        setSupportActionBar(navigationToolbar)
        NavigationUI.setupWithNavController(navigationToolbar, navHost.navController, appBarConfiguration)
        supportActionBar?.hide()

        intent
            ?.let { SafeIntent(it) }
            ?.let {
                when {
                    isCustomTab -> Event.OpenedApp.Source.CUSTOM_TAB
                    it.isLauncherIntent -> Event.OpenedApp.Source.APP_ICON
                    it.action == Intent.ACTION_VIEW -> Event.OpenedApp.Source.LINK
                    else -> null
                }
            }
            ?.also { components.analytics.metrics.track(Event.OpenedApp(it)) }

        handleOpenedFromExternalSourceIfNecessary(intent)
    }

    override fun onDestroy() {
        themeManager.onThemeChange = null
        sessionObserver?.let { components.core.sessionManager.unregister(it) }
        super.onDestroy()
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        handleCrashIfNecessary(intent)
        handleOpenedFromExternalSourceIfNecessary(intent)
    }

    override fun onCreateView(
        parent: View?,
        name: String,
        context: Context,
        attrs: AttributeSet
    ): View? =
        when (name) {
            EngineView::class.java.name -> components.core.engine.createView(context, attrs).asView()
            else -> super.onCreateView(parent, name, context, attrs)
        }

    override fun onBackPressed() {
        supportFragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is BackHandler && it.onBackPressed()) {
                return
            }
        }
        super.onBackPressed()
    }

    private fun handleCrashIfNecessary(intent: Intent?) {
        if (intent == null) {
            return
        }
        if (!Crash.isCrashIntent(intent)) {
            return
        }

        openToCrashReporter(intent)
    }

    private fun openToCrashReporter(intent: Intent) {
        val directions = NavGraphDirections.actionGlobalCrashReporter(intent)
        navHost.navController.navigate(directions)
    }

    private fun handleOpenedFromExternalSourceIfNecessary(intent: Intent?) {
        if (intent?.extras?.getBoolean(OPEN_TO_BROWSER) != true) { return }

        this.intent.putExtra(OPEN_TO_BROWSER, false)
        var customTabSessionId: String? = null

        if (isCustomTab) {
            customTabSessionId = SafeIntent(intent).getStringExtra(IntentProcessor.ACTIVE_SESSION_ID)
        }

        openToBrowser(BrowserDirection.FromGlobal, customTabSessionId)
    }

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

    @Suppress("ComplexMethod")
    fun openToBrowser(from: BrowserDirection, customTabSessionId: String? = null) {
        val directions = when (from) {
            BrowserDirection.FromGlobal -> NavGraphDirections.actionGlobalBrowser(customTabSessionId)
            BrowserDirection.FromHome -> HomeFragmentDirections.actionHomeFragmentToBrowserFragment(customTabSessionId)
            BrowserDirection.FromSearch ->
                SearchFragmentDirections.actionSearchFragmentToBrowserFragment(customTabSessionId)
            BrowserDirection.FromSettings ->
                SettingsFragmentDirections.actionSettingsFragmentToBrowserFragment(customTabSessionId)
            BrowserDirection.FromBookmarks ->
                BookmarkFragmentDirections.actionBookmarkFragmentToBrowserFragment(customTabSessionId)
            BrowserDirection.FromBookmarksFolderSelect ->
                SelectBookmarkFolderFragmentDirections
                    .actionBookmarkSelectFolderFragmentToBrowserFragment(customTabSessionId)
            BrowserDirection.FromHistory ->
                HistoryFragmentDirections.actionHistoryFragmentToBrowserFragment(customTabSessionId)
            BrowserDirection.FromPair -> PairFragmentDirections.actionPairFragmentToBrowserFragment(customTabSessionId)
            BrowserDirection.FromTurnOnSync -> TurnOnSyncFragmentDirections.actionTurnOnSyncFragmentToBrowserFragment(
                customTabSessionId
            )
        }
        if (sessionObserver == null)
            sessionObserver = subscribeToSessions()

        navHost.navController.navigate(directions)
    }

    private fun load(searchTermOrURL: String, newTab: Boolean, engine: SearchEngine?, forceSearch: Boolean) {
        val isPrivate = this.browsingModeManager.isPrivate

        val loadUrlUseCase = if (newTab) {
            if (isPrivate) {
                components.useCases.tabsUseCases.addPrivateTab
            } else {
                components.useCases.tabsUseCases.addTab
            }
        } else components.useCases.sessionUseCases.loadUrl

        val searchUseCase: (String) -> Unit = { searchTerms ->
            if (newTab) {
                components.useCases.searchUseCases.newTabSearch
                    .invoke(searchTerms, Session.Source.USER_ENTERED, true, isPrivate, searchEngine = engine)
            } else components.useCases.searchUseCases.defaultSearch.invoke(searchTerms, engine)
        }

        if (!forceSearch && searchTermOrURL.isUrl()) {
            loadUrlUseCase.invoke(searchTermOrURL.toNormalizedUrl())
        } else {
            searchUseCase.invoke(searchTermOrURL)
        }
    }

    private val singleSessionObserver = object : Session.Observer {
        var urlLoading: String? = null

        override fun onLoadingStateChanged(session: Session, loading: Boolean) {
            super.onLoadingStateChanged(session, loading)

            if (loading) urlLoading = session.url
            else if (urlLoading != null && !session.private)
                components.analytics.metrics.track(Event.UriOpened)
        }
    }

    fun updateThemeForSession(session: Session) {
        if (session.private && !themeManager.currentTheme.isPrivate()) {
            browsingModeManager.mode = BrowsingModeManager.Mode.Private
        } else if (!session.private && themeManager.currentTheme.isPrivate()) {
            browsingModeManager.mode = BrowsingModeManager.Mode.Normal
        }
    }

    private fun subscribeToSessions(): SessionManager.Observer {

        return object : SessionManager.Observer {
            override fun onAllSessionsRemoved() {
                super.onAllSessionsRemoved()
                components.core.sessionManager.sessions.forEach {
                    it.unregister(singleSessionObserver)
                }
            }

            override fun onSessionAdded(session: Session) {
                super.onSessionAdded(session)
                session.register(singleSessionObserver)
            }

            override fun onSessionRemoved(session: Session) {
                super.onSessionRemoved(session)
                session.unregister(singleSessionObserver)
            }

            override fun onSessionsRestored() {
                super.onSessionsRestored()
                components.core.sessionManager.sessions.forEach {
                    it.register(singleSessionObserver)
                }
            }
        }.also { components.core.sessionManager.register(it) }
    }

    companion object {
        const val OPEN_TO_BROWSER = "open_to_browser"
    }
}

enum class BrowserDirection {
    FromGlobal, FromHome, FromSearch, FromSettings, FromBookmarks,
    FromBookmarksFolderSelect, FromHistory, FromPair, FromTurnOnSync
}
