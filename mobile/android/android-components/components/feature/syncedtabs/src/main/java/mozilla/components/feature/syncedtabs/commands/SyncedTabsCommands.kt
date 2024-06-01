/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.commands

import mozilla.components.browser.storage.sync.RemoteTabsCommandQueue
import mozilla.components.browser.storage.sync.RemoteTabsStorage
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceCommandOutgoing
import mozilla.components.concept.sync.DeviceCommandQueue
import mozilla.components.service.fxa.manager.FxaAccountManager

/**
 * A queue that connects a [RemoteTabsCommandQueue] to an [FxaAccountManager]
 * for sending synced tabs device commands.
 *
 * @param accountManager The account manager.
 * @param tabsStorage Persistent storage for the queued commands.
 */
class SyncedTabsCommands(
    accountManager: FxaAccountManager,
    tabsStorage: RemoteTabsStorage,
) : DeviceCommandQueue<DeviceCommandQueue.Type.RemoteTabs> by RemoteTabsCommandQueue(
    storage = tabsStorage,
    closeTabsCommandSender = CloseTabsCommandSender(accountManager),
)

internal class CloseTabsCommandSender(
    val accountManager: FxaAccountManager,
) : RemoteTabsCommandQueue.CommandSender<DeviceCommandOutgoing.CloseTab> {
    override suspend fun send(
        deviceId: String,
        command: DeviceCommandOutgoing.CloseTab,
    ): RemoteTabsCommandQueue.SendResult {
        val constellation = accountManager
            .authenticatedAccount()
            ?.deviceConstellation()
            ?: return RemoteTabsCommandQueue.SendResult.NoAccount

        val targetDevice = constellation.state()?.let { state ->
            state.otherDevices.firstOrNull {
                it.id == deviceId && it.capabilities.contains(DeviceCapability.CLOSE_TABS)
            }
        } ?: return RemoteTabsCommandQueue.SendResult.NoDevice

        return if (constellation.sendCommandToDevice(targetDevice.id, command)) {
            RemoteTabsCommandQueue.SendResult.Ok
        } else {
            RemoteTabsCommandQueue.SendResult.Error
        }
    }
}
