/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.util.AttributeSet
import android.view.MenuItem
import android.view.View
import android.view.ViewTreeObserver
import androidx.core.content.ContextCompat
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.preference.PreferenceManager
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.concept.engine.EngineView
import mozilla.components.lib.auth.canUseBiometricFeature
import mozilla.components.lib.crash.Crash
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.view.getWindowInsetsController
import mozilla.components.support.locale.LocaleAwareAppCompatActivity
import mozilla.components.support.utils.SafeIntent
import org.mozilla.focus.GleanMetrics.AppOpened
import org.mozilla.focus.GleanMetrics.Notifications
import org.mozilla.focus.R
import org.mozilla.focus.appreview.AppReviewUtils
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings
import org.mozilla.focus.ext.updateSecureWindowFlags
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.fragment.UrlInputFragment
import org.mozilla.focus.navigation.MainActivityNavigation
import org.mozilla.focus.navigation.Navigator
import org.mozilla.focus.perf.Performance
import org.mozilla.focus.session.IntentProcessor
import org.mozilla.focus.shortcut.HomeScreen
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.telemetry.startuptelemetry.StartupPathProvider
import org.mozilla.focus.telemetry.startuptelemetry.StartupTypeTelemetry
import org.mozilla.focus.utils.SupportUtils

private const val REQUEST_TIME_OUT = 2000L

@Suppress("TooManyFunctions", "LargeClass")
open class MainActivity : LocaleAwareAppCompatActivity() {
    private val intentProcessor by lazy {
        IntentProcessor(this, components.tabsUseCases, components.customTabsUseCases)
    }

    private val navigator by lazy { Navigator(components.appStore, MainActivityNavigation(this)) }
    private val tabCount: Int
        get() = components.store.state.privateTabs.size

    private val startupPathProvider = StartupPathProvider()
    private lateinit var startupTypeTelemetry: StartupTypeTelemetry

    override fun onCreate(savedInstanceState: Bundle?) {
        installSplashScreen()

        updateSecureWindowFlags()

        super.onCreate(savedInstanceState)

        // Checks if Activity is currently in PiP mode if launched from external intents, then exits it
        checkAndExitPiP()

        if (!isTaskRoot) {
            if (intent.hasCategory(Intent.CATEGORY_LAUNCHER) && Intent.ACTION_MAIN == intent.action) {
                finish()
                return
            }
        }

        @Suppress("DEPRECATION") // https://github.com/mozilla-mobile/focus-android/issues/5016
        window.decorView.systemUiVisibility =
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN

        window.statusBarColor = ContextCompat.getColor(this, android.R.color.transparent)
        when (resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
            Configuration.UI_MODE_NIGHT_UNDEFINED, // We assume light here per Android doc's recommendation
            Configuration.UI_MODE_NIGHT_NO -> {
                updateLightSystemBars()
            }
            Configuration.UI_MODE_NIGHT_YES -> {
                clearLightSystemBars()
            }
        }
        setContentView(R.layout.activity_main)

        startupPathProvider.attachOnActivityOnCreate(lifecycle, intent)
        startupTypeTelemetry = StartupTypeTelemetry(components.startupStateProvider, startupPathProvider).apply {
            attachOnMainActivityOnCreate(lifecycle)
        }

        val safeIntent = SafeIntent(intent)
        val isTheFirstLaunch = settings.getAppLaunchCount() == 0
        if (isTheFirstLaunch) {
            setSplashScreenPreDrawListener(safeIntent)
        } else {
            showFirstScreen(safeIntent)
        }

        if (intent.hasExtra(HomeScreen.ADD_TO_HOMESCREEN_TAG)) {
            intentProcessor.handleNewIntent(this, safeIntent)
        }

        if (safeIntent.isLauncherIntent) {
            AppOpened.fromIcons.record(AppOpened.FromIconsExtra(AppOpenType.LAUNCH.type))

            TelemetryWrapper.openFromIconEvent()
        }

        val launchCount = settings.getAppLaunchCount()
        PreferenceManager.getDefaultSharedPreferences(this)
            .edit()
            .putInt(getString(R.string.app_launch_count), launchCount + 1)
            .apply()

        AppReviewUtils.showAppReview(this)
    }

