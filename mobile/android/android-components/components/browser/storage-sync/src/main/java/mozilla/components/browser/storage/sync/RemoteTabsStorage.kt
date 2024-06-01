/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.withContext
import mozilla.appservices.remotetabs.RemoteCommand
import mozilla.appservices.remotetabs.RemoteTab
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.storage.Storage
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCommandOutgoing
import mozilla.components.concept.sync.DeviceCommandQueue
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.utils.logElapsedTime
import java.io.File
import mozilla.appservices.remotetabs.TabsApiException as RemoteTabProviderException
import mozilla.appservices.remotetabs.TabsStore as RemoteTabsProvider

private const val TABS_DB_NAME = "tabs.sqlite"

/**
 * An interface which defines read/write methods for remote tabs data.
 */
open class RemoteTabsStorage(
    private val context: Context,
    private val crashReporter: CrashReporting? = null,
) : Storage, SyncableStore {
    internal val api by lazy { RemoteTabsProvider(File(context.filesDir, TABS_DB_NAME).canonicalPath) }
    private val scope by lazy { CoroutineScope(Dispatchers.IO) }
    internal val logger = Logger("RemoteTabsStorage")

    override suspend fun warmUp() {
        logElapsedTime(logger, "Warming up storage") { api }
    }

    /**
     * Store the locally opened tabs.
     * @param tabs The list of opened tabs, for all opened non-private windows, on this device.
     */
    suspend fun store(tabs: List<Tab>) {
        return withContext(scope.coroutineContext) {
            try {
                api.setLocalTabs(
                    tabs.map {
                        val activeTab = it.active()
                        val urlHistory = listOf(activeTab.url) + it.previous().reversed().map { it.url }
                        RemoteTab(activeTab.title, urlHistory, activeTab.iconUrl, it.lastUsed, it.inactive)
                    },
                )
            } catch (e: RemoteTabProviderException) {
                crashReporter?.submitCaughtException(e)
            }
        }
    }

    /**
     * Get all remote devices tabs.
     * @return A mapping of opened tabs per device.
     */
    suspend fun getAll(): Map<SyncClient, List<Tab>> {
        return withContext(scope.coroutineContext) {
            try {
                api.getAll().map { device ->
                    val tabs = device.remoteTabs.map { tab ->
                        // Map RemoteTab to TabEntry
                        val title = tab.title
                        val icon = tab.icon
                        val lastUsed = tab.lastUsed
                        val history = tab.urlHistory.reversed().map { url -> TabEntry(title, url, icon) }
                        Tab(history, tab.urlHistory.lastIndex, lastUsed, tab.inactive)
                    }
                    // Map device to tabs
                    SyncClient(device.clientId) to tabs
                }.toMap()
            } catch (e: RemoteTabProviderException) {
                crashReporter?.submitCaughtException(e)
                return@withContext emptyMap()
            }
        }
    }

    override suspend fun runMaintenance(dbSizeLimit: UInt) {
        // Storage maintenance workflow for remote tabs is not implemented yet.
    }

    override fun cleanup() {
        scope.coroutineContext.cancelChildren()
    }

    override fun registerWithSyncManager() {
        return api.registerWithSyncManager()
    }
}

/**
 * A command queue for managing synced tabs on other devices.
 *
 * @property storage Persistent storage for the queued commands.
 * @property closeTabsCommandSender A function that sends a queued
 * "close tabs" command.
 */
