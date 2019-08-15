/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProviders
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.preference.PreferenceManager
import android.util.AttributeSet
import android.view.View
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.lib.crash.Crash
import mozilla.components.support.utils.SafeIntent
import org.mozilla.focus.R
import org.mozilla.focus.biometrics.Biometrics
import org.mozilla.focus.ext.components
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.fragment.FirstrunFragment
import org.mozilla.focus.fragment.UrlInputFragment
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity
import org.mozilla.focus.session.IntentProcessor
import org.mozilla.focus.session.removeAndCloseAllSessions
import org.mozilla.focus.session.ui.SessionsSheetFragment
import org.mozilla.focus.settings.ExperimentsSettingsFragment
import org.mozilla.focus.shortcut.HomeScreen
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.ExperimentsSyncService
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.utils.ViewUtils
import org.mozilla.focus.viewmodel.MainViewModel
import org.mozilla.focus.web.IWebView
import org.mozilla.focus.web.WebViewProvider

@Suppress("TooManyFunctions")
open class MainActivity : LocaleAwareAppCompatActivity() {
    protected open val isCustomTabMode: Boolean
        get() = false

    protected open val currentSessionForActivity: Session
        get() = components.sessionManager.selectedSessionOrThrow

    private val intentProcessor by lazy { IntentProcessor(this, components.sessionManager) }

    private var previousSessionCount = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        if (!isTaskRoot) {
            if (intent.hasCategory(Intent.CATEGORY_LAUNCHER) && Intent.ACTION_MAIN == intent.action) {
                finish()
                return
            }
        }

        initViewModel()

        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN

        setContentView(R.layout.activity_main)

        val intent = SafeIntent(intent)

        if (intent.hasExtra(HomeScreen.ADD_TO_HOMESCREEN_TAG)) {
            intentProcessor.handleNewIntent(this, intent)
        }

        if (intent.isLauncherIntent) {
            TelemetryWrapper.openFromIconEvent()
        }

        registerSessionObserver()

        WebViewProvider.preload(this)

