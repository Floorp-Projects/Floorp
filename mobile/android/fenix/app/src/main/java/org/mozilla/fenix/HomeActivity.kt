/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import androidx.annotation.IdRes
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavController
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.NavigationUI
import com.google.android.material.snackbar.Snackbar
import io.sentry.Sentry
import io.sentry.event.Breadcrumb
import io.sentry.event.BreadcrumbBuilder
import kotlinx.coroutines.launch
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.intent.EXTRA_SESSION_ID
import mozilla.components.concept.engine.EngineView
import mozilla.components.lib.crash.Crash
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.isSentryEnabled
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.share.ShareFragment
import org.mozilla.fenix.utils.Settings

@SuppressWarnings("TooManyFunctions", "LargeClass")
open class HomeActivity : AppCompatActivity(), ShareFragment.TabsSharedCallback {
    open val isCustomTab = false
    private var sessionObserver: SessionManager.Observer? = null

    lateinit var themeManager: ThemeManager

    private val navHost by lazy {
        supportFragmentManager.findFragmentById(R.id.container) as NavHostFragment
    }

    lateinit var browsingModeManager: BrowsingModeManager

    private val onDestinationChangedListener =
        NavController.OnDestinationChangedListener { _, dest, _ ->
            val fragmentName = resources.getResourceEntryName(dest.id)
            Sentry.getContext().recordBreadcrumb(
                BreadcrumbBuilder()
                    .setCategory("DestinationChanged")
                    .setMessage("Changing to fragment $fragmentName, isCustomTab: $isCustomTab")
                    .setLevel(Breadcrumb.Level.INFO)
                    .build()
            )
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        components.publicSuffixList.prefetch()
        browsingModeManager = createBrowsingModeManager()
        themeManager = createThemeManager(browsingModeManager.mode)

        setContentView(R.layout.activity_home)

        setupToolbarAndNavigation()

        if (Settings.getInstance(this).isTelemetryEnabled && isSentryEnabled()) {
            navHost.navController.addOnDestinationChangedListener(onDestinationChangedListener)
        }

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

    private fun setupToolbarAndNavigation() {
        // Add ids to this that we don't want to have a toolbar back button
        val appBarConfiguration = AppBarConfiguration.Builder().build()
        val navigationToolbar = findViewById<Toolbar>(R.id.navigationToolbar)
        setSupportActionBar(navigationToolbar)
        NavigationUI.setupWithNavController(
            navigationToolbar,
            navHost.navController,
            appBarConfiguration
        )
        navigationToolbar.setNavigationOnClickListener {
            onBackPressed()
        }
        supportActionBar?.hide()
    }

    override fun onDestroy() {
        sessionObserver?.let { components.core.sessionManager.unregister(it) }
        navHost.navController.removeOnDestinationChangedListener(onDestinationChangedListener)
        super.onDestroy()
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        handleCrashIfNecessary(intent)
        handleOpenedFromExternalSourceIfNecessary(intent)
    }

    override fun onResume() {
        super.onResume()
        lifecycleScope.launch {
            with(components.backgroundServices) {
                // Make sure accountManager is initialized.
                accountManager.initAsync().await()
                // If we're authenticated, kick-off a sync and a device state refresh.
                accountManager.authenticatedAccount()?.let {
                    accountManager.syncNowAsync(startup = true)
                    it.deviceConstellation().refreshDeviceStateAsync().await()
                }
            }
        }
    }

    override fun onCreateView(
        parent: View?,
        name: String,
        context: Context,
        attrs: AttributeSet
    ): View? =
        when (name) {
            EngineView::class.java.name -> components.core.engine.createView(
                context,
                attrs
            ).asView()
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
        if (intent?.extras?.getBoolean(OPEN_TO_BROWSER_AND_LOAD) == true) {
            this.intent.putExtra(OPEN_TO_BROWSER_AND_LOAD, false)
            openToBrowserAndLoad(
                intent.getStringExtra(
                    IntentReceiverActivity.SPEECH_PROCESSING
                ), true, BrowserDirection.FromGlobal, forceSearch = true
            )
            return
        } else if (intent?.extras?.getBoolean(OPEN_TO_SEARCH) == true) {
            this.intent.putExtra(OPEN_TO_SEARCH, false)
            components.analytics.metrics.track(Event.SearchWidgetNewTabPressed)
            navHost.navController.nav(null, NavGraphDirections.actionGlobalSearch(null))
            return
        }

        if (intent?.extras?.getBoolean(OPEN_TO_BROWSER) != true) return

        this.intent.putExtra(OPEN_TO_BROWSER, false)
        var customTabSessionId: String? = null

        if (isCustomTab) {
            customTabSessionId = SafeIntent(intent).getStringExtra(EXTRA_SESSION_ID)
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

    fun openToBrowser(from: BrowserDirection, customTabSessionId: String? = null) {
        if (sessionObserver == null)
            sessionObserver = subscribeToSessions()

        with(navHost.navController) {
            if (currentDestination?.id == R.id.browserFragment || popBackStack(
                    R.id.browserFragment,
                    false
                )
            ) return
        }

        @IdRes val fragmentId = if (from.fragmentId != 0) from.fragmentId else null
        navHost.navController.nav(fragmentId, NavGraphDirections.actionGlobalBrowser(customTabSessionId))
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

    private val singleSessionObserver = object : Session.Observer {
        var urlLoading: String? = null

        override fun onLoadingStateChanged(session: Session, loading: Boolean) {
            if (loading) {
                urlLoading = session.url
            } else if (urlLoading != null && !session.private) {
                components.analytics.metrics.track(Event.UriOpened)
            }
        }
    }

    fun updateThemeForSession(session: Session) {
        val sessionMode = BrowsingMode.fromBoolean(session.private)
        if (sessionMode != browsingModeManager.mode) {
            browsingModeManager.mode = sessionMode
        }
    }

    private fun createBrowsingModeManager(): BrowsingModeManager {
        return if (isCustomTab) {
            CustomTabBrowsingModeManager()
        } else {
            DefaultBrowsingModeManager(Settings.getInstance(this)) { mode ->
                themeManager.currentTheme = mode
            }
        }
    }

    private fun createThemeManager(currentTheme: BrowsingMode): ThemeManager {
        val themeManager = if (isCustomTab) {
            CustomTabThemeManager()
        } else {
            DefaultThemeManager(currentTheme) {
                themeManager.setActivityTheme(this)
                recreate()
            }
        }
        themeManager.setActivityTheme(this)
        themeManager.applyStatusBarTheme(this)
        return themeManager
    }

    private fun subscribeToSessions(): SessionManager.Observer {

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
        this@HomeActivity.getRootView()?.let {
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
