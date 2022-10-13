/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.async
import kotlinx.coroutines.plus
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceCommandOutgoing.SendTab
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.TabData
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.ktx.kotlin.crossProduct
import kotlin.coroutines.CoroutineContext

/**
 * Contains use cases for sending tabs to devices related to the firefox-accounts.
 *
 * See [SendTabFeature] for the ability to receive tabs from other devices.
 *
 * @param accountManager The AccountManager on which we want to retrieve our devices.
 * @param coroutineContext The Coroutine Context on which we want to do the actual sending.
 * By default, we want to do this on the IO dispatcher since it involves making network requests to
 * the Sync servers.
 */
class SendTabUseCases(
    accountManager: FxaAccountManager,
    coroutineContext: CoroutineContext = Dispatchers.IO,
) {
    private var job: Job = SupervisorJob()
    private val scope = CoroutineScope(coroutineContext) + job

    class SendToDeviceUseCase internal constructor(
        private val accountManager: FxaAccountManager,
        private val scope: CoroutineScope,
    ) {
        /**
         * Sends the tab to provided deviceId if possible.
         *
         * @param deviceId The deviceId to send the tab.
         * @param tab The tab to send.
         * @return a deferred boolean if the result was successful or not.
         */
        operator fun invoke(deviceId: String, tab: TabData) =
            scope.async { send(deviceId, tab) }

        /**
         * Sends the tabs to provided deviceId if possible.
         *
         * @param deviceId The deviceId to send the tab.
         * @param tabs The list of tabs to send.
         * @return a deferred boolean as true if the combined result was successful or not.
         */
        operator fun invoke(deviceId: String, tabs: List<TabData>): Deferred<Boolean> {
            return scope.async {
                tabs.map { tab ->
                    send(deviceId, tab)
                }.fold(true) { acc, result ->
                    acc and result
                }
            }
        }

        private suspend fun send(deviceId: String, tab: TabData): Boolean {
            // Filter tabs that don't have a send-capable uri
            if (!isValidTabSchema(tab)) {
                return false
            }
            filterSendTabDevices(accountManager) { constellation, devices ->
                val device = devices.firstOrNull {
                    it.id == deviceId
                }
                device?.let {
                    return constellation.sendCommandToDevice(
                        device.id,
                        SendTab(tab.title, tab.url),
                    )
                }
            }

            return false
        }
    }

    class SendToAllUseCase internal constructor(
        private val accountManager: FxaAccountManager,
        private val scope: CoroutineScope,
    ) {

        /**
         * Sends the tab to all send-tab compatible devices.
         *
         * @param tab The tab to send.
         * @return a deferred boolean as true if the combined result was successful or not.
         */
        operator fun invoke(tab: TabData): Deferred<Boolean> {
            return scope.async {
                sendToAll { devices ->
                    devices.map { device ->
                        device to tab
                    }
                }
            }
        }

        /**
         * Sends the tabs to all the send-tab compatible devices.
         *
         * @param tabs a collection of tabs to send.
         * @return a deferred boolean as true if the combined result was successful or not.
         */
        operator fun invoke(tabs: Collection<TabData>): Deferred<Boolean> {
            return scope.async {
                sendToAll { devices ->
                    val filteredTabs = tabs.filter { isValidTabSchema(it) }
                    devices.crossProduct(filteredTabs) { device, tab ->
                        device to tab
                    }
                }
            }
        }

        private suspend inline fun sendToAll(
            block: (Collection<Device>) -> List<Pair<Device, TabData>>,
        ): Boolean {
            // Filter devices to send tab capable ones.
            filterSendTabDevices(accountManager) { constellation, devices ->
                // Get a list of device-tab combinations that we want to send.
                return block(devices).map { (device, tab) ->
                    // Filter tabs that don't have a send-capable uri
                    if (!isValidTabSchema(tab)) {
                        return false
                    }
                    // Send the tab!
                    constellation.sendCommandToDevice(
                        device.id,
                        SendTab(tab.title, tab.url),
                    )
                }.fold(true) { acc, result ->
                    // Collect the results and reduce them into one final result.
                    acc and result
                }
            }
            return false
        }
    }

    val sendToDeviceAsync: SendToDeviceUseCase by lazy {
        SendToDeviceUseCase(
            accountManager,
            scope,
        )
    }

    val sendToAllAsync: SendToAllUseCase by lazy {
        SendToAllUseCase(
            accountManager,
            scope,
        )
    }
}

@VisibleForTesting
internal inline fun filterSendTabDevices(
    accountManager: FxaAccountManager,
    block: (DeviceConstellation, Collection<Device>) -> Unit,
) {
    val constellation = accountManager.authenticatedAccount()?.deviceConstellation() ?: return

    constellation.state()?.let { state ->
        state.otherDevices.filter {
            it.capabilities.contains(DeviceCapability.SEND_TAB)
        }.let { devices ->
            block(constellation, devices)
        }
    }
}

@VisibleForTesting
internal fun isValidTabSchema(tab: TabData): Boolean {
    // We don't sync certain schemas, about|resource|chrome|file|blob|moz-extension
    // See https://searchfox.org/mozilla-central/rev/7d379061bd56251df911728686c378c5820513d8/modules/libpref/init/all.js#4356
    val filteredSchemas = arrayOf("about:", "resource:", "chrome:", "file:", "blob:", "moz-extension:")
    return filteredSchemas.none({ tab.url.startsWith(it) })
}
