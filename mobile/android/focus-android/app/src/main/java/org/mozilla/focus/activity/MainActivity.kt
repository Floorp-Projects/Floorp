/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.Manifest.permission.POST_NOTIFICATIONS
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
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.ActionBar
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.core.view.isVisible
import androidx.preference.PreferenceManager
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.search.widget.BaseVoiceSearchActivity
import mozilla.components.lib.auth.canUseBiometricFeature
import mozilla.components.lib.crash.Crash
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.view.createWindowInsetsController
import mozilla.components.support.locale.LocaleAwareAppCompatActivity
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.StatusBarUtils
import org.mozilla.experiments.nimbus.initializeTooling
import org.mozilla.focus.GleanMetrics.AppOpened
import org.mozilla.focus.GleanMetrics.Notifications
import org.mozilla.focus.R
import org.mozilla.focus.appreview.AppReviewUtils
import org.mozilla.focus.databinding.ActivityMainBinding
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.setNavigationIcon
import org.mozilla.focus.ext.settings
import org.mozilla.focus.ext.updateSecureWindowFlags
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.fragment.UrlInputFragment
import org.mozilla.focus.navigation.MainActivityNavigation
import org.mozilla.focus.navigation.Navigator
import org.mozilla.focus.searchwidget.ExternalIntentNavigation
import org.mozilla.focus.session.IntentProcessor
import org.mozilla.focus.session.PrivateNotificationFeature
import org.mozilla.focus.shortcut.HomeScreen
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.startuptelemetry.StartupPathProvider
import org.mozilla.focus.telemetry.startuptelemetry.StartupTypeTelemetry
import org.mozilla.focus.utils.SupportUtils

private const val REQUEST_TIME_OUT = 2000L

@Suppress("TooManyFunctions", "LargeClass")
open class MainActivity : LocaleAwareAppCompatActivity() {
    private var isToolbarInflated = false
    private val intentProcessor by lazy {
        IntentProcessor(this, components.tabsUseCases, components.customTabsUseCases)
    }

    private val navigator by lazy { Navigator(components.appStore, MainActivityNavigation(this)) }
    private val tabCount: Int
        get() = components.store.state.privateTabs.size

    private val startupPathProvider = StartupPathProvider()
    private lateinit var startupTypeTelemetry: StartupTypeTelemetry
    private var _binding: ActivityMainBinding? = null
    private val binding get() = _binding!!
    private lateinit var privateNotificationFeature: PrivateNotificationFeature
    private val notificationPermission =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            when {
                granted -> {
                    privateNotificationFeature.start()
                }
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        components.experiments.initializeTooling(applicationContext, intent)
        installSplashScreen()

        updateSecureWindowFlags()

        super.onCreate(savedInstanceState)
        _binding = ActivityMainBinding.inflate(layoutInflater)
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
            Configuration.UI_MODE_NIGHT_NO,
            -> {
                updateLightSystemBars()
            }
            Configuration.UI_MODE_NIGHT_YES -> {
                clearLightSystemBars()
            }
        }
        setContentView(binding.root)

        startupPathProvider.attachOnActivityOnCreate(lifecycle, intent)
        startupTypeTelemetry = StartupTypeTelemetry(components.startupStateProvider, startupPathProvider).apply {
            attachOnMainActivityOnCreate(lifecycle)
        }

        val safeIntent = SafeIntent(intent)

        lifecycle.addObserver(navigator)

        if (savedInstanceState == null) {
            handleAppNavigation(safeIntent)
        }

        if (savedInstanceState == null && intent.hasExtra(HomeScreen.ADD_TO_HOMESCREEN_TAG)) {
            intentProcessor.handleNewIntent(this, safeIntent)
        }

        if (safeIntent.isLauncherIntent) {
            AppOpened.fromIcons.record(AppOpened.FromIconsExtra(AppOpenType.LAUNCH.type))
        }

        val launchCount = settings.getAppLaunchCount()
        PreferenceManager.getDefaultSharedPreferences(this)
            .edit()
            .putInt(getString(R.string.app_launch_count), launchCount + 1)
            .apply()

        AppReviewUtils.showAppReview(this)

        privateNotificationFeature = PrivateNotificationFeature(
            context = applicationContext,
            browserStore = components.store,
            permissionRequestHandler = { requestNotificationPermission() },
        ).also {
            it.start()
        }

