/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import GeckoProvider
import android.content.Context
import android.content.res.Configuration
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SessionStorage
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.mediaquery.PreferredColorScheme
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.media.MediaFeature
import mozilla.components.feature.media.RecordingDevicesNotificationFeature
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.feature.session.HistoryDelegate
import mozilla.components.feature.webcompat.WebCompatFeature
import org.mozilla.fenix.AppRequestInterceptor
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.test.Mockable
import java.util.concurrent.TimeUnit

/**
 * Component group for all core browser functionality.
 */
@Mockable
class Core(private val context: Context) {
    /**
     * The browser engine component initialized based on the build
     * configuration (see build variants).
     */
    val engine: Engine by lazy {
        val defaultSettings = DefaultSettings(
            requestInterceptor = AppRequestInterceptor(context),
            remoteDebuggingEnabled = context.settings.isRemoteDebuggingEnabled,
            testingModeEnabled = false,
            trackingProtectionPolicy = createTrackingProtectionPolicy(),
            historyTrackingDelegate = HistoryDelegate(historyStorage),
            preferredColorScheme = getPreferredColorScheme(),
            automaticFontSizeAdjustment = context.settings.shouldUseAutoSize,
            fontInflationEnabled = context.settings.shouldUseAutoSize,
            suspendMediaWhenInactive = !FeatureFlags.mediaIntegration,
            allowAutoplayMedia = context.settings.isAutoPlayEnabled
        )

        GeckoEngine(context, defaultSettings, GeckoProvider.getOrCreateRuntime(context)).also {
            WebCompatFeature.install(it)
        }
    }

    /**
     * [Client] implementation to be used for code depending on `concept-fetch``
     */
    val client: Client by lazy {
        GeckoViewFetchClient(context, GeckoProvider.getOrCreateRuntime(context))
    }

    val sessionStorage: SessionStorage by lazy {
        SessionStorage(context, engine = engine)
    }

    /**
     * The [BrowserStore] holds the global [BrowserState].
     */
    val store by lazy {
        BrowserStore()
    }

    /**
     * The session manager component provides access to a centralized registry of
     * all browser sessions (i.e. tabs). It is initialized here to persist and restore
     * sessions from the [SessionStorage], and with a default session (about:blank) in
     * case all sessions/tabs are closed.
     */
    val sessionManager by lazy {
        SessionManager(engine, store).also { sessionManager ->
            // Install the "icons" WebExtension to automatically load icons for every visited website.
            icons.install(engine, sessionManager)

            // Show an ongoing notification when recording devices (camera, microphone) are used by web content
            RecordingDevicesNotificationFeature(context, sessionManager)
                .enable()

            // Restore the previous state.
            GlobalScope.launch(Dispatchers.Main) {
                withContext(Dispatchers.IO) {
                    sessionStorage.restore()
                }?.let { snapshot ->
                    sessionManager.restore(
                        snapshot,
                        updateSelection = (sessionManager.selectedSession == null)
                    )
                }

                // Now that we have restored our previous state (if there's one) let's setup auto saving the state while
                // the app is used.
                sessionStorage.autoSave(sessionManager)
                    .periodicallyInForeground(interval = 30, unit = TimeUnit.SECONDS)
                    .whenGoingToBackground()
                    .whenSessionsChange()
            }

            if (FeatureFlags.mediaIntegration) {
                MediaStateMachine.start(sessionManager)

                // Enable media features like showing an ongoing notification with media controls when
                // media in web content is playing.
                MediaFeature(context).enable()
            }
        }
    }

    /**
     * Icons component for loading, caching and processing website icons.
     */
    val icons by lazy {
        BrowserIcons(context, context.components.core.client)
    }

    /**
     * The storage component to persist browsing history (with the exception of
     * private sessions).
     */
    val historyStorage by lazy { PlacesHistoryStorage(context) }

    val bookmarksStorage by lazy { PlacesBookmarksStorage(context) }

    val tabCollectionStorage by lazy { TabCollectionStorage(context, sessionManager) }

    val permissionStorage by lazy { PermissionStorage(context) }

    /**
     * Constructs a [TrackingProtectionPolicy] based on current preferences.
     *
     * @param normalMode whether or not tracking protection should be enabled
     * in normal browsing mode, defaults to the current preference value.
     * @param privateMode whether or not tracking protection should be enabled
     * in private browsing mode, default to the current preference value.
     * @return the constructed tracking protection policy based on preferences.
     */
    fun createTrackingProtectionPolicy(
        normalMode: Boolean = context.settings.shouldUseTrackingProtection,
        privateMode: Boolean = true
    ): TrackingProtectionPolicy {
        val trackingProtectionPolicy =
            if (context.settings.useStrictTrackingProtection) TrackingProtectionPolicy.strict() else
                TrackingProtectionPolicy.recommended()

        return when {
            normalMode && privateMode -> trackingProtectionPolicy
            normalMode && !privateMode -> trackingProtectionPolicy.forRegularSessionsOnly()
            !normalMode && privateMode -> trackingProtectionPolicy.forPrivateSessionsOnly()
            else -> TrackingProtectionPolicy.none()
        }
    }

    /**
     * Sets Preferred Color scheme based on Dark/Light Theme Settings or Current Configuration
     */
    fun getPreferredColorScheme(): PreferredColorScheme {
        val inDark =
            (context.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) ==
                    Configuration.UI_MODE_NIGHT_YES
        return when {
            context.settings.shouldUseDarkTheme -> PreferredColorScheme.Dark
            context.settings.shouldUseLightTheme -> PreferredColorScheme.Light
            inDark -> PreferredColorScheme.Dark
            else -> PreferredColorScheme.Light
        }
    }
}
