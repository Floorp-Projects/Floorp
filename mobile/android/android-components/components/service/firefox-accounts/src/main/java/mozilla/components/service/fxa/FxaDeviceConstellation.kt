/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import android.content.Context
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
import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.DeviceCommandOutgoing
import mozilla.components.concept.sync.DevicePushSubscription
import mozilla.components.concept.sync.DeviceType
import mozilla.components.support.base.crash.CrashReporting
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.sync.telemetry.SyncTelemetry

/**
 * Provides an implementation of [DeviceConstellation] backed by a [FirefoxAccount].
 */
@SuppressWarnings("TooManyFunctions")
class FxaDeviceConstellation(
    private val account: FirefoxAccount,
    private val scope: CoroutineScope,
    private val crashReporter: CrashReporting? = null
) : DeviceConstellation, Observable<AccountEventsObserver> by ObserverRegistry() {
    private val logger = Logger("FxaDeviceConstellation")

    private val deviceObserverRegistry = ObserverRegistry<DeviceConstellationObserver>()

    @Volatile
    private var constellationState: ConstellationState? = null

    override fun state(): ConstellationState? = constellationState

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
            handleFxaExceptions(logger, "processing raw commands") {
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

    override fun setDeviceNameAsync(name: String, context: Context): Deferred<Boolean> {
        return scope.async {
            val rename = handleFxaExceptions(logger, "changing device name") {
                account.setDeviceDisplayName(name)
            }
            FxaDeviceSettingsCache(context).updateCachedName(name)
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

    override fun sendCommandToDeviceAsync(
        targetDeviceId: String,
        outgoingCommand: DeviceCommandOutgoing
    ): Deferred<Boolean> {
        return scope.async {
            handleFxaExceptions(logger, "sending device command") {
                when (outgoingCommand) {
                    is DeviceCommandOutgoing.SendTab -> {
                        account.sendSingleTab(targetDeviceId, outgoingCommand.title, outgoingCommand.url)
                        SyncTelemetry.processFxaTelemetry(account.gatherTelemetry(), crashReporter)
                    }
                    else -> logger.debug("Skipped sending unsupported command type: $outgoingCommand")
                }
            }
        }
    }

    // Poll for missed commands. Commands are the only event-type that can be
    // polled for, although missed commands will be delivered as AccountEvents.
    override fun pollForCommandsAsync(): Deferred<Boolean> {
        return scope.async {
            val events = handleFxaExceptions(logger, "polling for device commands", { null }) {
                account.pollDeviceCommands().map { AccountEvent.DeviceCommandIncoming(command = it.into()) }
            }

            if (events == null) {
                false
            } else {
                processEvents(events)
                SyncTelemetry.processFxaTelemetry(account.gatherTelemetry(), crashReporter)
                true
            }
        }
    }

    private fun processEvents(events: List<AccountEvent>) = CoroutineScope(Dispatchers.Main).launch {
        notifyObservers { onEvents(events) }
    }

    override fun refreshDevicesAsync(): Deferred<Boolean> = scope.async {
        logger.info("Refreshing device list...")

        // Attempt to fetch devices, or bail out on failure.
        val allDevices = fetchAllDevicesAsync().await() ?: return@async false

        // Find the current device.
        val currentDevice = allDevices.find { it.isCurrentDevice }?.also {
            // Check if our current device's push subscription needs to be renewed.
            if (it.subscriptionExpired) {
                logger.info("Current device needs push endpoint registration")
            }
        }

        // Filter out the current devices.
        val otherDevices = allDevices.filter { !it.isCurrentDevice }

        val newState = ConstellationState(currentDevice, otherDevices)
        constellationState = newState

        logger.info("Refreshed device list; saw ${allDevices.size} device(s).")

        CoroutineScope(Dispatchers.Main).launch {
            // NB: at this point, 'constellationState' might have changed.
            // Notify with an immutable, local 'newState' instead.
            deviceObserverRegistry.notifyObservers { onDevicesUpdate(newState) }
        }

        true
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
