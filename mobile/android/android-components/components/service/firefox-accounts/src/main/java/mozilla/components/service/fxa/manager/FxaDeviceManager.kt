/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import android.content.Context
import androidx.annotation.GuardedBy
import androidx.annotation.VisibleForTesting
import androidx.work.Constraints
import androidx.work.CoroutineWorker
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AuthException
import mozilla.components.concept.sync.AuthExceptionType
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.service.fxa.FxaException
import mozilla.components.service.fxa.FxaPanicException
import mozilla.components.service.fxa.FxaUnauthorizedException
import mozilla.components.support.base.log.logger.Logger

import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import java.util.concurrent.TimeUnit

/**
 * Device manager is responsible for providing information about the current device and other
 * available devices in the constellation. Event polling is available and received events are
 * exposed via an observable interface.
 */
internal open class PollingDeviceManager(
    private val constellation: DeviceConstellation,
    private val scope: CoroutineScope,
    private val deviceObserverRegistry: ObserverRegistry<DeviceConstellationObserver>
) : Observable<DeviceEventsObserver> by ObserverRegistry() {
    private val logger = Logger("FxaEventsManager")

    @GuardedBy("this")
    @Volatile
    private var constellationState: ConstellationState? = null

    fun constellationState(): ConstellationState? = synchronized(this) {
        return constellationState
    }

    fun startPolling() {
        getRefreshManager().startRefresh()
    }

    fun stopPolling() {
        getRefreshManager().stopRefresh()
    }

    fun pollAsync(): Deferred<Unit> {
        return scope.async {
            logger.debug("poll")
            val events = try {
                constellation.pollForEventsAsync().await()
            } catch (e: FxaPanicException) {
                // Re-throw panics.
                throw e
            } catch (e: FxaUnauthorizedException) {
                // Propagate auth errors.
                authErrorRegistry.notifyObservers { onAuthErrorAsync(AuthException(AuthExceptionType.UNAUTHORIZED, e)) }
                return@async
            } catch (e: FxaException) {
                // Ignore intermittent errors.
                logger.warn("Failed to poll for events", e)
                return@async
            }
            processEvents(events)
        }
    }

    fun processEvents(events: List<DeviceEvent>) {
        // In the future, this could be an internal device-related events processing layer.
        // E.g., once we grow events such as "there is a new device in the constellation".
        logger.debug("processing events: $events")
        // NB: we don't use Dispatcher.Main here like in refresh method because our
        // observer is internal, and we depend on it to relay using the Main dispatcher.
        notifyObservers { onEvents(events) }
    }

    fun refreshDevicesAsync(): Deferred<Unit> = synchronized(this) {
        return scope.async {
            logger.info("Refreshing device list...")

            val allDevices = try {
                constellation.fetchAllDevicesAsync().await()
            } catch (e: FxaPanicException) {
                // Re-throw panics.
                throw e
            } catch (e: FxaUnauthorizedException) {
                // Propagate auth errors.
                authErrorRegistry.notifyObservers { onAuthErrorAsync(AuthException(AuthExceptionType.UNAUTHORIZED, e)) }
                return@async
            } catch (e: FxaException) {
                // Ignore intermittent errors.
                logger.warn("Failed to fetch devices", e)
                return@async
            }

            // Find the current device.
            val currentDevice = allDevices.find { it.isCurrentDevice }
            // Filter out the current devices.
            val otherDevices = allDevices.filter { !it.isCurrentDevice }

            val newState = ConstellationState(currentDevice, otherDevices)
            constellationState = newState

            // TODO notifyObserversOnMainThread
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
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    open fun getRefreshManager(): PeriodicRefreshManager {
        return FxaDeviceRefreshManager
    }
}

// This interface exists to facilitate testing.
interface PeriodicRefreshManager {
    fun startRefresh()
    fun stopRefresh()
}

/**
 * Responsible for periodically checking for new device events and refreshing device constellation state.
 */
internal object FxaDeviceRefreshManager : PeriodicRefreshManager {
    // Minimal interval supported by WorkManager is 15 minutes.
    private const val DEVICE_EVENT_POLL_WORKER = "DeviceEventPolling"
    private const val POLL_PERIOD = 15L
    private val POLL_PERIOD_UNIT = TimeUnit.MINUTES

    override fun startRefresh() {
        WorkManager.getInstance().enqueueUniquePeriodicWork(
                DEVICE_EVENT_POLL_WORKER,
                ExistingPeriodicWorkPolicy.REPLACE,
                PeriodicWorkRequestBuilder<WorkManagerDeviceRefreshWorker>(POLL_PERIOD, POLL_PERIOD_UNIT)
                        .setConstraints(
                                Constraints.Builder()
                                        .setRequiredNetworkType(NetworkType.CONNECTED)
                                        .build()
                        )
                        .build()
        )
    }

    override fun stopRefresh() {
        WorkManager.getInstance().cancelUniqueWork(DEVICE_EVENT_POLL_WORKER)
    }
}

internal object DeviceManagerProvider {
    var deviceManager: PollingDeviceManager? = null
}

internal class WorkManagerDeviceRefreshWorker(
    context: Context,
    params: WorkerParameters
) : CoroutineWorker(context, params) {
    private val logger = Logger("DeviceManagerPollWorker")

    @Suppress("ReturnCount", "ComplexMethod")
    override suspend fun doWork(): Result {
        logger.debug("Polling for new events via ${DeviceManagerProvider.deviceManager}")

        DeviceManagerProvider.deviceManager?.let { deviceManager ->
            deviceManager.refreshDevicesAsync().await()
            deviceManager.pollAsync().await()
        }

        return Result.success()
    }
}
