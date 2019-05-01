/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

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
import mozilla.components.service.fxa.manager.DeviceManagerProvider
import mozilla.components.service.fxa.manager.PollingDeviceManager
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
    private val deviceManager = PollingDeviceManager(this, scope, deviceObserverRegistry)

    init {
        DeviceManagerProvider.deviceManager = deviceManager

        deviceManager.register(object : DeviceEventsObserver {
            override fun onEvents(events: List<DeviceEvent>) {
                // TODO notifyObserversOnMainThread
                CoroutineScope(Dispatchers.Main).launch {
                    notifyObservers { onEvents(events) }
                }
            }
        })
    }

    override fun initDeviceAsync(name: String, type: DeviceType, capabilities: List<DeviceCapability>): Deferred<Unit> {
        return scope.async {
            account.initializeDevice(name, type.into(), capabilities.map { it.into() }.toSet())
        }
    }

    override fun ensureCapabilitiesAsync(): Deferred<Unit> {
        return scope.async {
            account.ensureCapabilities()
        }
    }

    override fun processRawEvent(payload: String) {
        scope.launch {
            val events = account.handlePushMessage(payload).filter {
                it is mozilla.appservices.fxaclient.AccountEvent.TabReceived
            }.map {
                (it as mozilla.appservices.fxaclient.AccountEvent.TabReceived).into()
            }
            deviceManager.processEvents(events)
        }
    }

    override fun pollForEventsAsync(): Deferred<List<DeviceEvent>> {
        // Currently ignoring non-TabReceived events.
        return scope.async {
            account.pollDeviceCommands().filter {
                it is mozilla.appservices.fxaclient.AccountEvent.TabReceived
            }.map {
                (it as mozilla.appservices.fxaclient.AccountEvent.TabReceived).into()
            }
        }
    }

    override fun fetchAllDevicesAsync(): Deferred<List<Device>> {
        return scope.async {
            account.getDevices().map { it.into() }
        }
    }

    override fun registerDeviceObserver(
        observer: DeviceConstellationObserver,
        owner: LifecycleOwner,
        autoPause: Boolean
    ) {
        deviceObserverRegistry.register(observer, owner, autoPause)
    }

    override fun setDeviceNameAsync(name: String): Deferred<Unit> {
        return scope.async {
            account.setDeviceDisplayName(name)
        }
    }

    override fun setDevicePushSubscriptionAsync(subscription: DevicePushSubscription): Deferred<Unit> {
        return scope.async {
            account.setDevicePushSubscription(
                    subscription.endpoint, subscription.publicKey, subscription.authKey
            )
        }
    }

    override fun sendEventToDeviceAsync(targetDeviceId: String, outgoingEvent: DeviceEventOutgoing): Deferred<Unit> {
        return scope.async {
            when (outgoingEvent) {
                is DeviceEventOutgoing.SendTab -> {
                    account.sendSingleTab(targetDeviceId, outgoingEvent.title, outgoingEvent.url)
                }
                else -> logger.debug("Skipped sending unsupported event type: $outgoingEvent")
            }
        }
    }

    override fun state(): ConstellationState? {
        return deviceManager.constellationState()
    }

    override fun startPeriodicRefresh() {
        deviceManager.startPolling()
    }

    override fun stopPeriodicRefresh() {
        deviceManager.stopPolling()
    }

    override fun refreshDeviceStateAsync(): Deferred<Unit> {
        return scope.async {
            deviceManager.refreshDevicesAsync().await()
            deviceManager.processEvents(pollForEventsAsync().await())
        }
    }
}
