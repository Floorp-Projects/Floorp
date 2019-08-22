/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import android.preference.PreferenceManager
import androidx.lifecycle.ProcessLifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.push.Bus
import mozilla.components.concept.sync.AccountObserver
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
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.GlobalSyncableStoreProvider
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.Experiments
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.isInExperiment
import org.mozilla.fenix.test.Mockable
import org.mozilla.fenix.utils.Settings

/**
 * Component group for background services. These are the components that need to be accessed from within a
 * background worker.
 */
@Mockable
class BackgroundServices(
    context: Context,
    historyStorage: PlacesHistoryStorage,
    bookmarkStorage: PlacesBookmarksStorage
) {
    companion object {
        const val CLIENT_ID = "a2270f727f45f648"
        const val REDIRECT_URL = "https://accounts.firefox.com/oauth/success/$CLIENT_ID"
    }

    fun defaultDeviceName(context: Context): String = context.getString(
        R.string.default_device_name,
        context.getString(R.string.app_name),
        Build.MANUFACTURER,
        Build.MODEL
    )

    private val serverConfig = ServerConfig.release(CLIENT_ID, REDIRECT_URL)
    private val deviceConfig = DeviceConfig(
        name = defaultDeviceName(context),
        type = DeviceType.MOBILE,

        // NB: flipping this flag back and worth is currently not well supported and may need hand-holding.
        // Consult with the android-components peers before changing.
        // See https://github.com/mozilla/application-services/issues/1308
        capabilities = if (FeatureFlags.sendTabEnabled) {
            setOf(DeviceCapability.SEND_TAB)
        } else {
            emptySet()
        }
    )
    // If sync has been turned off on the server then disable syncing.
    private val syncConfig = if (context.isInExperiment(Experiments.asFeatureSyncDisabled)) {
        null
    } else {
        SyncConfig(setOf("history", "bookmarks"), syncPeriodInMinutes = 240L) // four hours
    }

    val pushConfig by lazy {
        val projectIdKey = context.getString(R.string.pref_key_push_project_id)
        val resId = context.resources.getIdentifier(projectIdKey, "string", context.packageName)
        if (resId == 0) {
            return@lazy null
        }
        val projectId = context.resources.getString(resId)
        PushConfig(projectId)
    }

    val pushService by lazy { FirebasePush() }

    val push by lazy {
        AutoPushFeature(
            context = context,
            service = pushService,
            config = pushConfig!!
        )
    }

    init {
        // Make the "history" and "bookmark" stores accessible to workers spawned by the sync manager.
        GlobalSyncableStoreProvider.configureStore("history" to historyStorage)
        GlobalSyncableStoreProvider.configureStore("bookmarks" to bookmarkStorage)
    }

    private val deviceEventObserver = object : DeviceEventsObserver {
        private val logger = Logger("DeviceEventsObserver")
        override fun onEvents(events: List<DeviceEvent>) {
            logger.info("Received ${events.size} device event(s)")
            events.filter { it is DeviceEvent.TabReceived }.forEach {
                notificationManager.showReceivedTabs(it as DeviceEvent.TabReceived)
            }
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
    private val accountObserver = object : AccountObserver {
        override fun onLoggedOut() {
            pushService.stop()

            push.unsubscribeForType(PushType.Services)

            context.components.analytics.metrics.track(Event.SyncAuthSignOut)

            Settings.getInstance(context).fxaSignedIn = false
        }

        override fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) {
            pushService.start(context)

            if (newAccount) {
                context.components.analytics.metrics.track(Event.FXANewSignup)
                push.subscribeForType(PushType.Services)
            }

            context.components.analytics.metrics.track(Event.SyncAuthSignIn)

            Settings.getInstance(context).fxaSignedIn = true
        }
    }

    private val preferences: SharedPreferences by lazy {
        PreferenceManager.getDefaultSharedPreferences(
            context
        )
    }

    val accountManager = FxaAccountManager(
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
    ).also {
        Settings.getInstance(context).fxaHasSyncedItems = syncConfig?.syncableStores?.isNotEmpty() ?: false

        if (FeatureFlags.sendTabEnabled) {
            it.registerForDeviceEvents(deviceEventObserver, ProcessLifecycleOwner.get(), false)

            // Enable push if we have the config.
            if (pushConfig != null) {

                // Register our account observer so we know how to update our push subscriptions.
                it.register(accountObserver)

                val logger = Logger("AutoPushFeature")

                // Notify observers for Services' messages.
                push.registerForPushMessages(
                    PushType.Services,
                    object : Bus.Observer<PushType, String> {
                        override fun onEvent(type: PushType, message: String) {
                            it.authenticatedAccount()?.deviceConstellation()
                                ?.processRawEventAsync(message)
                        }
                    },
                    ProcessLifecycleOwner.get(),
                    false
                )

                // Notify observers for subscription changes.
                push.registerForSubscriptions(object : PushSubscriptionObserver {
                    override fun onSubscriptionAvailable(subscription: AutoPushSubscription) {
                        // Update for only the services subscription.
                        if (subscription.type == PushType.Services) {
                            logger.info("New push subscription received for FxA")
                            it.authenticatedAccount()?.deviceConstellation()
                                ?.setDevicePushSubscriptionAsync(
                                    DevicePushSubscription(
                                        endpoint = subscription.endpoint,
                                        publicKey = subscription.publicKey,
                                        authKey = subscription.authKey
                                    )
                                )
                        }
                    }
                }, ProcessLifecycleOwner.get(), false)

                // For all the current Fenix users, we need to remove the current push token and
                // re-subscribe again on the right push server. We should never do this otherwise!
                // Should be removed after majority of our users are correctly subscribed.
                // See: https://github.com/mozilla-mobile/fenix/issues/4218
                val prefResetSubKey = "reset_broken_push_subscription"
                if (!preferences.getBoolean(prefResetSubKey, false)) {
                    preferences.edit().putBoolean(prefResetSubKey, true).apply()
                    logger.info("Forcing push registration renewal")
                    push.forceRegistrationRenewal()
                }
            }
        }
        CoroutineScope(Dispatchers.Main).launch { it.initAsync().await() }
    }

    /**
     * Provides notification functionality, manages notification channels.
     */
    val notificationManager by lazy {
        NotificationManager(context)
    }
}
