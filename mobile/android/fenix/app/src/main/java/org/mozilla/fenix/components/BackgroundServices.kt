/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.os.Build
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.browser.storage.sync.RemoteTabsStorage
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.accounts.push.FxaPushSupportFeature
import mozilla.components.feature.accounts.push.SendTabFeature
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.SCOPE_SESSION
import mozilla.components.service.fxa.manager.SCOPE_SYNC
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import mozilla.components.service.fxa.sync.GlobalSyncableStoreProvider
import mozilla.components.service.sync.logins.SyncableLoginsStorage
import mozilla.components.support.utils.RunWhenReadyQueue
import org.mozilla.fenix.Config
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.sync.SyncedTabsIntegration
import org.mozilla.fenix.utils.Mockable
import org.mozilla.fenix.utils.Settings

/**
 * Component group for background services. These are the components that need to be accessed from within a
 * background worker.
 */
@Mockable
@Suppress("LongParameterList")
class BackgroundServices(
    private val context: Context,
    private val push: Push,
    crashReporter: CrashReporter,
    historyStorage: Lazy<PlacesHistoryStorage>,
    bookmarkStorage: Lazy<PlacesBookmarksStorage>,
    passwordsStorage: Lazy<SyncableLoginsStorage>,
    remoteTabsStorage: Lazy<RemoteTabsStorage>
) {
    // Allows executing tasks which depend on the account manager, but do not need to eagerly initialize it.
    val accountManagerAvailableQueue = RunWhenReadyQueue()

    fun defaultDeviceName(context: Context): String =
        context.getString(
            R.string.default_device_name_2,
            context.getString(R.string.app_name),
            Build.MANUFACTURER,
            Build.MODEL
        )

    val serverConfig = FxaServer.config(context)
    private val deviceConfig = DeviceConfig(
        name = defaultDeviceName(context),
        type = DeviceType.MOBILE,

        // NB: flipping this flag back and worth is currently not well supported and may need hand-holding.
        // Consult with the android-components peers before changing.
        // See https://github.com/mozilla/application-services/issues/1308
        capabilities = setOf(DeviceCapability.SEND_TAB),

        // Enable encryption for account state on supported API levels (23+).
        // Just on Nightly and local builds for now.
        // Enabling this for all channels is tracked in https://github.com/mozilla-mobile/fenix/issues/6704
        secureStateAtRest = Config.channel.isNightlyOrDebug
    )

    @VisibleForTesting
    val supportedEngines =
        setOf(SyncEngine.History, SyncEngine.Bookmarks, SyncEngine.Passwords, SyncEngine.Tabs)
    private val syncConfig = SyncConfig(supportedEngines, syncPeriodInMinutes = 240L) // four hours

    init {
        /* Make the "history", "bookmark", "passwords", and "tabs" stores accessible to workers
           spawned by the sync manager. */
        GlobalSyncableStoreProvider.configureStore(SyncEngine.History to historyStorage)
        GlobalSyncableStoreProvider.configureStore(SyncEngine.Bookmarks to bookmarkStorage)
        GlobalSyncableStoreProvider.configureStore(SyncEngine.Passwords to passwordsStorage)
        GlobalSyncableStoreProvider.configureStore(SyncEngine.Tabs to remoteTabsStorage)
    }

    private val telemetryAccountObserver = TelemetryAccountObserver(
        context.settings(),
        context.components.analytics.metrics
    )

    val accountAbnormalities = AccountAbnormalities(context, crashReporter)

    val accountManager by lazy { makeAccountManager(context, serverConfig, deviceConfig, syncConfig) }

    val syncedTabsStorage by lazy {
        SyncedTabsStorage(accountManager, context.components.core.store, remoteTabsStorage.value)
    }

    @VisibleForTesting(otherwise = PRIVATE)
    fun makeAccountManager(
        context: Context,
        serverConfig: ServerConfig,
        deviceConfig: DeviceConfig,
        syncConfig: SyncConfig?
    ) = FxaAccountManager(
        context,
        serverConfig,
        deviceConfig,
        syncConfig,
        setOf(
            // We don't need to specify sync scope explicitly, but `syncConfig` may be disabled due to
            // an 'experiments' flag. In that case, sync scope necessary for syncing won't be acquired
            // during authentication unless we explicitly specify it below.
            // This is a good example of an information leak at the API level.
            // See https://github.com/mozilla-mobile/android-components/issues/3732
            SCOPE_SYNC,
            // Necessary to enable "Manage Account" functionality and ability to generate OAuth
            // codes for certain scopes.
            SCOPE_SESSION
        )
    ).also { accountManager ->
        // TODO this needs to change once we have a SyncManager
        context.settings().fxaHasSyncedItems = accountManager.authenticatedAccount()?.let {
            SyncEnginesStorage(context).getStatus().any { it.value }
        } ?: false

        // Register a telemetry account observer to keep track of FxA auth metrics.
        accountManager.register(telemetryAccountObserver)

        // Register an "abnormal fxa behaviour" middleware to keep track of events such as
        // unexpected logouts.
        accountManager.register(accountAbnormalities)

        // Enable push if it's configured.
        push.feature?.let { autoPushFeature ->
            FxaPushSupportFeature(context, accountManager, autoPushFeature)
        }

        SendTabFeature(accountManager) { device, tabs ->
            notificationManager.showReceivedTabs(context, device, tabs)
        }

        SyncedTabsIntegration(context, accountManager).launch()

        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            accountManager.initAsync()
        )
    }.also {
        accountManagerAvailableQueue.ready()
    }

    /**
     * Provides notification functionality, manages notification channels.
     */
    private val notificationManager by lazy {
        NotificationManager(context)
    }
}

@VisibleForTesting(otherwise = PRIVATE)
internal class TelemetryAccountObserver(
    private val settings: Settings,
    private val metricController: MetricController
) : AccountObserver {
    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
        when (authType) {
            // User signed-in into an existing FxA account.
            AuthType.Signin ->
                metricController.track(Event.SyncAuthSignIn)

            // User created a new FxA account.
            AuthType.Signup ->
                metricController.track(Event.SyncAuthSignUp)

            // User paired to an existing account via QR code scanning.
            AuthType.Pairing ->
                metricController.track(Event.SyncAuthPaired)

            // User signed-in into an FxA account shared from another locally installed app
            // (e.g. Fennec).
            AuthType.Shared ->
                metricController.track(Event.SyncAuthFromShared)

            // Account Manager recovered a broken FxA auth state, without direct user involvement.
            AuthType.Recovered ->
                metricController.track(Event.SyncAuthRecovered)

            // User signed-in into an FxA account via unknown means.
            // Exact mechanism identified by the 'action' param.
            is AuthType.OtherExternal ->
                metricController.track(Event.SyncAuthOtherExternal)
        }
        // Used by Leanplum as a context variable.
        settings.fxaSignedIn = true
    }

    override fun onLoggedOut() {
        metricController.track(Event.SyncAuthSignOut)
        // Used by Leanplum as a context variable.
        settings.fxaSignedIn = false
    }
}
