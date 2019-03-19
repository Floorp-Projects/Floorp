/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import android.view.inputmethod.InputMethodManager
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.NavigationUI
import mozilla.components.browser.session.Session
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
import org.mozilla.fenix.search.SearchFragmentDirections
import org.mozilla.fenix.settings.SettingsFragmentDirections

@SuppressWarnings("TooManyFunctions")
open class HomeActivity : AppCompatActivity() {
    open val isCustomTab = false

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

        setTheme(themeManager.currentTheme)
        DefaultThemeManager.applyStatusBarTheme(window, themeManager, this)

        browsingModeManager = DefaultBrowsingModeManager(this)

        setContentView(R.layout.activity_home)

        val appBarConfiguration = AppBarConfiguration.Builder(setOf(R.id.libraryFragment)).build()
        val navigationToolbar = findViewById<Toolbar>(R.id.navigationToolbar)
        setSupportActionBar(navigationToolbar)
        NavigationUI.setupWithNavController(navigationToolbar, navHost.navController, appBarConfiguration)

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
    
    override fun onResume() {
        super.onResume()
        // There is no session, or it has timed out; we should pop everything to home
        if (components.core.sessionStorage.current() == null) {
            navHost.navController.popBackStack(R.id.homeFragment, false)
        }
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

    override fun onPause() {
        super.onPause()
        hideSoftwareKeyboard()
    }

    private fun hideSoftwareKeyboard() {
        (getSystemService(Activity.INPUT_METHOD_SERVICE) as InputMethodManager).apply {
            currentFocus?.also {
                this.hideSoftInputFromWindow(it.windowToken, 0)
            }
        }
    }

    private fun handleCrashIfNecessary(intent: Intent?) {
        if (intent == null) { return }
        if (!Crash.isCrashIntent(intent)) { return }

        openToCrashReporter(intent)
    }

    private fun openToCrashReporter(intent: Intent) {
        val directions = NavGraphDirections.actionGlobalCrashReporter(intent)
        navHost.navController.navigate(directions)
    }

    private fun handleOpenedFromExternalSourceIfNecessary(intent: Intent?) {
        if (intent?.extras?.getBoolean(OPEN_TO_BROWSER) == true) {
            handleOpenedFromExternalSource()
        }
    }

    private fun handleOpenedFromExternalSource() {
        intent?.putExtra(OPEN_TO_BROWSER, false)
        openToBrowser(SafeIntent(intent).getStringExtra(IntentProcessor.ACTIVE_SESSION_ID), BrowserDirection.FromGlobal)
    }

    fun openToBrowserAndLoad(text: String, sessionId: String? = null, from: BrowserDirection) {
        openToBrowser(sessionId, from)
        load(text, sessionId)
    }

    fun openToBrowser(sessionId: String?, from: BrowserDirection) {
        val directions = when (from) {
            BrowserDirection.FromGlobal -> NavGraphDirections.actionGlobalBrowser(sessionId)
            BrowserDirection.FromHome -> HomeFragmentDirections.actionHomeFragmentToBrowserFragment(sessionId)
            BrowserDirection.FromSearch -> SearchFragmentDirections.actionSearchFragmentToBrowserFragment(sessionId)
            BrowserDirection.FromSettings ->
                SettingsFragmentDirections.actionSettingsFragmentToBrowserFragment(sessionId)
        }

        navHost.navController.navigate(directions)
    }

    private fun load(text: String, sessionId: String?) {
        val isPrivate = this.browsingModeManager.isPrivate

        val loadUrlUseCase = if (sessionId == null) {
            if (isPrivate) {
                components.useCases.tabsUseCases.addPrivateTab
            } else {
                components.useCases.tabsUseCases.addTab
            }
        } else components.useCases.sessionUseCases.loadUrl

        val searchUseCase: (String) -> Unit = { searchTerms ->
            if (sessionId == null) {
                components.useCases.searchUseCases.newTabSearch
                    .invoke(searchTerms, Session.Source.USER_ENTERED, true, isPrivate)
            } else components.useCases.searchUseCases.defaultSearch.invoke(searchTerms)
        }

        if (text.isUrl()) {
            loadUrlUseCase.invoke(text.toNormalizedUrl())
        } else {
            searchUseCase.invoke(text)
        }
    }

    companion object {
        const val OPEN_TO_BROWSER = "open_to_browser"
    }
}

enum class BrowserDirection {
    FromGlobal, FromHome, FromSearch, FromSettings
}
