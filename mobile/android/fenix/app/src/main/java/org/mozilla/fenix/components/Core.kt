/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.content.SharedPreferences
import android.os.Bundle
import android.preference.PreferenceManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SessionStorage
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.session.HistoryDelegate
import mozilla.components.feature.session.bundling.SessionBundleStorage
import mozilla.components.lib.crash.handler.CrashHandlerService
import org.mozilla.fenix.AppRequestInterceptor
import org.mozilla.fenix.utils.Settings
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import java.util.concurrent.TimeUnit

/**
 * Component group for all core browser functionality.
 */
class Core(private val context: Context) {

    private val runtime by lazy {
        val builder = GeckoRuntimeSettings.Builder()

        testConfig?.let {
            builder.extras(it)
        }

        val runtimeSettings = builder
            .crashHandler(CrashHandlerService::class.java)
            .build()

        GeckoRuntime.create(context, runtimeSettings)
    }

    var testConfig: Bundle? = null

    /**
     * The browser engine component initialized based on the build
     * configuration (see build variants).
     */
    val engine: Engine by lazy {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)

        val defaultSettings = DefaultSettings(
            requestInterceptor = AppRequestInterceptor(context),
            remoteDebuggingEnabled = Settings.getInstance(context).isRemoteDebuggingEnabled,
            testingModeEnabled = false,
            trackingProtectionPolicy = createTrackingProtectionPolicy(prefs),
            historyTrackingDelegate = HistoryDelegate(historyStorage)
        )

        GeckoEngine(context, defaultSettings, runtime)
    }

    /**
     * [Client] implementation to be used for code depending on `concept-fetch``
     */
    val client: Client by lazy {
        GeckoViewFetchClient(context, runtime)
    }

    val sessionStorage: SessionBundleStorage by lazy {
        SessionBundleStorage(context, bundleLifetime = Pair(BUNDLE_LIFETIME_IN_MINUTES, TimeUnit.MINUTES),
            engine = engine)
    }

    /**
     * The session manager component provides access to a centralized registry of
     * all browser sessions (i.e. tabs). It is initialized here to persist and restore
     * sessions from the [SessionStorage], and with a default session (about:blank) in
     * case all sessions/tabs are closed.
     */
    val sessionManager by lazy {
        SessionManager(engine).also { sessionManager ->
            // Restore a previous, still active bundle.
            GlobalScope.launch(Dispatchers.Main) {
                val snapshot = async(Dispatchers.IO) {
                    sessionStorage.restore()?.restoreSnapshot()
                }

                // There's an active bundle with a snapshot: Feed it into the SessionManager.
                snapshot.await()?.let {
                    try {
                        sessionManager.restore(it)
                    } catch (_: IllegalArgumentException) {
                        return@let
                    }
                }

                // Now that we have restored our previous state (if there's one) let's setup auto saving the state while
                // the app is used.
                sessionStorage.apply {
                    autoSave(sessionManager)
                        .periodicallyInForeground(interval = 30, unit = TimeUnit.SECONDS)
                        .whenGoingToBackground()
                        .whenSessionsChange()
                    autoClose(sessionManager)
                }
            }
        }
    }

    /**
     * The storage component to persist browsing history (with the exception of
     * private sessions).
     */
    val historyStorage by lazy { PlacesHistoryStorage(context) }

    val bookmarksStorage
            by lazy { PlacesBookmarksStorage(context) }

    /**
     * Constructs a [TrackingProtectionPolicy] based on current preferences.
     *
     * @param prefs the shared preferences to use when reading tracking
     * protection settings.
     * @param normalMode whether or not tracking protection should be enabled
     * in normal browsing mode, defaults to the current preference value.
     * @param privateMode whether or not tracking protection should be enabled
     * in private browsing mode, default to the current preference value.
     * @return the constructed tracking protection policy based on preferences.
     */
    fun createTrackingProtectionPolicy(
        prefs: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(context),
        normalMode: Boolean = true,
        privateMode: Boolean = true
    ): TrackingProtectionPolicy {

        return when {
            normalMode && privateMode -> TrackingProtectionPolicy.all()
            normalMode && !privateMode -> TrackingProtectionPolicy.all().forRegularSessionsOnly()
            !normalMode && privateMode -> TrackingProtectionPolicy.all().forPrivateSessionsOnly()
            else -> TrackingProtectionPolicy.none()
        }
    }

    companion object {
        private const val BUNDLE_LIFETIME_IN_MINUTES = 5L
    }
}
