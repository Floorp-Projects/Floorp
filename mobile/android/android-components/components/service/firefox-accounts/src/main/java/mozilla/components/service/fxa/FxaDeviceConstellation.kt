/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.withContext
import mozilla.appservices.fxaclient.PersistedFirefoxAccount as FirefoxAccount
import mozilla.appservices.fxaclient.FxaErrorException as FxaException
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.DeviceCommandOutgoing
import mozilla.components.concept.sync.DeviceConfig
import mozilla.components.concept.sync.DevicePushSubscription
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.sync.ServiceResult
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.sync.telemetry.SyncTelemetry

internal sealed class FxaDeviceConstellationException : Exception() {
    /**
     * Failure while ensuring device capabilities.
     */
    class EnsureCapabilitiesFailed : FxaDeviceConstellationException()
}

/**
 * Provides an implementation of [DeviceConstellation] backed by a [FirefoxAccount].
 */
@SuppressWarnings("TooManyFunctions")
class FxaDeviceConstellation(
    private val account: FirefoxAccount,
    private val scope: CoroutineScope,
    @VisibleForTesting
    internal val crashReporter: CrashReporting? = null
) : DeviceConstellation, Observable<AccountEventsObserver> by ObserverRegistry() {
    private val logger = Logger("FxaDeviceConstellation")

    private val deviceObserverRegistry = ObserverRegistry<DeviceConstellationObserver>()

    @Volatile
    private var constellationState: ConstellationState? = null

    override fun state(): ConstellationState? = constellationState

    @VisibleForTesting
    internal enum class DeviceFinalizeAction {
        Initialize,
        EnsureCapabilities,
        None
    }

    @Suppress("ComplexMethod")
    @Throws(FxaPanicException::class)
    override suspend fun finalizeDevice(
        authType: AuthType,
        config: DeviceConfig
    ): ServiceResult = withContext(scope.coroutineContext) {
        val finalizeAction = when (authType) {
            AuthType.Signin,
            AuthType.Signup,
            AuthType.Pairing,
            is AuthType.OtherExternal,
            AuthType.MigratedCopy -> DeviceFinalizeAction.Initialize
            AuthType.Existing,
            AuthType.MigratedReuse -> DeviceFinalizeAction.EnsureCapabilities
            AuthType.Recovered -> DeviceFinalizeAction.None
        }

        if (finalizeAction == DeviceFinalizeAction.None) {
            ServiceResult.Ok
        } else {
            val capabilities = config.capabilities.map { it.into() }.toSet()
            if (finalizeAction == DeviceFinalizeAction.Initialize) {
                try {
                    account.initializeDevice(config.name, config.type.into(), capabilities)
                    ServiceResult.Ok
                } catch (e: FxaPanicException) {
                    throw e
                } catch (e: FxaUnauthorizedException) {
                    ServiceResult.AuthError
                } catch (e: FxaException) {
                    ServiceResult.OtherError
                }
            } else {
                try {
                    account.ensureCapabilities(capabilities)
                    ServiceResult.Ok
                } catch (e: FxaPanicException) {
                    throw e
                } catch (e: FxaUnauthorizedException) {
                    // Unless we've added a new capability, in practice 'ensureCapabilities' isn't
                    // actually expected to do any work: everything should have been done by initializeDevice.
                    // So if it did, and failed, let's report this so that we're aware of this!
                    // See https://github.com/mozilla-mobile/android-components/issues/8164
                    crashReporter?.submitCaughtException(FxaDeviceConstellationException.EnsureCapabilitiesFailed())
                    ServiceResult.AuthError
                } catch (e: FxaException) {
                    ServiceResult.OtherError
                }
            }
        }
    }

    override suspend fun processRawEvent(payload: String) = withContext(scope.coroutineContext) {
        handleFxaExceptions(logger, "processing raw commands") {
            processEvents(account.handlePushMessage(payload).map { it.into() })
        }
    }

    override fun registerDeviceObserver(
        observer: DeviceConstellationObserver,
        owner: LifecycleOwner,
        autoPause: Boolean
    ) {
        deviceObserverRegistry.register(observer, owner, autoPause)
    }

    override suspend fun setDeviceName(name: String, context: Context) = withContext(scope.coroutineContext) {
        val rename = handleFxaExceptions(logger, "changing device name") {
            account.setDeviceDisplayName(name)
        }
        FxaDeviceSettingsCache(context).updateCachedName(name)
        // See the latest device (name) changes after changing it.

        rename && refreshDevices()
    }

    override suspend fun setDevicePushSubscription(
        subscription: DevicePushSubscription
    ) = withContext(scope.coroutineContext) {
        handleFxaExceptions(logger, "updating device push subscription") {
            account.setDevicePushSubscription(
                    subscription.endpoint, subscription.publicKey, subscription.authKey
            )
        }
    }

    override suspend fun sendCommandToDevice(
        targetDeviceId: String,
        outgoingCommand: DeviceCommandOutgoing
    ) = withContext(scope.coroutineContext) {
        val result = handleFxaExceptions(logger, "sending device command", { error -> error }) {
            when (outgoingCommand) {
                is DeviceCommandOutgoing.SendTab -> {
                    account.sendSingleTab(targetDeviceId, outgoingCommand.title, outgoingCommand.url)
                    SyncTelemetry.processFxaTelemetry(account.gatherTelemetry(), crashReporter)
                }
                else -> logger.debug("Skipped sending unsupported command type: $outgoingCommand")
            }
            null
        }

        if (result != null) {
            when (result) {
                // Don't submit network exceptions to our crash reporter. They're just noise.
                is FxaException.Network -> {
                    logger.warn("Failed to 'sendCommandToDevice' due to a network exception")
                }
                else -> {
                    logger.warn("Failed to 'sendCommandToDevice'", result)
                    crashReporter?.submitCaughtException(SendCommandException(result))
                }
            }

            false
        } else {
            true
        }
    }

    // Poll for missed commands. Commands are the only event-type that can be
    // polled for, although missed commands will be delivered as AccountEvents.
    override suspend fun pollForCommands() = withContext(scope.coroutineContext) {
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

    private fun processEvents(events: List<AccountEvent>) {
        notifyObservers { onEvents(events) }
    }

    override suspend fun refreshDevices(): Boolean {
        return withContext(scope.coroutineContext) {
            logger.info("Refreshing device list...")

            // Attempt to fetch devices, or bail out on failure.
            val allDevices = fetchAllDevices() ?: return@withContext false

            // Find the current device.
            val currentDevice = allDevices.find { it.isCurrentDevice }?.also {
                // Check if our current device's push subscription needs to be renewed.
                if (it.subscription == null || it.subscriptionExpired) {
                    logger.info("Current device needs push endpoint registration, so checking for missed commands")
                    pollForCommands()
                }
            }

            // Filter out the current devices.
            val otherDevices = allDevices.filter { !it.isCurrentDevice }

            val newState = ConstellationState(currentDevice, otherDevices)
            constellationState = newState

            logger.info("Refreshed device list; saw ${allDevices.size} device(s).")

            // NB: at this point, 'constellationState' might have changed.
            // Notify with an immutable, local 'newState' instead.
            deviceObserverRegistry.notifyObservers {
                logger.info("Notifying observer about constellation updates.")
                onDevicesUpdate(newState)
            }

            true
        }
    }

    /**
     * Get all devices in the constellation.
     * @return A list of all devices in the constellation, or `null` on failure.
     */
    private suspend fun fetchAllDevices(): List<Device>? {
        return handleFxaExceptions(logger, "fetching all devices", { null }) {
            account.getDevices().map { it.into() }
        }
    }
}
