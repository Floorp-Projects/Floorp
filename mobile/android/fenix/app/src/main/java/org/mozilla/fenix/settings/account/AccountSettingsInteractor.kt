/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    fun onChangeDeviceName(newDeviceName: String): Boolean

    /**
     * Called whenever the "Sign out" button is tapped
     */
    fun onSignOut()
}

class AccountSettingsInteractor(
    private val navController: NavController,
    private val syncNow: () -> Unit,
    private val checkValidName: (String) -> Boolean,
    private val setDeviceName: (String) -> Unit,
    private val store: AccountSettingsStore
) : AccountSettingsUserActions {

    override fun onSyncNow() {
        syncNow.invoke()
    }

    override fun onChangeDeviceName(newDeviceName: String): Boolean {
        val isValidName = checkValidName.invoke(newDeviceName)
        if (!isValidName) {
            return false
        }
        // Optimistically set the device name to what user requested.
        store.dispatch(AccountSettingsAction.UpdateDeviceName(newDeviceName))

        setDeviceName.invoke(newDeviceName)
        return true
    }

    override fun onSignOut() {
        val directions = AccountSettingsFragmentDirections.actionAccountSettingsFragmentToSignOutFragment()
        navController.nav(
            R.id.accountSettingsFragment,
            directions
        )
    }
}
