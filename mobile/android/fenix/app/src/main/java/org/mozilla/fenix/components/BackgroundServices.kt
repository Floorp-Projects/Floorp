/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.os.Build
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.push.Bus
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.DevicePushSubscription
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import mozilla.components.feature.push.PushConfig
import mozilla.components.feature.push.PushSubscriptionObserver
import mozilla.components.feature.push.PushType
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.GlobalSyncableStoreProvider
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.Experiments
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.isInExperiment
import org.mozilla.fenix.test.Mockable
import java.util.FormatFlagsConversionMismatchException

/**
 * Component group for background services. These are the components that need to be accessed from within a
 * background worker.
 */
@Mockable
class BackgroundServices(
    private val context: Context,
    historyStorage: PlacesHistoryStorage,
    bookmarkStorage: PlacesBookmarksStorage
) {
    companion object {
        const val CLIENT_ID = "a2270f727f45f648"

        fun redirectUrl(context: Context) = if (context.isInExperiment(Experiments.asFeatureWebChannelsDisabled)) {
            "https://accounts.firefox.com/oauth/success/$CLIENT_ID"
        } else {
            "urn:ietf:wg:oauth:2.0:oob:oauth-redirect-webchannel"
        }
    }

    // // A malformed string is causing crashes.
    // This will be removed when the string is fixed. See #5552
    fun defaultDeviceName(context: Context): String = try {
            context.getString(
                R.string.default_device_name,
                context.getString(R.string.app_name),
                Build.MANUFACTURER,
                Build.MODEL
            )
        } catch (ex: FormatFlagsConversionMismatchException) {
            "%s on %s %s".format(
                context.getString(R.string.app_name),
                Build.MANUFACTURER,
                Build.MODEL
            )
        }

    private val serverConfig = ServerConfig.release(CLIENT_ID, redirectUrl(context))
    private val deviceConfig = DeviceConfig(
        name = defaultDeviceName(context),
        type = DeviceType.MOBILE,

        // NB: flipping this flag back and worth is currently not well supported and may need hand-holding.
        // Consult with the android-components peers before changing.
        // See https://github.com/mozilla/application-services/issues/1308
        capabilities = setOf(DeviceCapability.SEND_TAB)
    )
    // If sync has been turned off on the server then disable syncing.
    @VisibleForTesting(otherwise = PRIVATE)
    val syncConfig = if (context.isInExperiment(Experiments.asFeatureSyncDisabled)) {
        null
    } else {
        SyncConfig(setOf(SyncEngine.History, SyncEngine.Bookmarks), syncPeriodInMinutes = 240L) // four hours
    }

    private val pushService by lazy { FirebasePush() }

    val push by lazy { makePushConfig()?.let { makePush(it) } }

    init {
        // Make the "history" and "bookmark" stores accessible to workers spawned by the sync manager.
        GlobalSyncableStoreProvider.configureStore(SyncEngine.History to historyStorage)
        GlobalSyncableStoreProvider.configureStore(SyncEngine.Bookmarks to bookmarkStorage)
    }

    private val deviceEventObserver = object : DeviceEventsObserver {
        private val logger = Logger("DeviceEventsObserver")
        override fun onEvents(events: List<DeviceEvent>) {
            logger.info("Received ${events.size} device event(s)")
            events.filterIsInstance<DeviceEvent.TabReceived>().forEach {
                notificationManager.showReceivedTabs(it)
            }
        }
    }

    private val telemetryAccountObserver = TelemetryAccountObserver(
        context,
        context.components.analytics.metrics
    )

    private val pushAccountObserver by lazy { push?.let { PushAccountObserver(it) } }

    val accountManager = makeAccountManager(context, serverConfig, deviceConfig, syncConfig)

    @VisibleForTesting(otherwise = PRIVATE)
    fun makePush(pushConfig: PushConfig): AutoPushFeature {
        return AutoPushFeature(
            context = context,
            service = pushService,
            config = pushConfig
        )
    }

    @VisibleForTesting(otherwise = PRIVATE)
    fun makePushConfig(): PushConfig? {
        val logger = Logger("PushConfig")
        val projectIdKey = context.getString(R.string.pref_key_push_project_id)
        val resId = context.resources.getIdentifier(projectIdKey, "string", context.packageName)
        if (resId == 0) {
            logger.warn("No firebase configuration found; cannot support push service.")
            return null
        }

        logger.debug("Creating push configuration for autopush.")
        val projectId = context.resources.getString(resId)
        return PushConfig(projectId)
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
        // We don't need to specify this explicitly, but `syncConfig` may be disabled due to an 'experiments'
        // flag. In that case, sync scope necessary for syncing won't be acquired during authentication
        // unless we explicitly specify it below.
        // This is a good example of an information leak at the API level.
        // See https://github.com/mozilla-mobile/android-components/issues/3732
        setOf("https://identity.mozilla.com/apps/oldsync")
    ).also { accountManager ->
        // TODO this needs to change once we have a SyncManager
        context.settings().fxaHasSyncedItems = syncConfig?.supportedEngines?.isNotEmpty() ?: false
        accountManager.registerForDeviceEvents(deviceEventObserver, ProcessLifecycleOwner.get(), false)

        // Register a telemetry account observer to keep track of FxA auth metrics.
        accountManager.register(telemetryAccountObserver)

        // Enable push if it's configured.
        push?.let { autoPushFeature ->
            // Register the push account observer so we know how to update our push subscriptions.
            accountManager.register(pushAccountObserver!!)

            val logger = Logger("AutoPushFeature")

            // Notify observers for Services' messages.
            autoPushFeature.registerForPushMessages(
                PushType.Services,
                object : Bus.Observer<PushType, String> {
                    override fun onEvent(type: PushType, message: String) {
                        accountManager.authenticatedAccount()?.deviceConstellation()
                            ?.processRawEventAsync(message)
                    }
                })

            // Notify observers for subscription changes.
            autoPushFeature.registerForSubscriptions(object : PushSubscriptionObserver {
                override fun onSubscriptionAvailable(subscription: AutoPushSubscription) {
                    // Update for only the services subscription.
                    if (subscription.type == PushType.Services) {
                        logger.info("New push subscription received for FxA")
                        accountManager.authenticatedAccount()?.deviceConstellation()
                            ?.setDevicePushSubscriptionAsync(
                                DevicePushSubscription(
                                    endpoint = subscription.endpoint,
                                    publicKey = subscription.publicKey,
                                    authKey = subscription.authKey
                                )
                            )
                    }
                }
            })
        }
        accountManager.initAsync()
    }

    /**
     * Provides notification functionality, manages notification channels.
     */
    val notificationManager by lazy {
        NotificationManager(context)
    }
}

@VisibleForTesting(otherwise = PRIVATE)
class TelemetryAccountObserver(
    private val context: Context,
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
        context.settings().fxaSignedIn = true
    }

    override fun onLoggedOut() {
        metricController.track(Event.SyncAuthSignOut)
        // Used by Leanplum as a context variable.
        context.settings().fxaSignedIn = false
    }
}

/**
 * When we login/logout of FxA, we need to update our push subscriptions to match the newly
 * logged in account.
 *
 * We added the push service to the AccountManager observer so that we can control when the
 * service will start/stop. Firebase was added when landing the push service to ensure it works
 * as expected without causing any (as many) side effects.
 *
 * In order to use Firebase with Leanplum and other marketing features, we need it always
 * running so we cannot leave this code in place when we implement those features.
 *
 * We should have this removed when we are more confident
 * of the send-tab/push feature: https://github.com/mozilla-mobile/fenix/issues/4063
 */
@VisibleForTesting(otherwise = PRIVATE)
class PushAccountObserver(private val push: AutoPushFeature) : AccountObserver {
    override fun onLoggedOut() {
        push.unsubscribeForType(PushType.Services)
    }

    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
        if (authType != AuthType.Existing) {
            push.subscribeForType(PushType.Services)
        }
    }
}
