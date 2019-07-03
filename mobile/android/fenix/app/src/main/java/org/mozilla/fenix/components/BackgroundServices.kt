/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.os.Build
import androidx.lifecycle.ProcessLifecycleOwner
import androidx.work.WorkManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.sync.BackgroundSyncManager
import mozilla.components.feature.sync.GlobalSyncableStoreProvider
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.manager.DeviceTuple
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Experiments
import org.mozilla.fenix.isInExperiment
import org.mozilla.fenix.test.Mockable

/**
 * Component group for background services. These are the components that need to be accessed from within a
 * background worker.
 */
@Mockable
class BackgroundServices(
    context: Context,
    historyStorage: PlacesHistoryStorage,
    bookmarkStorage: PlacesBookmarksStorage,
    notificationManager: NotificationManager
) {
    companion object {
        const val CLIENT_ID = "a2270f727f45f648"
        const val REDIRECT_URL = "https://accounts.firefox.com/oauth/success/$CLIENT_ID"
    }

    // This is slightly messy - here we need to know the union of all "scopes"
    // needed by components which rely on FxA integration. If this list
    // grows too far we probably want to find a way to determine the set
    // at runtime.
    private val scopes: Array<String> = arrayOf("profile", "https://identity.mozilla.com/apps/oldsync")
    private val config = Config.release(CLIENT_ID, REDIRECT_URL)

    init {
        // Make the "history" and "bookmark" stores accessible to workers spawned by the sync manager.
        GlobalSyncableStoreProvider.configureStore("history" to historyStorage)
        GlobalSyncableStoreProvider.configureStore("bookmarks" to bookmarkStorage)
    }

    // if sync has been turned off on the server then make `syncManager` null
    val syncManager = if (context.isInExperiment(Experiments.asFeatureSyncDisabled)) {
        WorkManager.getInstance().cancelUniqueWork("Periodic")
        null
    } else {
        BackgroundSyncManager("https://identity.mozilla.com/apps/oldsync").also {
            it.addStore("history")
            it.addStore("bookmarks")
        }
    }

    private val deviceEventObserver = object : DeviceEventsObserver {
        private val logger = Logger("DeviceEventsObserver")
        override fun onEvents(events: List<DeviceEvent>) {
            logger.info("Received ${events.size} device event(s)")
            events.filter { it is DeviceEvent.TabReceived }.forEach {
                notificationManager.showReceivedTabs(it as DeviceEvent.TabReceived)
            }
        }
    }

    // NB: flipping this flag back and worth is currently not well supported and may need hand-holding.
    // Consult with the android-components peers before changing.
    // See https://github.com/mozilla/application-services/issues/1308
    private val deviceCapabilities = if (BuildConfig.SEND_TAB_ENABLED) {
        listOf(DeviceCapability.SEND_TAB)
    } else {
        emptyList()
    }

    private val defaultDeviceName = Build.MANUFACTURER + " " + Build.MODEL

    val accountManager = FxaAccountManager(
        context,
        config,
        scopes,
        DeviceTuple(defaultDeviceName, DeviceType.MOBILE, deviceCapabilities),
        syncManager
    ).also {
        it.registerForDeviceEvents(deviceEventObserver, ProcessLifecycleOwner.get(), true)
        CoroutineScope(Dispatchers.Main).launch { it.initAsync().await() }
    }
}
