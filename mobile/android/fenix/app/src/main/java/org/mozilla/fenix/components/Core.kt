/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import GeckoProvider
import android.content.Context
import android.content.res.Configuration
import io.sentry.Sentry
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
import mozilla.components.browser.storage.sync.RemoteTabsStorage
import mozilla.components.browser.thumbnails.ThumbnailsMiddleware
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.mediaquery.PreferredColorScheme
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.downloads.DownloadMiddleware
import mozilla.components.feature.logins.exceptions.LoginExceptionStorage
import mozilla.components.feature.media.RecordingDevicesNotificationFeature
import mozilla.components.feature.media.middleware.MediaMiddleware
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.feature.readerview.ReaderViewMiddleware
import mozilla.components.feature.session.HistoryDelegate
import mozilla.components.feature.webcompat.WebCompatFeature
import mozilla.components.feature.webcompat.reporter.WebCompatReporterFeature
import mozilla.components.feature.webnotifications.WebNotificationFeature
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.lib.dataprotect.generateEncryptionKey
import mozilla.components.service.digitalassetlinks.RelationChecker
import mozilla.components.service.digitalassetlinks.local.StatementApi
import mozilla.components.service.digitalassetlinks.local.StatementRelationChecker
import mozilla.components.service.sync.logins.SyncableLoginsStorage
import org.mozilla.fenix.AppRequestInterceptor
import org.mozilla.fenix.Config
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.downloads.DownloadService
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.media.MediaService
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry
import org.mozilla.fenix.search.telemetry.incontent.InContentTelemetry
import org.mozilla.fenix.utils.Mockable
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
            trackingProtectionPolicy = trackingProtectionPolicyFactory.createTrackingProtectionPolicy(),
            historyTrackingDelegate = HistoryDelegate(lazyHistoryStorage),
            preferredColorScheme = getPreferredColorScheme(),
            automaticFontSizeAdjustment = context.settings().shouldUseAutoSize,
            fontInflationEnabled = context.settings().shouldUseAutoSize,
            suspendMediaWhenInactive = false,
            forceUserScalableContent = context.settings().forceEnableZoom,
            loginAutofillEnabled = context.settings().shouldAutofillLogins
        )

        GeckoEngine(
            context,
            defaultSettings,
            GeckoProvider.getOrCreateRuntime(
                context,
                lazyPasswordsStorage,
                trackingProtectionPolicyFactory.createTrackingProtectionPolicy()
            )
        ).also {
            WebCompatFeature.install(it)

            /**
             * There are some issues around localization to be resolved, as well as questions around
             * the capacity of the WebCompat team, so the "Report site issue" feature should stay
             * disabled in Fenix Release builds for now.
             * This is consistent with both Fennec and Firefox Desktop.
             */
            if (Config.channel.isNightlyOrDebug || Config.channel.isBeta) {
                WebCompatReporterFeature.install(it)
            }
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
                lazyPasswordsStorage,
                trackingProtectionPolicyFactory.createTrackingProtectionPolicy()
            )
        )
    }

    private val sessionStorage: SessionStorage by lazy {
        SessionStorage(context, engine = engine)
    }

    /**
     * The [BrowserStore] holds the global [BrowserState].
     */
    val store by lazy {
        BrowserStore(
            middleware = listOf(
                MediaMiddleware(context, MediaService::class.java),
                DownloadMiddleware(context, DownloadService::class.java),
                ReaderViewMiddleware(),
                ThumbnailsMiddleware(thumbnailStorage)
            )
        )
    }

    /**
     * The [CustomTabsServiceStore] holds global custom tabs related data.
     */
    val customTabsStore by lazy { CustomTabsServiceStore() }

    /**
     * The [RelationChecker] checks Digital Asset Links relationships for Trusted Web Activities.
     */
    val relationChecker: RelationChecker by lazy {
        StatementRelationChecker(StatementApi(client))
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
            icons.install(engine, store)

            // Install the "ads" WebExtension to get the links in an partner page.
            adsTelemetry.install(engine, store)

            // Install the "cookies" WebExtension and tracks user interaction with SERPs.
            searchTelemetry.install(engine, store)

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

            WebNotificationFeature(
                context, engine, icons, R.drawable.ic_status_logo,
                permissionStorage.permissionsStorage, HomeActivity::class.java
            )
        }
    }

    /**
     * Icons component for loading, caching and processing website icons.
     */
    val icons by lazy {
        BrowserIcons(context, client)
    }

    val adsTelemetry by lazy {
        AdsTelemetry(context.components.analytics.metrics)
    }

    val searchTelemetry by lazy {
        InContentTelemetry(context.components.analytics.metrics)
    }

    /**
     * Shortcut component for managing shortcuts on the device home screen.
     */
    val webAppShortcutManager by lazy {
        WebAppShortcutManager(
            context,
            client,
            webAppManifestStorage
        )
    }

    // Lazy wrappers around storage components are used to pass references to these components without
    // initializing them until they're accessed.
    // Use these for startup-path code, where we don't want to do any work that's not strictly necessary.
    // For example, this is how the GeckoEngine delegates (history, logins) are configured.
    // We can fully initialize GeckoEngine without initialized our storage.
    val lazyHistoryStorage = lazy { PlacesHistoryStorage(context) }
    val lazyBookmarksStorage = lazy { PlacesBookmarksStorage(context) }
    val lazyPasswordsStorage = lazy { SyncableLoginsStorage(context, passwordsEncryptionKey) }

    /**
     * The storage component to sync and persist tabs in a Firefox Sync account.
     */
    val lazyRemoteTabsStorage = lazy { RemoteTabsStorage() }

    // For most other application code (non-startup), these wrappers are perfectly fine and more ergonomic.
    val historyStorage by lazy { lazyHistoryStorage.value }
    val bookmarksStorage by lazy { lazyBookmarksStorage.value }
    val passwordsStorage by lazy { lazyPasswordsStorage.value }

    val tabCollectionStorage by lazy { TabCollectionStorage(context, sessionManager) }

    /**
     * A storage component for persisting thumbnail images of tabs.
     */
    val thumbnailStorage by lazy { ThumbnailStorage(context) }

    val topSiteStorage by lazy { TopSiteStorage(context) }

    val permissionStorage by lazy { PermissionStorage(context) }

    val webAppManifestStorage by lazy { ManifestStorage(context) }

    val loginExceptionStorage by lazy { LoginExceptionStorage(context) }

    /**
     * Shared Preferences that encrypt/decrypt using Android KeyStore and lib-dataprotect for 23+
     * only on Nightly/Debug for now, otherwise simply stored.
     * See https://github.com/mozilla-mobile/fenix/issues/8324
     */
    private fun getSecureAbove22Preferences() =
        SecureAbove22Preferences(
            context = context,
            name = KEY_STORAGE_NAME,
            forceInsecure = !Config.channel.isNightlyOrDebug
        )

    private val passwordsEncryptionKey by lazy {
        getSecureAbove22Preferences().getString(PASSWORDS_KEY)
            ?: generateEncryptionKey(KEY_STRENGTH).also {
                if (context.settings().passwordsEncryptionKeyGenerated &&
                    isSentryEnabled()) {
                    // We already had previously generated an encryption key, but we have lost it
                    Sentry.capture("Passwords encryption key for passwords storage was lost and we generated a new one")
                }
                context.settings().recordPasswordsEncryptionKeyGenerated()
                getSecureAbove22Preferences().putString(PASSWORDS_KEY, it)
            }
    }

    val trackingProtectionPolicyFactory = TrackingProtectionPolicyFactory(context.settings())

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
