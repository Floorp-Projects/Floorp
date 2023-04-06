/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import androidx.navigation.NavController
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.nav

interface AccountSettingsUserActions {

    /**
     * Called whenever the "Sync now" button is tapped
     */
    fun onSyncNow()

    /**
     * Called whenever user sets a new device name
     * @param newDeviceName the device name to change to
     * @return Boolean indicating whether the new device name has been accepted or not
     */
    fun onChangeDeviceName(newDeviceName: String, invalidNameResponse: () -> Unit): Boolean

    /**
     * Called whenever the "Sign out" button is tapped
     */
    fun onSignOut()
}

class AccountSettingsInteractor(
    private val navController: NavController,
    private val syncNow: () -> Unit,
    private val syncDeviceName: (String) -> Boolean,
    private val store: AccountSettingsFragmentStore,
) : AccountSettingsUserActions {

    override fun onSyncNow() {
        syncNow.invoke()
    }

    override fun onChangeDeviceName(newDeviceName: String, invalidNameResponse: () -> Unit): Boolean {
        if (!syncDeviceName(newDeviceName)) {
            invalidNameResponse.invoke()
            return false
        }
        // Our "change the device name on the server" operation may fail.
        // Currently, we disregard this failure and pretend we succeeded.
        // At the same time, when user changes the device name,
        // we immediately update the UI to display the new name.
        // So, in case of a network (or other) failure when talking to the server,
        // we'll have a discrepancy - the UI will reflect new value, but actually the value never changed.
        // So, when user presses "sync now", we'll fetch the old value, and reset the UI.
        store.dispatch(AccountSettingsFragmentAction.UpdateDeviceName(newDeviceName))

        return true
    }

    override fun onSignOut() {
        val directions = AccountSettingsFragmentDirections.actionAccountSettingsFragmentToSignOutFragment()
        navController.nav(
            R.id.accountSettingsFragment,
            directions,
        )
    }
}
