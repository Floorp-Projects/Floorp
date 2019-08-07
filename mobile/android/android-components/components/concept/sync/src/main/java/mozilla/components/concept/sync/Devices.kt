/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:SuppressWarnings("TooManyFunctions")
package mozilla.components.concept.sync

import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.Deferred
import mozilla.components.support.base.observer.Observable

/**
 * Describes available interactions with the current device and other devices associated with an [OAuthAccount].
 */
interface DeviceConstellation : Observable<DeviceEventsObserver> {
    /**
     * Register current device in the associated [DeviceConstellation].
     *
     * @param name An initial name for the current device. This may be changed via [setDeviceNameAsync].
     * @param type Type of the current device. This can't be changed.
     * @param capabilities A list of capabilities that the current device claims to have.
     * @return A [Deferred] that will be resolved with a success flag once operation is complete.
     */
    fun initDeviceAsync(
        name: String,
        type: DeviceType = DeviceType.MOBILE,
        capabilities: Set<DeviceCapability>
    ): Deferred<Boolean>

    /**
     * Ensure that all passed in [capabilities] are configured.
     * This may involve backend service registration, or other work involving network/disc access.
     * @param capabilities A list of capabilities to configure. This is expected to be the same or
     * longer list than what was passed into [initDeviceAsync]. Removing capabilities is currently
     * not supported.
     * @return A [Deferred] that will be resolved with a success flag once operation is complete.
     */
    fun ensureCapabilitiesAsync(capabilities: Set<DeviceCapability>): Deferred<Boolean>

    /**
     * Current state of the constellation. May be missing if state was never queried.
     * @return [ConstellationState] describes current and other known devices in the constellation.
     */
    fun state(): ConstellationState?

    /**
     * Allows monitoring state of the device constellation via [DeviceConstellationObserver].
     * Use this to be notified of changes to the current device or other devices.
     */
    fun registerDeviceObserver(observer: DeviceConstellationObserver, owner: LifecycleOwner, autoPause: Boolean)

    /**
     * Get all devices in the constellation.
     * @return A list of all devices in the constellation, or `null` on failure.
     */
    fun fetchAllDevicesAsync(): Deferred<List<Device>?>

    /**
     * Set name of the current device.
     * @param name New device name.
     * @return A [Deferred] that will be resolved with a success flag once operation is complete.
     */
    fun setDeviceNameAsync(name: String): Deferred<Boolean>

    /**
     * Set a [DevicePushSubscription] for the current device.
     * @param subscription A new [DevicePushSubscription].
     * @return A [Deferred] that will be resolved with a success flag once operation is complete.
     */
    fun setDevicePushSubscriptionAsync(subscription: DevicePushSubscription): Deferred<Boolean>

    /**
     * Send an event to a specified device.
     * @param targetDeviceId A device ID of the recipient.
     * @param outgoingEvent An event to send.
     * @return A [Deferred] that will be resolved with a success flag once operation is complete.
     */
    fun sendEventToDeviceAsync(targetDeviceId: String, outgoingEvent: DeviceEventOutgoing): Deferred<Boolean>

    /**
     * Process a raw event, obtained via a push message or some other out-of-band mechanism.
     * @param payload A raw, plaintext payload to be processed.
     * @return A [Deferred] that will be resolved with a success flag once operation is complete.
     */
    fun processRawEventAsync(payload: String): Deferred<Boolean>

    /**
     * Poll for events targeted at the current [Device]. It's expected that if a [DeviceEvent] was
     * returned after a poll, it will not be returned in consequent polls.
     * @return A list of [DeviceEvent] instances that are currently pending for this [Device], or `null` on failure.
     */
    fun pollForEventsAsync(): Deferred<List<DeviceEvent>?>

    /**
     * Begin periodically refreshing constellation state, including polling for events.
     */
    fun startPeriodicRefresh()

    /**
     * Stop periodically refreshing constellation state and polling for events.
     */
    fun stopPeriodicRefresh()

    /**
     * Refreshes [ConstellationState] and polls for device events.
     *
     * @return A [Deferred] that will be resolved with a success flag once operation is complete. Failure may
     * indicate a partial success.
     */
    fun refreshDeviceStateAsync(): Deferred<Boolean>
}

/**
 * Describes current device and other devices in the constellation.
 */
data class ConstellationState(val currentDevice: Device?, val otherDevices: List<Device>)

/**
 * Allows monitoring constellation state.
 */
interface DeviceConstellationObserver {
    fun onDevicesUpdate(constellation: ConstellationState)
}

/**
 * Describes a type of the physical device in the constellation.
 */
enum class DeviceType {
    DESKTOP,
    MOBILE,
    UNKNOWN
}

/**
 * Describes an Autopush-compatible push channel subscription.
 */
data class DevicePushSubscription(
    val endpoint: String,
    val publicKey: String,
    val authKey: String
)

/**
 * Capabilities that a [Device] may have.
 */
enum class DeviceCapability {
    SEND_TAB
}

/**
 * Describes a device in the [DeviceConstellation].
 */
data class Device(
    val id: String,
    val displayName: String,
    val deviceType: DeviceType,
    val isCurrentDevice: Boolean,
    val lastAccessTime: Long?,
    val capabilities: List<DeviceCapability>,
    val subscriptionExpired: Boolean,
    val subscription: DevicePushSubscription?
)
