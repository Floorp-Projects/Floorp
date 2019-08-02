/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.sendtab

import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.concept.push.Bus
import mozilla.components.concept.push.PushService
import mozilla.components.concept.sync.AccountObserver as SyncAccountObserver
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.DevicePushSubscription
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.TabData
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import mozilla.components.feature.push.PushSubscriptionObserver
import mozilla.components.feature.push.PushType
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.base.log.logger.Logger

/**
 * A feature that uses the [FxaAccountManager] to send and receive tabs with optional push support
 * for receiving tabs from the [AutoPushFeature] and a [PushService].
 *
 * If the push components are not used, the feature can still function while tabs would only be
 * received when refreshing the device state.
 *
 * @param accountManager Firefox account manager.
 * @param pushFeature The [AutoPushFeature] if that is setup for observing push events.
 * @param owner Android lifecycle owner for the observers. Defaults to the [ProcessLifecycleOwner]
 * so that we can always observe events throughout the application lifecycle.
 * @param autoPause whether or not the observer should automatically be
 * paused/resumed with the bound lifecycle.
 * @param onTabsReceived the callback invoked with new tab(s) are received.
 */
class SendTabFeature(
    accountManager: FxaAccountManager,
    pushFeature: AutoPushFeature? = null,
    owner: LifecycleOwner = ProcessLifecycleOwner.get(),
    autoPause: Boolean = false,
    onTabsReceived: (Device?, List<TabData>) -> Unit
) {
    init {
        val accountObserver = AccountObserver(pushFeature)
        val pushObserver = PushObserver(accountManager)
        val deviceObserver = DeviceObserver(onTabsReceived)

        // Always observe the account for device events.
        accountManager.registerForDeviceEvents(deviceObserver, owner, autoPause)

        pushFeature?.apply {
            registerForPushMessages(PushType.Services, pushObserver, owner, autoPause)
            registerForSubscriptions(pushObserver, owner, autoPause)

            // observe the account only if we have the push feature (service is optional)
            accountManager.register(accountObserver, owner, autoPause)
        }
    }
}

internal class PushObserver(
    private val accountManager: FxaAccountManager
) : Bus.Observer<PushType, String>, PushSubscriptionObserver {
    private val logger = Logger("PushObserver")

    override fun onSubscriptionAvailable(subscription: AutoPushSubscription) {
        logger.debug("Received new push subscription from $subscription.type")

        if (subscription.type == PushType.Services) {
            accountManager.withConstellation {
                it.setDevicePushSubscriptionAsync(
                    DevicePushSubscription(
                        endpoint = subscription.endpoint,
                        publicKey = subscription.publicKey,
                        authKey = subscription.authKey
                    )
                )
            }
        }
    }

    override fun onEvent(type: PushType, message: String) {
        logger.debug("Received new push message for $type")

        accountManager.withConstellation {
            it.processRawEventAsync(message)
        }
    }
}

internal class DeviceObserver(
    private val onTabsReceived: (Device?, List<TabData>) -> Unit
) : DeviceEventsObserver {
    private val logger = Logger("DeviceObserver")

    override fun onEvents(events: List<DeviceEvent>) {
        events.asSequence()
            .filterIsInstance<DeviceEvent.TabReceived>()
            .forEach { event ->
                logger.debug("Showing ${event.entries.size} tab(s) received from deviceID=${event.from?.id}")

                onTabsReceived(event.from, event.entries)
            }
    }
}

internal class AccountObserver(
    private val feature: AutoPushFeature?
) : SyncAccountObserver {
    private val logger = Logger("AccountObserver")

    override fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) {
        // We need a new subscription only when we have a new account.
        // This is removed when an account logs out.
        if (newAccount) {
            logger.debug("Subscribing for ${PushType.Services} events.")

            feature?.subscribeForType(PushType.Services)
        }
    }

    override fun onLoggedOut() {
        logger.debug("Unsubscribing for ${PushType.Services} events.")

        feature?.unsubscribeForType(PushType.Services)
    }
}

internal inline fun FxaAccountManager.withConstellation(block: (DeviceConstellation) -> Unit) {
    authenticatedAccount()?.let {
        block(it.deviceConstellation())
    }
}