class RemoteTabsCommandQueue(
    internal val storage: RemoteTabsStorage,
    internal val closeTabsCommandSender: CommandSender<DeviceCommandOutgoing.CloseTab>,
) : DeviceCommandQueue<DeviceCommandQueue.Type.RemoteTabs>,
    Observable<DeviceCommandQueue.Observer> by ObserverRegistry() {

    // This queue is backed by `appservices.remotetabs.RemoteCommandStore`,
    // but the actual commands are eventually sent via
    // `appservices.fxaclient.FxaClient`.

    // The `appservices.remotetabs` and `appservices.fxaclient` packages use
    // different types to represent the same commands, but we want to
    // smooth over this difference for consumers, so we parameterize
    // the queue over `concept.sync.DeviceCommandQueue.Type.RemoteTabs`, and
    // map those to the `appservices.remotetab` types below.

    internal val api by lazy { storage.api.newRemoteCommandStore() }
    private val scope by lazy { CoroutineScope(Dispatchers.IO) }

    override suspend fun add(deviceId: String, command: DeviceCommandQueue.Type.RemoteTabs) =
        withContext(scope.coroutineContext) {
            when (command) {
                is DeviceCommandOutgoing.CloseTab -> {
                    command.urls.forEach {
                        api.addRemoteCommand(deviceId, RemoteCommand.CloseTab(it))
                    }
                    notifyObservers { onAdded() }
                }
            }
        }

    override suspend fun remove(deviceId: String, command: DeviceCommandQueue.Type.RemoteTabs) =
        withContext(scope.coroutineContext) {
            when (command) {
                is DeviceCommandOutgoing.CloseTab -> {
                    command.urls.forEach {
                        api.removeRemoteCommand(deviceId, RemoteCommand.CloseTab(it))
                    }
                    notifyObservers { onRemoved() }
                }
            }
        }

    override suspend fun flush(): DeviceCommandQueue.FlushResult = withContext(scope.coroutineContext) {
        api.getUnsentCommands()
            .mapNotNull { pendingCommand ->
                when (val providerCommand = pendingCommand.command) {
                    is RemoteCommand.CloseTab -> async {
                        val command =
                            DeviceCommandOutgoing.CloseTab(listOf(providerCommand.url))
                        when (closeTabsCommandSender.send(pendingCommand.deviceId, command)) {
                            is SendResult.Ok -> {
                                api.setPendingCommandSent(pendingCommand)
                                DeviceCommandQueue.FlushResult.ok()
                            }
                            // If the user isn't signed in, or the
                            // target device isn't there, retrying without
                            // user intervention won't help. Keep the pending
                            // command in the queue, but return `Ok` so that
                            // the flush isn't rescheduled.
                            is SendResult.NoAccount -> DeviceCommandQueue.FlushResult.ok()
                            is SendResult.NoDevice -> DeviceCommandQueue.FlushResult.ok()
                            is SendResult.Error -> DeviceCommandQueue.FlushResult.retry()
                        }
                    }
                    else -> null
                }
            }
            .awaitAll()
            .fold(DeviceCommandQueue.FlushResult.ok(), DeviceCommandQueue.FlushResult::and)
    }

    /** Sends a queued command of type [T] to another device. */
    fun interface CommandSender<T> {
        /**
         * Sends the command.
         *
         * @param deviceId The target device ID.
         * @param command The command to send.
         * @return The result of sending the command to the target device.
         */
        suspend fun send(deviceId: String, command: T): SendResult
    }

    /** The result of sending a queued command. */
    sealed interface SendResult {
        /** The queued command was successfully sent. */
        data object Ok : SendResult

        /**
         * The queued command couldn't be sent because the user
         * isn't authenticated.
         */
        data object NoAccount : SendResult

        /**
         * The queued command couldn't be sent because the target device is
         * unavailable, or doesn't support the command.
         */
        data object NoDevice : SendResult

        /** The queued command couldn't be sent for any other reason. */
        data object Error : SendResult
    }
}

/**
 * Represents a Sync client that can be associated with a list of opened tabs.
 */
data class SyncClient(val id: String)

/**
 * A tab, which is defined by an history (think the previous/next button in your web browser) and
 * a currently active history entry.
 */
data class Tab(
    val history: List<TabEntry>,
    val active: Int,
    val lastUsed: Long,
    val inactive: Boolean,
) {
    /**
     * The current active tab entry. In other words, this is the page that's currently shown for a
     * tab.
     */
    fun active(): TabEntry {
        return history[active]
    }

    /**
     * The list of tabs history entries that come before this tab. In other words, the "previous"
     * navigation button history list.
     */
    fun previous(): List<TabEntry> {
        return history.subList(0, active)
    }

    /**
     * The list of tabs history entries that come after this tab. In other words, the "next"
     * navigation button history list.
     */
    fun next(): List<TabEntry> {
        return history.subList(active + 1, history.lastIndex + 1)
    }
}

/**
 * A synced device and its list of tabs.
 */
data class SyncedDeviceTabs(
    val device: Device,
    val tabs: List<Tab>,
)

/**
 * A Tab history entry.
 */
data class TabEntry(
    val title: String,
    val url: String,
    val iconUrl: String?,
)
