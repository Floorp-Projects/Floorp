/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import GeckoProvider
import android.content.Context
import android.content.res.Configuration
import io.sentry.Sentry
import kotlinx.coroutines.CompletableDeferred
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
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.CookiePolicy
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory
import mozilla.components.concept.engine.mediaquery.PreferredColorScheme
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.media.MediaFeature
import mozilla.components.feature.media.RecordingDevicesNotificationFeature
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.feature.session.HistoryDelegate
import mozilla.components.feature.webcompat.WebCompatFeature
import mozilla.components.feature.webnotifications.WebNotificationFeature
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.lib.dataprotect.generateEncryptionKey
import mozilla.components.service.sync.logins.AsyncLoginsStorageAdapter
import mozilla.components.service.sync.logins.SyncableLoginsStore
import org.mozilla.fenix.AppRequestInterceptor
import org.mozilla.fenix.Config
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.test.Mockable
import java.io.File
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
            remoteDebuggingEnabled = context.settings().isRemoteDebuggingEnabled,
            testingModeEnabled = false,
            trackingProtectionPolicy = createTrackingProtectionPolicy(),
            historyTrackingDelegate = HistoryDelegate(historyStorage),
            preferredColorScheme = getPreferredColorScheme(),
            automaticFontSizeAdjustment = context.settings().shouldUseAutoSize,
            fontInflationEnabled = context.settings().shouldUseAutoSize,
            suspendMediaWhenInactive = !FeatureFlags.mediaIntegration,
            forceUserScalableContent = context.settings().forceEnableZoom
        )

        GeckoEngine(
            context,
            defaultSettings,
            GeckoProvider.getOrCreateRuntime(
                context, asyncPasswordsStorage, getSecureAbove22Preferences()
            )
        ).also {
            WebCompatFeature.install(it)
        }
    }

    /**
     * [Client] implementation to be used for code depending on `concept-fetch``
     */
    val client: Client by lazy {
        GeckoViewFetchClient(
            context,
            GeckoProvider.getOrCreateRuntime(
                context,
                asyncPasswordsStorage,
                getSecureAbove22Preferences()
            )
        )
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
     * The [CustomTabsServiceStore] holds global custom tabs related data.
     */
    val customTabsStore by lazy { CustomTabsServiceStore() }

    /**
     * The session manager component provides access to a centralized registry of
     * all browser sessions (i.e. tabs). It is initialized here to persist and restore
     * sessions from the [SessionStorage], and with a default session (about:blank) in
     * case all sessions/tabs are closed.
     */
    val sessionManager by lazy {
        SessionManager(engine, store).also { sessionManager ->
            // Install the "icons" WebExtension to automatically load icons for every visited website.
            icons.install(engine, store)

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

            WebNotificationFeature(
                context, engine, icons, R.drawable.ic_status_logo,
                HomeActivity::class.java
            )
        }
    }

    /**
     * Icons component for loading, caching and processing website icons.
     */
    val icons by lazy {
        BrowserIcons(context, client)
    }

    /**
     * Shortcut component for managing shortcuts on the device home screen.
     */
    val webAppShortcutManager by lazy {
        WebAppShortcutManager(
            context,
            client,
            webAppManifestStorage,
            supportWebApps = FeatureFlags.progressiveWebApps
        )
    }

    /**
     * The storage component to persist browsing history (with the exception of
     * private sessions).
     */
    val historyStorage by lazy { PlacesHistoryStorage(context) }

    val bookmarksStorage by lazy { PlacesBookmarksStorage(context) }

    val tabCollectionStorage by lazy { TabCollectionStorage(context, sessionManager) }

    val topSiteStorage by lazy { TopSiteStorage(context) }

    val permissionStorage by lazy { PermissionStorage(context) }

    val webAppManifestStorage by lazy { ManifestStorage(context) }

    val asyncPasswordsStorage by lazy {
        AsyncLoginsStorageAdapter.forDatabase(
            File(
                context.filesDir,
                "logins.sqlite"
            ).canonicalPath
        )
    }

    val syncablePasswordsStorage by lazy {
        SyncableLoginsStore(
            asyncPasswordsStorage
        ) {
            CompletableDeferred(passwordsEncryptionKey)
        }
    }

    /**
     * Shared Preferences that encrypt/decrypt using Android KeyStore and lib-dataprotect for 23+
     * only on Nightly/Debug for now, otherwise simply stored.
     * See https://github.com/mozilla-mobile/fenix/issues/8324
     */
    fun getSecureAbove22Preferences() =
        SecureAbove22Preferences(
            context = context,
            name = KEY_STORAGE_NAME,
            forceInsecure = !Config.channel.isNightlyOrDebug
        )

    val passwordsEncryptionKey: String =
        getSecureAbove22Preferences().getString(PASSWORDS_KEY)
            ?: generateEncryptionKey(KEY_STRENGTH).also {
                if (context.settings().passwordsEncryptionKeyGenerated) {
                    // We already had previously generated an encryption key, but we have lost it
                    Sentry.capture("Passwords encryption key for passwords storage was lost and we generated a new one")
                }
                context.settings().recordPasswordsEncryptionKeyGenerated()
                getSecureAbove22Preferences().putString(PASSWORDS_KEY, it)
            }

    /**
     * Constructs a [TrackingProtectionPolicy] based on current preferences.
     *
     * @param normalMode whether or not tracking protection should be enabled
     * in normal browsing mode, defaults to the current preference value.
     * @param privateMode whether or not tracking protection should be enabled
     * in private browsing mode, default to the current preference value.
     * @return the constructed tracking protection policy based on preferences.
     */
    @Suppress("ComplexMethod")
    fun createTrackingProtectionPolicy(
        normalMode: Boolean = context.settings().shouldUseTrackingProtection,
        privateMode: Boolean = true
    ): TrackingProtectionPolicy {
        val trackingProtectionPolicy =
            when {
                context.settings().useStrictTrackingProtection -> TrackingProtectionPolicy.strict()
                context.settings().useCustomTrackingProtection -> return TrackingProtectionPolicy.select(
                    cookiePolicy = geCustomCookiePolicy(),
                    trackingCategories = getCustomTrackingCategories()
                ).apply {
                    if (context.settings().blockTrackingContentSelectionInCustomTrackingProtection == "private") {
                        forPrivateSessionsOnly()
                    }
                }
                else -> TrackingProtectionPolicy.recommended()
            }

        return when {
            normalMode && privateMode -> trackingProtectionPolicy
            normalMode && !privateMode -> trackingProtectionPolicy.forRegularSessionsOnly()
            !normalMode && privateMode -> trackingProtectionPolicy.forPrivateSessionsOnly()
            else -> TrackingProtectionPolicy.none()
        }
    }

    private fun geCustomCookiePolicy(): CookiePolicy {
            return when (context.settings().blockCookiesSelectionInCustomTrackingProtection) {
                "all" -> CookiePolicy.ACCEPT_NONE
                "social" -> CookiePolicy.ACCEPT_NON_TRACKERS
                "unvisited" -> CookiePolicy.ACCEPT_VISITED
                "third-party" -> CookiePolicy.ACCEPT_ONLY_FIRST_PARTY
                else -> CookiePolicy.ACCEPT_NONE
            }
    }

    private fun getCustomTrackingCategories(): Array<TrackingCategory> {
        val categories = arrayListOf(
            TrackingCategory.AD,
            TrackingCategory.ANALYTICS,
            TrackingCategory.SOCIAL,
            TrackingCategory.MOZILLA_SOCIAL
        )

        if (context.settings().blockTrackingContentInCustomTrackingProtection) {
            categories.add(TrackingCategory.SCRIPTS_AND_SUB_RESOURCES)
        }

        if (context.settings().blockFingerprintersInCustomTrackingProtection) {
            categories.add(TrackingCategory.FINGERPRINTING)
        }

        if (context.settings().blockCryptominersInCustomTrackingProtection) {
            categories.add(TrackingCategory.CRYPTOMINING)
        }

        return categories.toTypedArray()
    }

    /**
     * Sets Preferred Color scheme based on Dark/Light Theme Settings or Current Configuration
     */
    fun getPreferredColorScheme(): PreferredColorScheme {
        val inDark =
            (context.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) ==
                    Configuration.UI_MODE_NIGHT_YES
        return when {
            context.settings().shouldUseDarkTheme -> PreferredColorScheme.Dark
            context.settings().shouldUseLightTheme -> PreferredColorScheme.Light
            inDark -> PreferredColorScheme.Dark
            else -> PreferredColorScheme.Light
        }
    }

    companion object {
        private const val KEY_STRENGTH = 256
        private const val KEY_STORAGE_NAME = "core_prefs"
        private const val PASSWORDS_KEY = "passwords"
    }
}
