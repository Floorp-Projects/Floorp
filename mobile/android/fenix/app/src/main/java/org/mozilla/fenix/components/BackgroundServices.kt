/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.os.Build
import androidx.lifecycle.ProcessLifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.DeviceType
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.GlobalSyncableStoreProvider
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

    private val serverConfig = ServerConfig.release(CLIENT_ID, REDIRECT_URL)
    private val deviceConfig = DeviceConfig(
        name = Build.MANUFACTURER + " " + Build.MODEL,
        type = DeviceType.MOBILE,

        // NB: flipping this flag back and worth is currently not well supported and may need hand-holding.
        // Consult with the android-components peers before changing.
        // See https://github.com/mozilla/application-services/issues/1308
        capabilities = if (BuildConfig.SEND_TAB_ENABLED) {
            setOf(DeviceCapability.SEND_TAB)
        } else {
            emptySet()
        }
    )
    // If sync has been turned off on the server then disable syncing.
    private val syncConfig = if (context.isInExperiment(Experiments.asFeatureSyncDisabled)) {
        null
    } else {
        SyncConfig(setOf("history", "bookmarks"), syncPeriodInMinutes = 240L) // four hours
    }

    init {
        // Make the "history" and "bookmark" stores accessible to workers spawned by the sync manager.
        GlobalSyncableStoreProvider.configureStore("history" to historyStorage)
        GlobalSyncableStoreProvider.configureStore("bookmarks" to bookmarkStorage)
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

    val accountManager = FxaAccountManager(
        context,
        serverConfig,
        deviceConfig,
        syncConfig,
        // We don't need to specify this explicitly, but `syncConfig` may be disabled due to an 'experiments'
        // flag. In that case, sync scope necessary for syncing won't be acquired during authentication
        // unless we explicitly specify it below.
        // This is a good example of an information leak at the API level.
        // See https://github.com/mozilla-mobile/android-components/issues/3732
        setOf("https://identity.mozilla.com/apps/oldsync")
    ).also {
        it.registerForDeviceEvents(deviceEventObserver, ProcessLifecycleOwner.get(), true)
        CoroutineScope(Dispatchers.Main).launch { it.initAsync().await() }
    }
}