        components.notificationsDelegate.bindToActivity(this)
    }

    private fun requestNotificationPermission() {
        privateNotificationFeature.stop()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            notificationPermission.launch(POST_NOTIFICATIONS)
        }
    }

    private fun setSplashScreenPreDrawListener(safeIntent: SafeIntent) {
        val endTime = System.currentTimeMillis() + REQUEST_TIME_OUT
        binding.container.viewTreeObserver.addOnPreDrawListener(
            object : ViewTreeObserver.OnPreDrawListener {
                override fun onPreDraw(): Boolean {
                    return if (System.currentTimeMillis() >= endTime) {
                        ExternalIntentNavigation.handleAppNavigation(
                            bundle = safeIntent.extras,
                            context = this@MainActivity,
                        )
                        binding.container.viewTreeObserver.removeOnPreDrawListener(this)
                        true
                    } else {
                        false
                    }
                }
            },
        )
    }

    private fun checkAndExitPiP() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && isInPictureInPictureMode && intent != null) {
            // Exit PiP mode
            moveTaskToBack(false)
            startActivity(Intent(this, this::class.java).setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT))
        }
    }

    final override fun onUserLeaveHint() {
        // The notification permission prompt will trigger onUserLeaveHint too.
        // We shouldn't treat this situation as user leaving.
        if (!components.notificationsDelegate.isRequestingPermission) {
            val browserFragment =
                supportFragmentManager.findFragmentByTag(BrowserFragment.FRAGMENT_TAG) as BrowserFragment?
            if (browserFragment is UserInteractionHandler && browserFragment.onHomePressed()) {
                return
            }
        }
        super.onUserLeaveHint()
    }

    override fun onResume() {
        super.onResume()
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
    }

    override fun onStop() {
        super.onStop()
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

        handleAppRestoreFromBackground(intent)

        if (intent.dataString.equals(SupportUtils.OPEN_WITH_DEFAULT_BROWSER_URL)) {
            components.appStore.dispatch(
                AppAction.OpenSettings(
                    page = Screen.Settings.Page.General,
                ),
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
        }

        if (ACTION_ERASE == action) {
            processEraseAction(intent)
        }

        if (intent.isLauncherIntent) {
            AppOpened.fromIcons.record(AppOpened.FromIconsExtra(AppOpenType.RESUME.type))
        }

        super.onNewIntent(unsafeIntent)
    }

    private fun handleAppRestoreFromBackground(intent: SafeIntent) {
        if (!intent.extras?.getString(BaseVoiceSearchActivity.SPEECH_PROCESSING).isNullOrEmpty()) {
            handleAppNavigation(intent)
            return
        }
        when (components.appStore.state.screen) {
            is Screen.Settings -> components.appStore.dispatch(
                AppAction.OpenSettings(
                    page =
                    (components.appStore.state.screen as Screen.Settings).page,
                ),
            )
            is Screen.SitePermissionOptionsScreen -> components.appStore.dispatch(
                AppAction.OpenSitePermissionOptionsScreen(
                    sitePermission =
                    (components.appStore.state.screen as Screen.SitePermissionOptionsScreen).sitePermission,
                ),
            )
            else -> {
                handleAppNavigation(intent)
            }
        }
    }

    private fun handleAppNavigation(intent: SafeIntent) {
        if (components.appStore.state.screen == Screen.Locked()) {
            components.appStore.dispatch(AppAction.Lock(intent.extras))
        } else if (settings.getAppLaunchCount() == 0) {
            setSplashScreenPreDrawListener(intent)
        } else {
            ExternalIntentNavigation.handleAppNavigation(
                bundle = intent.extras,
                context = this,
            )
        }
    }

    private fun processEraseAction(intent: SafeIntent) {
        val fromNotificationAction = intent.getBooleanExtra(EXTRA_NOTIFICATION, false)

        components.tabsUseCases.removeAllTabs()

        if (fromNotificationAction) {
            Notifications.eraseOpenButtonTapped.record(Notifications.EraseOpenButtonTappedExtra(tabCount))
        }
    }

    override fun onCreateView(parent: View?, name: String, context: Context, attrs: AttributeSet): View? {
        return if (name == EngineView::class.java.name) {
            components.engine.createView(context, attrs).asView()
        } else {
            super.onCreateView(parent, name, context, attrs)
        }
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

        onBackPressedDispatcher.onBackPressed()
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
                    false,
                ).apply()
        }
    }

    private fun updateLightSystemBars() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            window.statusBarColor = getColorFromAttr(android.R.attr.statusBarColor)
            window.createWindowInsetsController().isAppearanceLightStatusBars = true
        } else {
            window.statusBarColor = Color.BLACK
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // API level can display handle light navigation bar color
            window.createWindowInsetsController().isAppearanceLightNavigationBars = true
            window.navigationBarColor = ContextCompat.getColor(this, android.R.color.transparent)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                window.navigationBarDividerColor =
                    ContextCompat.getColor(this, android.R.color.transparent)
            }
        }
    }

    private fun clearLightSystemBars() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            window.createWindowInsetsController().isAppearanceLightStatusBars = false
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // API level can display handle light navigation bar color
            window.createWindowInsetsController().isAppearanceLightNavigationBars = false
        }
    }

    fun getToolbar(): ActionBar {
        if (!isToolbarInflated) {
            val toolbar = binding.toolbar.inflate() as Toolbar
            setSupportActionBar(toolbar)
            setNavigationIcon(R.drawable.ic_back_button)
            isToolbarInflated = true
        }
        return supportActionBar!!
    }

    fun customizeStatusBar(backgroundColorId: Int? = null) {
        with(binding.statusBarBackground) {
            binding.statusBarBackground.isVisible = true
            StatusBarUtils.getStatusBarHeight(this) { statusBarHeight ->
                layoutParams.height = statusBarHeight
                backgroundColorId?.let { color ->
                    setBackgroundColor(ContextCompat.getColor(context, color))
                }
            }
        }
    }

    fun hideStatusBarBackground() {
        binding.statusBarBackground.isVisible = false
    }

    override fun onDestroy() {
        super.onDestroy()
        _binding = null

        if (this::privateNotificationFeature.isInitialized) {
            privateNotificationFeature.stop()
        }

        components.notificationsDelegate.unBindActivity(this)
    }

    enum class AppOpenType(val type: String) {
        LAUNCH("Launch"),
        RESUME("Resume"),
    }

    companion object {
        const val ACTION_ERASE = "erase"
        const val ACTION_OPEN = "open"

        const val EXTRA_NOTIFICATION = "notification"
    }
}
