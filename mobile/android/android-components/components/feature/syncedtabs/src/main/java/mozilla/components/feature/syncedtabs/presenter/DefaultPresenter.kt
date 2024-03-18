/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.presenter

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.syncedtabs.controller.SyncedTabsController
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.feature.syncedtabs.view.SyncedTabsView.ErrorType
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import mozilla.components.service.fxa.sync.SyncStatusObserver

/**
 * The tricky part in this class is to handle all possible Sync+FxA states:
 *
 * - No Sync account
 * - Connected to FxA but not Sync (impossible state on mobile at the moment).
 * - Connected to Sync, but needs reconnection.
 * - Connected to Sync, but tabs syncing disabled.
 * - Connected to Sync, but tabs haven't been synced yet (they stay in memory after the first sync).
 * - Connected to Sync, but only one device in the account (us), so no other tab to show.
 * - Connected to Sync.
 *
 */
internal class DefaultPresenter(
    private val context: Context,
    override val controller: SyncedTabsController,
    override val accountManager: FxaAccountManager,
    override val view: SyncedTabsView,
    private val lifecycleOwner: LifecycleOwner,
) : SyncedTabsPresenter {

    @VisibleForTesting
    internal val eventObserver = SyncedTabsSyncObserver(context, view, controller)

    @VisibleForTesting
    internal val accountObserver = SyncedTabsAccountObserver(view, controller)

    override fun start() {
        accountManager.registerForSyncEvents(
            observer = eventObserver,
            owner = lifecycleOwner,
            autoPause = true,
        )

        accountManager.register(
            observer = accountObserver,
            owner = lifecycleOwner,
            autoPause = true,
        )

        // No authenticated account present at all.
        if (accountManager.authenticatedAccount() == null) {
            view.onError(ErrorType.SYNC_UNAVAILABLE)
            return
        }

        // Have an account, but it ran into auth issues.
        if (accountManager.accountNeedsReauth()) {
            view.onError(ErrorType.SYNC_NEEDS_REAUTHENTICATION)
            return
        }

        // Synced tabs not enabled.
        if (!isSyncedTabsEngineEnabled(context)) {
            view.onError(ErrorType.SYNC_ENGINE_UNAVAILABLE)
            return
        }

        controller.syncAccount()
    }

    override fun stop() {
        accountManager.unregisterForSyncEvents(eventObserver)
        accountManager.unregister(accountObserver)
    }

    companion object {
        // This status isn't always set before it's inspected. This causes erroneous reports of the
        // sync engine being unavailable. Tabs are included in sync by default, so it's safe to
        // default to true until they are deliberately disabled.
        private fun isSyncedTabsEngineEnabled(context: Context): Boolean {
            return SyncEnginesStorage(context).getStatus()[SyncEngine.Tabs] ?: true
        }
    }

    internal class SyncedTabsAccountObserver(
        private val view: SyncedTabsView,
        private val controller: SyncedTabsController,
    ) : AccountObserver {

        override fun onLoggedOut() {
            CoroutineScope(Dispatchers.Main).launch { view.onError(ErrorType.SYNC_UNAVAILABLE) }
        }

        override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
            CoroutineScope(Dispatchers.Main).launch {
                controller.syncAccount()
            }
        }

        override fun onAuthenticationProblems() {
            CoroutineScope(Dispatchers.Main).launch {
                view.onError(ErrorType.SYNC_NEEDS_REAUTHENTICATION)
            }
        }
    }

    internal class SyncedTabsSyncObserver(
        private val context: Context,
        private val view: SyncedTabsView,
        private val controller: SyncedTabsController,
    ) : SyncStatusObserver {

        override fun onIdle() {
            if (isSyncedTabsEngineEnabled(context)) {
                controller.refreshSyncedTabs()
            } else {
                view.onError(ErrorType.SYNC_ENGINE_UNAVAILABLE)
            }
        }

        override fun onError(error: Exception?) {
            view.onError(ErrorType.SYNC_ENGINE_UNAVAILABLE)
        }

        override fun onStarted() {
            view.startLoading()
        }
    }
}