    private fun setSplashScreenPreDrawListener(safeIntent: SafeIntent) {
        val content: View = findViewById(android.R.id.content)
        val endTime = System.currentTimeMillis() + REQUEST_TIME_OUT
        content.viewTreeObserver.addOnPreDrawListener(object : ViewTreeObserver.OnPreDrawListener {
            override fun onPreDraw(): Boolean {
                return if (System.currentTimeMillis() >= endTime) {
                    showFirstScreen(safeIntent)
                    content.viewTreeObserver.removeOnPreDrawListener(this)
                    true
                } else {
                    false
                }
            }
        }
        )
    }

    private fun showFirstScreen(safeIntent: SafeIntent) {
        // The performance check was added after the shouldShowFirstRun to take as much of the
        // code path as possible
        if (settings.isFirstRun() &&
            !Performance.processIntentIfPerformanceTest(safeIntent, this)
        ) {
            components.appStore.dispatch(AppAction.ShowFirstRun)
        }
        lifecycle.addObserver(navigator)
    }

    private fun checkAndExitPiP() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && isInPictureInPictureMode && intent != null) {
            // Exit PiP mode
            moveTaskToBack(false)
            startActivity(Intent(this, this::class.java).setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT))
        }
    }

    final override fun onUserLeaveHint() {
        val browserFragment =
            supportFragmentManager.findFragmentByTag(BrowserFragment.FRAGMENT_TAG) as BrowserFragment?
        if (browserFragment is UserInteractionHandler && browserFragment.onHomePressed()) {
            return
        }
        super.onUserLeaveHint()
    }

    override fun onResume() {
        super.onResume()

        TelemetryWrapper.startSession()
        checkBiometricStillValid()
    }

    override fun onPause() {
        val fragmentManager = supportFragmentManager
        val browserFragment =
            fragmentManager.findFragmentByTag(BrowserFragment.FRAGMENT_TAG) as BrowserFragment?
        browserFragment?.cancelAnimation()

        val urlInputFragment =
            fragmentManager.findFragmentByTag(UrlInputFragment.FRAGMENT_TAG) as UrlInputFragment?
        urlInputFragment?.cancelAnimation()

        super.onPause()

        TelemetryWrapper.stopSession()
    }

    override fun onStop() {
        super.onStop()

        TelemetryWrapper.stopMainActivity()
    }

    override fun onNewIntent(unsafeIntent: Intent) {
        if (Crash.isCrashIntent(unsafeIntent)) {
            val browserFragment = supportFragmentManager
                .findFragmentByTag(BrowserFragment.FRAGMENT_TAG) as BrowserFragment?
            val crash = Crash.fromIntent(unsafeIntent)

            browserFragment?.handleTabCrash(crash)
        }
        startupPathProvider.onIntentReceived(intent)
        val intent = SafeIntent(unsafeIntent)

        if (intent.dataString.equals(SupportUtils.OPEN_WITH_DEFAULT_BROWSER_URL)) {
            components.appStore.dispatch(
                AppAction.OpenSettings(
                    page = Screen.Settings.Page.General
                )
            )
            super.onNewIntent(unsafeIntent)
            return
        }

        val action = intent.action

        if (intent.hasExtra(HomeScreen.ADD_TO_HOMESCREEN_TAG)) {
            intentProcessor.handleNewIntent(this, intent)
        }

        if (ACTION_OPEN == action) {
            Notifications.openButtonTapped.record(NoExtras())

            TelemetryWrapper.openNotificationActionEvent()
        }

        if (ACTION_ERASE == action) {
            processEraseAction(intent)
        }

        if (intent.isLauncherIntent) {
            AppOpened.fromIcons.record(AppOpened.FromIconsExtra(AppOpenType.RESUME.type))

            TelemetryWrapper.resumeFromIconEvent()
        }

        super.onNewIntent(unsafeIntent)
    }

    private fun processEraseAction(intent: SafeIntent) {
        val fromShortcut = intent.getBooleanExtra(EXTRA_SHORTCUT, false)
        val fromNotificationAction = intent.getBooleanExtra(EXTRA_NOTIFICATION, false)

        components.tabsUseCases.removeAllTabs()

        if (fromNotificationAction) {
            Notifications.eraseOpenButtonTapped.record(Notifications.EraseOpenButtonTappedExtra(tabCount))
        }

        if (fromShortcut) {
            TelemetryWrapper.eraseShortcutEvent()
        } else if (fromNotificationAction) {
            TelemetryWrapper.eraseAndOpenNotificationActionEvent()
        }
    }

    override fun onCreateView(parent: View?, name: String, context: Context, attrs: AttributeSet): View? {
        return if (name == EngineView::class.java.name) {
            components.engine.createView(context, attrs).asView()
        } else super.onCreateView(parent, name, context, attrs)
    }

    override fun onBackPressed() {
        val fragmentManager = supportFragmentManager

        val urlInputFragment =
            fragmentManager.findFragmentByTag(UrlInputFragment.FRAGMENT_TAG) as UrlInputFragment?
        if (urlInputFragment != null &&
            urlInputFragment.isVisible &&
            urlInputFragment.onBackPressed()
        ) {
            // The URL input fragment has handled the back press. It does its own animations so
            // we do not try to remove it from outside.
            return
        }

        val browserFragment =
            fragmentManager.findFragmentByTag(BrowserFragment.FRAGMENT_TAG) as BrowserFragment?
        if (browserFragment != null &&
            browserFragment.isVisible &&
            browserFragment.onBackPressed()
        ) {
            // The Browser fragment handles back presses on its own because it might just go back
            // in the browsing history.
            return
        }

        val appStore = components.appStore
        if (appStore.state.screen is Screen.Settings || appStore.state.screen is Screen.SitePermissionOptionsScreen) {
            // When on a settings screen we want the same behavior as navigating "up" via the toolbar
            // and therefore dispatch the `NavigateUp` action on the app store.
            val selectedTabId = components.store.state.selectedTabId
            appStore.dispatch(AppAction.NavigateUp(selectedTabId))
            return
        }

        super.onBackPressed()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == android.R.id.home) {
            // We forward an up action to the app store with the NavigateUp action to let the reducer
            // decide to show a different screen.
            val selectedTabId = components.store.state.selectedTabId
            components.appStore.dispatch(AppAction.NavigateUp(selectedTabId))
            return true
        }

        return super.onOptionsItemSelected(item)
    }

    // Handles the edge case of a user removing all enrolled prints while auth was enabled
    private fun checkBiometricStillValid() {
        // Disable biometrics if the user is no longer eligible due to un-enrolling fingerprints:
        if (!canUseBiometricFeature()) {
            PreferenceManager.getDefaultSharedPreferences(this)
                .edit().putBoolean(
                    getString(R.string.pref_key_biometric),
                    false
                ).apply()
        }
    }

    private fun updateLightSystemBars() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            window.statusBarColor = getColorFromAttr(android.R.attr.statusBarColor)
            window.getWindowInsetsController().isAppearanceLightStatusBars = true
        } else {
            window.statusBarColor = Color.BLACK
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // API level can display handle light navigation bar color
            window.getWindowInsetsController().isAppearanceLightNavigationBars = true
            window.navigationBarColor = ContextCompat.getColor(this, android.R.color.transparent)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                window.navigationBarDividerColor =
                    ContextCompat.getColor(this, android.R.color.transparent)
            }
        }
    }

    private fun clearLightSystemBars() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            window.getWindowInsetsController().isAppearanceLightStatusBars = false
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // API level can display handle light navigation bar color
            window.getWindowInsetsController().isAppearanceLightNavigationBars = false
        }
    }

    enum class AppOpenType(val type: String) {
        LAUNCH("Launch"),
        RESUME("Resume")
    }

    companion object {
        const val ACTION_ERASE = "erase"
        const val ACTION_OPEN = "open"

        const val EXTRA_NOTIFICATION = "notification"
        private const val EXTRA_SHORTCUT = "shortcut"
    }
}
