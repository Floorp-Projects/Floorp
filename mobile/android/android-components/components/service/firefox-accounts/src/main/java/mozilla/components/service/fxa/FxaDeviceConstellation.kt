/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import androidx.annotation.GuardedBy
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.appservices.fxaclient.FirefoxAccount
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventOutgoing
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.DevicePushSubscription
import mozilla.components.concept.sync.DeviceType
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

/**
 * Provides an implementation of [DeviceConstellation] backed by a [FirefoxAccount].
 */
@SuppressWarnings("TooManyFunctions")
class FxaDeviceConstellation(
    private val account: FirefoxAccount,
    private val scope: CoroutineScope
) : DeviceConstellation, Observable<DeviceEventsObserver> by ObserverRegistry() {
    private val logger = Logger("FxaDeviceConstellation")

    private val deviceObserverRegistry = ObserverRegistry<DeviceConstellationObserver>()

    @GuardedBy("this")
    private var constellationState: ConstellationState? = null

    override fun state(): ConstellationState? = synchronized(this) {
        return constellationState
    }

    override fun initDeviceAsync(
        name: String,
        type: DeviceType,
        capabilities: Set<DeviceCapability>
    ): Deferred<Boolean> {
        return scope.async {
            handleFxaExceptions(logger, "initializing device") {
                account.initializeDevice(name, type.into(), capabilities.map { it.into() }.toSet())
            }
        }
    }

    override fun ensureCapabilitiesAsync(capabilities: Set<DeviceCapability>): Deferred<Boolean> {
        return scope.async {
            handleFxaExceptions(logger, "ensuring capabilities") {
                account.ensureCapabilities(capabilities.map { it.into() }.toSet())
            }
        }
    }

    override fun processRawEventAsync(payload: String): Deferred<Boolean> {
        return scope.async {
            handleFxaExceptions(logger, "processing raw events") {
                processEvents(account.handlePushMessage(payload).map { it.into() })
            }
        }
    }

    override fun registerDeviceObserver(
        observer: DeviceConstellationObserver,
        owner: LifecycleOwner,
        autoPause: Boolean
    ) {
        deviceObserverRegistry.register(observer, owner, autoPause)
    }

    override fun setDeviceNameAsync(name: String): Deferred<Boolean> {
        return scope.async {
            val rename = handleFxaExceptions(logger, "changing device name") {
                account.setDeviceDisplayName(name)
            }
            // See the latest device (name) changes after changing it.
            val refreshDevices = refreshDevicesAsync().await()

            rename && refreshDevices
        }
    }

    override fun setDevicePushSubscriptionAsync(subscription: DevicePushSubscription): Deferred<Boolean> {
        return scope.async {
            handleFxaExceptions(logger, "updating device push subscription") {
                account.setDevicePushSubscription(
                        subscription.endpoint, subscription.publicKey, subscription.authKey
                )
            }
        }
    }

    override fun sendEventToDeviceAsync(targetDeviceId: String, outgoingEvent: DeviceEventOutgoing): Deferred<Boolean> {
        return scope.async {
            handleFxaExceptions(logger, "sending device event") {
                when (outgoingEvent) {
                    is DeviceEventOutgoing.SendTab -> {
                        account.sendSingleTab(targetDeviceId, outgoingEvent.title, outgoingEvent.url)
                    }
                    else -> logger.debug("Skipped sending unsupported event type: $outgoingEvent")
                }
            }
        }
    }

    override fun pollForEventsAsync(): Deferred<Boolean> {
        return scope.async {
            val events = handleFxaExceptions(logger, "polling for device events", { null }) {
                account.pollDeviceCommands().map { it.into() }
            }

            if (events == null) {
                false
            } else {
                processEvents(events)
                true
            }
        }
    }

    private fun processEvents(events: List<DeviceEvent>) = CoroutineScope(Dispatchers.Main).launch {
        notifyObservers { onEvents(events) }
    }

    override fun refreshDevicesAsync(): Deferred<Boolean> = synchronized(this) {
        return scope.async {
            logger.info("Refreshing device list...")

            // Attempt to fetch devices, or bail out on failure.
            val allDevices = fetchAllDevicesAsync().await() ?: return@async false

            // Find the current device.
            val currentDevice = allDevices.find { it.isCurrentDevice }
            // Filter out the current devices.
            val otherDevices = allDevices.filter { !it.isCurrentDevice }

            val newState = ConstellationState(currentDevice, otherDevices)
            constellationState = newState

            CoroutineScope(Dispatchers.Main).launch {
                // NB: at this point, 'constellationState' might have changed.
                // Notify with an immutable, local 'newState' instead.
                deviceObserverRegistry.notifyObservers { onDevicesUpdate(newState) }
            }

            // Check if our current device's push subscription needs to be renewed.
            constellationState?.currentDevice?.let {
                if (it.subscriptionExpired) {
                    logger.info("Current device needs push endpoint registration")
                }
            }

            logger.info("Refreshed device list; saw ${allDevices.size} device(s).")
            true
        }
    }

    /**
     * Get all devices in the constellation.
     * @return A list of all devices in the constellation, or `null` on failure.
     */
    private fun fetchAllDevicesAsync(): Deferred<List<Device>?> {
        return scope.async {
            handleFxaExceptions(logger, "fetching all devices", { null }) {
                account.getDevices().map { it.into() }
            }
        }
    }
}