        val launchCount = Settings.getInstance(this).getAppLaunchCount()
        PreferenceManager.getDefaultSharedPreferences(this)
                .edit()
                .putInt(getString(R.string.app_launch_count), launchCount + 1)
                .apply()
    }

    private fun initViewModel() {
        val viewModel = ViewModelProviders.of(this).get(MainViewModel::class.java)
        viewModel.getExperimentsLiveData().observe(this, Observer { aBoolean ->
            if (aBoolean!!) {
                val preferenceFragment = ExperimentsSettingsFragment()
                supportFragmentManager
                        .beginTransaction()
                        .replace(R.id.container, preferenceFragment, ExperimentsSettingsFragment.FRAGMENT_TAG)
                        .addToBackStack(null)
                        .commitAllowingStateLoss()
            }
        })
    }

    private fun registerSessionObserver() {
        components.sessionManager.register(object : SessionManager.Observer {
            override fun onSessionSelected(session: Session) {
                showBrowserScreenForCurrentSession()
            }

            override fun onAllSessionsRemoved() {
                showUrlInputScreen()

                WebViewProvider.performNewBrowserSessionCleanup()
            }

            override fun onSessionRemoved(session: Session) {
                previousSessionCount = components.sessionManager.sessions.count()
                if (!isCustomTabMode && components.sessionManager.sessions.isEmpty()) {
                    showUrlInputScreen()

                    WebViewProvider.performNewBrowserSessionCleanup()
                }
            }
        }, owner = this)

        if (!isCustomTabMode && components.sessionManager.sessions.isEmpty()) {
            showUrlInputScreen()

            WebViewProvider.performNewBrowserSessionCleanup()
        } else {
            showBrowserScreenForCurrentSession()
        }

        // If needed show the first run tour on top of the browser or url input fragment.
        if (Settings.getInstance(this@MainActivity).shouldShowFirstrun() && !isCustomTabMode) {
            showFirstrun(components.sessionManager.selectedSession)
        }
    }

    override fun applyLocale() {
        // We don't care here: all our fragments update themselves as appropriate
    }

    override fun onResume() {
        super.onResume()

        TelemetryWrapper.startSession()
        checkBiometricStillValid()
    }

    override fun onPause() {
        if (isFinishing) {
            WebViewProvider.performCleanup(this)
        }

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
        ExperimentsSyncService.scheduleSync(this)
    }

    override fun onNewIntent(unsafeIntent: Intent) {
        if (Crash.isCrashIntent(unsafeIntent)) {
            val browserFragment = supportFragmentManager
                    .findFragmentByTag(BrowserFragment.FRAGMENT_TAG) as BrowserFragment?
            val crash = Crash.fromIntent(unsafeIntent)

            browserFragment?.handleTabCrash(crash)
        }

        val intent = SafeIntent(unsafeIntent)

        if (intent.dataString.equals(SupportUtils.OPEN_WITH_DEFAULT_BROWSER_URL)) {
            openGeneralSettings()
            return
        }

        val action = intent.action

        if (intent.hasExtra(HomeScreen.ADD_TO_HOMESCREEN_TAG)) {
            intentProcessor.handleNewIntent(this, intent)
        }

        if (ACTION_OPEN == action) {
            TelemetryWrapper.openNotificationActionEvent()
        }

        if (ACTION_ERASE == action) {
            processEraseAction(intent)
        }

        if (intent.isLauncherIntent) {
            TelemetryWrapper.resumeFromIconEvent()
        }
    }

    private fun processEraseAction(intent: SafeIntent) {
        val fromShortcut = intent.getBooleanExtra(EXTRA_SHORTCUT, false)
        val fromNotification = intent.getBooleanExtra(EXTRA_NOTIFICATION, false)

        components.sessionManager.removeAndCloseAllSessions()

        if (fromShortcut) {
            TelemetryWrapper.eraseShortcutEvent()
        } else if (fromNotification) {
            TelemetryWrapper.eraseAndOpenNotificationActionEvent()
        }
    }

    private fun showUrlInputScreen() {
        val fragmentManager = supportFragmentManager
        val browserFragment = fragmentManager.findFragmentByTag(BrowserFragment.FRAGMENT_TAG) as BrowserFragment?

        val isShowingBrowser = browserFragment != null
        val crashReporterIsVisible = browserFragment?.crashReporterIsVisible() ?: false

        if (isShowingBrowser && !crashReporterIsVisible) {
            ViewUtils.showBrandedSnackbar(findViewById(android.R.id.content),
                    R.string.feedback_erase,
                    resources.getInteger(R.integer.erase_snackbar_delay))
        }

        // We add the url input fragment to the layout if it doesn't exist yet.
        val transaction = fragmentManager
                .beginTransaction()

        // We only want to play the animation if a browser fragment is added and resumed.
        // If it is not resumed then the application is currently in the process of resuming
        // and the session was removed while the app was in the background (e.g. via the
        // notification). In this case we do not want to show the content and remove the
        // browser fragment immediately.
        val shouldAnimate = isShowingBrowser && browserFragment!!.isResumed

        if (shouldAnimate) {
            if (AppConstants.isGeckoBuild) {
                transaction.setCustomAnimations(0, R.anim.erase_animation_gv)
            } else {
                transaction.setCustomAnimations(0, R.anim.erase_animation)
            }
        }

        // Currently this callback can get invoked while the app is in the background. Therefore we are using
        // commitAllowingStateLoss() here because we can't do a fragment transaction while the app is in the
        // background - like we already do in showBrowserScreenForCurrentSession().
        // Ideally we'd make it possible to pause observers while the app is in the background:
        // https://github.com/mozilla-mobile/android-components/issues/876
        transaction
                .replace(R.id.container, UrlInputFragment.createWithoutSession(), UrlInputFragment.FRAGMENT_TAG)
                .commitAllowingStateLoss()
    }

    private fun showFirstrun(currentSession: Session? = null) {
        supportFragmentManager
                .beginTransaction()
                .add(R.id.container, FirstrunFragment.create(currentSession), FirstrunFragment.FRAGMENT_TAG)
                .commit()
    }

    protected fun showBrowserScreenForCurrentSession() {
        val currentSession = currentSessionForActivity
        val fragmentManager = supportFragmentManager

        val fragment = fragmentManager.findFragmentByTag(BrowserFragment.FRAGMENT_TAG) as BrowserFragment?
        if (fragment != null && fragment.session == currentSession) {
            // There's already a BrowserFragment displaying this session.
            return
        }

        val browserFragment = BrowserFragment.createForSession(currentSession)
        val isNewSession = previousSessionCount < components.sessionManager.sessions.count() && previousSessionCount > 0

        if ((currentSession.source == Session.Source.ACTION_SEND ||
                currentSession.source == Session.Source.HOME_SCREEN) && isNewSession) {
            browserFragment.openedFromExternalLink = true
        }

        fragmentManager
                .beginTransaction()
                .replace(R.id.container, browserFragment, BrowserFragment.FRAGMENT_TAG)
            .commitAllowingStateLoss()

        previousSessionCount = components.sessionManager.sessions.count()
    }

    override fun onCreateView(name: String, context: Context, attrs: AttributeSet): View? {
        return if (name == IWebView::class.java.name) {
            // Inject our implementation of IWebView from the WebViewProvider.
            WebViewProvider.create(this, attrs)
        } else super.onCreateView(name, context, attrs)
    }

    override fun onBackPressed() {
        val fragmentManager = supportFragmentManager

        val sessionsSheetFragment = fragmentManager.findFragmentByTag(
            SessionsSheetFragment.FRAGMENT_TAG) as SessionsSheetFragment?
        if (sessionsSheetFragment != null &&
                sessionsSheetFragment.isVisible &&
                sessionsSheetFragment.onBackPressed()) {
            // SessionsSheetFragment handles back presses itself (custom animations).
            return
        }

        val urlInputFragment = fragmentManager.findFragmentByTag(UrlInputFragment.FRAGMENT_TAG) as UrlInputFragment?
        if (urlInputFragment != null &&
                urlInputFragment.isVisible &&
                urlInputFragment.onBackPressed()) {
            // The URL input fragment has handled the back press. It does its own animations so
            // we do not try to remove it from outside.
            return
        }

        val browserFragment = fragmentManager.findFragmentByTag(BrowserFragment.FRAGMENT_TAG) as BrowserFragment?
        if (browserFragment != null &&
                browserFragment.isVisible &&
                browserFragment.onBackPressed()) {
            // The Browser fragment handles back presses on its own because it might just go back
            // in the browsing history.
            return
        }

        super.onBackPressed()
    }

    // Handles the edge case of a user removing all enrolled prints while auth was enabled
    private fun checkBiometricStillValid() {
        // Disable biometrics if the user is no longer eligible due to un-enrolling fingerprints:
        if (!Biometrics.hasFingerprintHardware(this)) {
            PreferenceManager.getDefaultSharedPreferences(this)
                    .edit().putBoolean(getString(R.string.pref_key_biometric),
                            false).apply()
        }
    }

    companion object {
        const val ACTION_ERASE = "erase"
        const val ACTION_OPEN = "open"

        const val EXTRA_NOTIFICATION = "notification"

        private const val EXTRA_SHORTCUT = "shortcut"

        const val EXPERIMENTS_JOB_ID: Int = 4141
    }
}
